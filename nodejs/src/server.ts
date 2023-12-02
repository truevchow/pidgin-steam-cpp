import { ConnectRouter } from "@connectrpc/connect";
import { AuthService } from './protobufs/comm_protobufs/auth_connect'
import { AuthState, AuthResponse } from './protobufs/comm_protobufs/auth_pb'
import { fastify } from "fastify";
import { fastifyConnectPlugin } from "@connectrpc/connect-fastify";

import SteamUser from 'steam-user';
// import SteamChatRoomClient from 'steam-user/components/chatroom';
// import {EAccountType, EPersonaState, SteamID} from 'steamid';
import SteamID from 'steamid';

interface SteamClientUser {
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

class SessionWrapper {
    client: SteamUser;
    steamGuardCallback: undefined | ((code: string) => void);
    resolve: undefined | ((value: AuthResponse) => void);

    expectRefreshToken: boolean;
    refreshToken: undefined | string;
    loggedOnDetails: undefined | any;

    constructor(client: SteamUser, expectRefreshToken: boolean) {
        this.client = client;
        this.expectRefreshToken = expectRefreshToken;
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
                    reasonStr: AuthState.STEAM_GUARD_CODE_REQUEST.toString(),
                    reason: AuthState.STEAM_GUARD_CODE_REQUEST,
                    sessionKey: sessionKey,
                }));
            }

            function fail(reason: AuthState = AuthState.INVALID_CREDENTIALS) {
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
                    reasonStr: AuthState.SUCCESS.toString(),
                    reason: AuthState.SUCCESS,
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

async function main() {
    const server = fastify();
    const endpoints: string[] = [];

    await server.register(fastifyConnectPlugin, {
        routes(router) {
            authRoute(router);
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