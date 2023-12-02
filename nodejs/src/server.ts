import { ConnectRouter } from "@connectrpc/connect";
import { AuthService } from './protobufs/comm_protobufs/auth_connect'
import { AuthResponse, AuthResponse_AuthState } from './protobufs/comm_protobufs/auth_pb'
import { MessageService } from './protobufs/comm_protobufs/message_connect'
import { MessageRequest, SendMessageResult, SendMessageResult_SendMessageResultCode, ResponseMessage, PollRequest, FriendsListRequest, FriendsListResponse, Persona, PersonaState } from './protobufs/comm_protobufs/message_pb'
import { fastify } from "fastify";
import { fastifyConnectPlugin } from "@connectrpc/connect-fastify";
import { once } from "events";

import SteamUser from 'steam-user';
// import SteamChatRoomClient from 'steam-user/components/chatroom';
// import {EAccountType, EPersonaState } from 'steamid';
import SteamID from 'steamid';
import { Timestamp } from "@bufbuild/protobuf";

interface SteamClientUserUpdate {  // emitted in `user` event
    rich_presence: any[];
    persona_state: SteamUser.EPersonaState;
    game_played_app_id: number;
    persona_state_flags: SteamUser.EPersonaStateFlag;
    online_session_instances: number;
    player_name: string;
    steamid_source: string;
    avatar_hash: Buffer;
    last_logoff: Date;
    last_logon: Date;
    last_seen_online: Date;
    game_name: string;
    gameid: string;
    game_data_blob: Buffer;
    game_lobby_id: string;
    player_name_pending_review: boolean;
    avatar_pending_review: boolean;
    avatar_url_icon: string;
    avatar_url_medium: string;
    avatar_url_full: string;
}

interface SteamClientUserState {  // stored in `SteamUser.users`
    rich_presence: any[];
    player_name: string;
    avatar_hash: {
        type: string;
        data: number[];
    };
    last_logoff: string;
    last_logon: string;
    last_seen_online: string;
    avatar_url_icon: string;
    avatar_url_medium: string;
    avatar_url_full: string;
};

interface SteamClientUser extends SteamClientUserState {
    persona_state: SteamUser.EPersonaState;
};

class SessionWrapper {
    client: SteamUser;
    steamGuardCallback: undefined | ((code: string) => void);
    resolve: undefined | ((value: AuthResponse) => void);

    expectRefreshToken: boolean;
    refreshToken: undefined | string;
    loggedOnDetails: undefined | any;

    friendsLoaded: boolean;
    users: Record<string, SteamClientUser> = {};  // needed since client.users doesn't contain all fields even after `user`` event

    constructor(client: SteamUser, expectRefreshToken: boolean) {
        this.client = client;
        this.expectRefreshToken = expectRefreshToken;
        this.friendsLoaded = false;
    }

    getUser(target: string | SteamID): SteamClientUser | undefined {
        if (target instanceof SteamID) {
            target = target.getSteamID64();
        }
        // return this.client.users[target];
        return this.users[target];
    }
}

let activeSessions: Map<string, SessionWrapper> = new Map();

function authRoute(router: ConnectRouter) {
    router.service(AuthService, {
        async authenticate(call) {
            // console.log("Received", call.toJson());
            let sessionKey: string;
            let wrapper: SessionWrapper;
            let isNew: boolean;

            function requestSteamGuardCode() {
                let resolve = wrapper.resolve!;
                // session.submitSteamGuardCode(steamGuardMachineToken);
                resolve(new AuthResponse({
                    success: true,
                    reasonStr: AuthResponse_AuthState.STEAM_GUARD_CODE_REQUEST.toString(),
                    reason: AuthResponse_AuthState.STEAM_GUARD_CODE_REQUEST,
                    sessionKey: sessionKey,
                }));
            }

            function fail(reason: AuthResponse_AuthState = AuthResponse_AuthState.INVALID_CREDENTIALS) {
                let resolve = wrapper.resolve!;
                resolve(new AuthResponse({
                    success: false,
                    reasonStr: reason.toString(),
                    reason: reason,
                }));
            }

            function succeed(refreshToken: string | undefined) {
                let resolve = wrapper.resolve!;
                resolve(new AuthResponse({
                    success: true,
                    reasonStr: AuthResponse_AuthState.SUCCESS.toString(),
                    reason: AuthResponse_AuthState.SUCCESS,
                    sessionKey: sessionKey,
                    refreshToken: refreshToken,
                }));
            }

            // Print all sessions
            console.log("Active clients:");
            activeSessions.forEach((wrapper, sessionKey) => {
                console.log(sessionKey, wrapper.client.accountInfo);
            });

            if (call.sessionKey && activeSessions.has(call.sessionKey!)) {
                isNew = false;
                console.log("Use existing client")
                sessionKey = call.sessionKey!;
                wrapper = activeSessions.get(call.sessionKey!)!;
            } else {
                isNew = true;
                console.log("Create new client")
                sessionKey = crypto.randomUUID();
                wrapper = new SessionWrapper(new SteamUser(), !call.refreshToken);
                activeSessions.set(sessionKey, wrapper);

                let client = wrapper.client;

                client.on('steamGuard', function (domain, callback) {
                    console.log("Steam Guard callback");
                    wrapper.steamGuardCallback = callback;
                    requestSteamGuardCode();
                });

                function succeedOnLogin() {
                    if (!wrapper.resolve) {
                        return;
                    }
                    if (wrapper.loggedOnDetails && (!wrapper.expectRefreshToken || wrapper.refreshToken)) {
                        succeed(wrapper.refreshToken);
                    }
                }

                client.on('loggedOn', function (details) {
                    console.log("client: Logged into Steam as " + client.steamID?.getSteam3RenderedID());
                    client.setPersona(SteamUser.EPersonaState.Online);  // needed to update {@link client.users}
                    wrapper.loggedOnDetails = details;
                    succeedOnLogin();
                });

                // NOTE: manually patch steam-user/index.d.ts to add these events:
                /*
                    refreshToken: [token: string];
                    friendPersonasLoaded: [];
                */
                client.on('refreshToken', function (refreshToken: string) {
                    console.log("client: Refresh token");
                    wrapper.refreshToken = refreshToken;
                    succeedOnLogin();
                });

                Promise.all([
                    once(client, 'friendPersonasLoaded').then(() => {
                        console.log("client: Friend personas loaded")
                        return 'friendPersonasLoaded';
                    }),
                    once(client, 'friendsList').then(() => {
                        console.log("client: Friend list loaded")
                        return 'friendsList';
                    }),
                ]).then(() => {
                    wrapper.friendsLoaded = true;
                });

                client.on('user', function (steamId, user) {
                    let update = user as SteamClientUserUpdate;
                    // console.log("client: User", steamId, client.users[steamId.getSteamID64()], user);
                    if (update.persona_state === undefined || update.persona_state === null) {
                        // console.log("client: delete persona_state", steamId)
                        delete (update as Partial<SteamClientUserUpdate>).persona_state;
                    }

                    const steamId64 = steamId.getSteamID64();
                    wrapper.users[steamId64] = {
                        ...(wrapper.users[steamId64] || {}),
                        ...(update as Partial<SteamClientUserUpdate>),
                    } as SteamClientUser;

                    // console.log("client: update user", steamId64, wrapper.users[steamId64]);
                    let newUser = wrapper.getUser(steamId64);
                    console.log("client: update user", steamId64, newUser?.player_name, newUser?.persona_state);
                });

                client.on('error', function (err) {
                    // This should ordinarily not happen. This only happens in case there's some kind of unexpected error while
                    // polling, e.g. the network connection goes down or Steam chokes on something.
                    console.log(`ERROR: This login attempt has failed! ${err.message}`);
                    fail();
                    // client.logOff();
                });

                // TODO: allow multiple Steam Guard auth attempts
                // TODO: maybe allow multiple login attempts?
                console.log('client: Logging on to Steam');
                if (call.refreshToken) {
                    console.log("Using provided refresh token")
                    client.logOn({
                        refreshToken: call.refreshToken,
                    });
                } else {
                    console.log("Using provided username and password")
                    client.logOn({
                        accountName: call.username,
                        password: call.password,
                    });
                }
            }

            console.log("Session key", sessionKey);
            return new Promise<AuthResponse>(async (resolve) => {
                wrapper.resolve = resolve;
                // https://github.com/DoctorMcKay/node-steam-session/blob/master/examples/login-with-password.ts
                if (isNew) {
                    return;
                }

                if (call.steamGuardCode) {
                    console.log("Using provided Steam Guard code:", call.steamGuardCode)
                    try {
                        let callback = wrapper.steamGuardCallback!;
                        wrapper.steamGuardCallback = undefined;
                        callback!(call.steamGuardCode!);
                    } catch (ex: any) {
                        console.log("Invalid Steam Guard code", typeof ex);
                        console.error(ex);
                        fail();
                        // client.logOff();
                    }
                }
            });
        }
    });
}

function messageRoute(router: ConnectRouter) {
    router.service(MessageService, {
        async sendChatMessage(call: MessageRequest): Promise<SendMessageResult> {
            console.log("Received", call.getType().typeName, call.toJson());
            let sessionKey = call.sessionKey!;
            let wrapper = activeSessions.get(sessionKey);
            if (!wrapper) {
                return new SendMessageResult({
                    success: false,
                    reason: SendMessageResult_SendMessageResultCode.INVALID_SESSION_KEY,
                    reasonStr: "Invalid session key",
                });
            }
            let client = wrapper.client;
            let steamID = new SteamID(call.targetId!);
            let message = call.message!;

            try {
                await client.chat.sendFriendMessage(steamID, message);
            } catch (ex: any) {
                console.log("Error while sending message", typeof ex);
                console.error(ex);
                return new SendMessageResult({
                    success: false,
                    reason: SendMessageResult_SendMessageResultCode.UNKNOWN_ERROR,
                    reasonStr: ex.message,
                });
            }

            return new SendMessageResult({
                success: true,
                reason: SendMessageResult_SendMessageResultCode.SUCCESS,
                reasonStr: "Success",
            });
        },
        async *pollChatMessages(call: PollRequest) {
            console.log("Received", call.getType().typeName, call.toJson());
            let sessionKey = call.sessionKey!;
            let wrapper = activeSessions.get(sessionKey);
            if (!wrapper) {
                return;
            }
            let client = wrapper.client;
            let steamID = new SteamID(call.targetId!);

            let { messages, more_available } = await client.chat.getFriendMessageHistory(steamID, {
                startTime: Date.now() - 1000 * 60 * 60 * 24 * 1, // 1 day ago
            });

            async function* generateMessages() {
                for await (let message of messages) {
                    console.log("Message", message);
                    yield new ResponseMessage({
                        senderId: message.sender.getSteamID64(),
                        message: message.message,
                        timestamp: Timestamp.fromDate(message.server_timestamp),
                    });
                }
            }

            return generateMessages();
        },
        async getFriendsList(call: FriendsListRequest) {
            console.log("Received", call.getType().typeName, call.toJson());
            let sessionKey = call.sessionKey!;
            let wrapper = activeSessions.get(sessionKey);
            if (!wrapper) {
                throw new Error("Invalid session key");
            }

            async function checkFriendsLoaded(startTime, timeout) {
                while (!wrapper!.friendsLoaded) {
                    if (Date.now() - startTime > timeout) {
                        throw new Error("Timed out waiting for friends list");
                    }
                    await new Promise(resolve => setTimeout(resolve, 100));
                }
            }
            await checkFriendsLoaded(Date.now(), 5000);

            let client = wrapper.client;

            function makePersona(steamId: string, relationship: SteamUser.EFriendRelationship) {
                let friend = wrapper!.getUser(steamId);
                if (!friend) {
                    throw new Error("Invalid steamId");
                }
                // console.log("Friend", steamId, friend);
                var personaState = friend.persona_state;
                if (personaState === undefined || personaState === null) {
                    personaState = SteamUser.EPersonaState.Offline;
                }
                // console.log("state", personaState, SteamUser.EPersonaState.Offline);
                console.log("Friend", steamId, friend.player_name, personaState)
                let isOnline = personaState !== SteamUser.EPersonaState.Offline;
                // console.log("isOnline", isOnline);
                return new Persona({
                    id: steamId,
                    name: friend.player_name,
                    personaState: (personaState as unknown) as PersonaState,
                    // avatarUrl: friend.avatar_url_full,
                    // lastLogoff: Timestamp.fromDate(friend.last_logoff),
                    // lastLogon: Timestamp.fromDate(friend.last_logon),
                    // lastSeenOnline: Timestamp.fromDate(friend.last_seen_online),
                });
            }

            return new FriendsListResponse({
                friends: Object.entries(client.myFriends).map(([steamId, relationship]) => {
                    try {
                        return makePersona(steamId, relationship);
                    } catch (ex) {
                        console.log("Error while getting friend", typeof ex);
                        console.error(ex);
                        return undefined;
                    }
                }).filter((persona) => persona !== undefined) as Persona[],
            });
        }
    });
}

async function main() {
    const server = fastify();
    const endpoints: string[] = [];

    await server.register(fastifyConnectPlugin, {
        routes(router) {
            authRoute(router);
            messageRoute(router);
            // Add each endpoint to the list
            router.handlers.forEach((route) => {
                endpoints.push(route.requestPath);
            });
            return router;
        },
    });

    server.get("/", (_, reply) => {
        reply.type("text/plain");
        reply.send("Hello World!");
    });

    await server.listen({ host: "localhost", port: 8080 });
    console.log("server is listening at", server.addresses());

    // Print the list of endpoints
    console.log("Available endpoints:");
    endpoints.forEach((endpoint) => {
        console.log(endpoint);
    });

    // Cleanup function to terminate all SteamUser clients
    const cleanup = async () => {
        console.log("Cleaning up");
        for (const [sessionKey, sessionWrapper] of activeSessions) {
            try {
                console.log(`Logging off ${sessionKey}`);
                sessionWrapper.client.logOff();
            } catch (ex) {
                console.log(`Error while logging off ${sessionKey}`, ex);
            }
        }
        console.log("Done cleanup");
    };

    // Register cleanup function on program termination
    process.on('beforeExit', async () => {
        await cleanup();
    });
}


(async () => {
    await main();
})();