import { promisify } from 'util';
import net from 'net';
import { Buffer } from 'buffer';
import fs from 'fs';

import { ConnectRouter } from "@connectrpc/connect";
import { AuthService } from './protobufs/comm_protobufs/auth_connect'
import { AuthState, AuthResponse } from './protobufs/comm_protobufs/auth_pb'
import { fastify } from "fastify";
import { fastifyConnectPlugin } from "@connectrpc/connect-fastify";

import SteamUser from 'steam-user';
// import SteamChatRoomClient from 'steam-user/components/chatroom';
// import {EAccountType, EPersonaState, SteamID} from 'steamid';
import SteamID from 'steamid';
import { EAuthSessionGuardType, EAuthTokenPlatformType, LoginSession } from 'steam-session';
import { StartSessionResponse, StartSessionResponseValidAction, } from 'steam-session/dist/interfaces-external';


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

class SteamLoginData {
    accessToken: string;
    refreshToken: string;
    webCookies: string[];

    constructor(accessToken: string, refreshToken: string, webCookies: string[]) {
        this.accessToken = accessToken;
        this.refreshToken = refreshToken;
        this.webCookies = webCookies;
    }
}

class SessionWrapper {
    client: SteamUser;
    session: LoginSession;
    resolve: undefined | ((value: AuthResponse) => void);

    constructor(client: SteamUser, session: LoginSession) {
        this.client = client;
        this.session = session;
    }
}

let activeSessions: Map<string, SessionWrapper> = new Map();

function authRoute(router: ConnectRouter) {
    router.service(AuthService, {
        async authenticate(call) {
            console.log("Received", call.toJson());
            let sessionKey: string;
            let wrapper: SessionWrapper;
            let isNew: boolean;

            // Print all sessions
            console.log("Active sessions:");
            activeSessions.forEach((wrapper, sessionKey) => {
                console.log(sessionKey, wrapper.session.accountName);
            });

            if (call.sessionKey && activeSessions.has(call.sessionKey!)) {
                isNew = false;
                console.log("Use existing session")
                sessionKey = call.sessionKey!;
                wrapper = activeSessions.get(call.sessionKey!)!;

                // return new AuthResponse({
                //     success: true,
                //     reasonStr: AuthState.SUCCESS.toString(),
                //     reason: AuthState.SUCCESS,
                //     sessionKey: sessionKey,
                // });
            } else {
                isNew = true;
                console.log("Create new session")
                sessionKey = crypto.randomUUID();
                wrapper = new SessionWrapper(new SteamUser(), new LoginSession(EAuthTokenPlatformType.WebBrowser));
                activeSessions.set(sessionKey, wrapper);
                
                // return new AuthResponse({
                //     success: true,
                //     reasonStr: AuthState.STEAM_GUARD_CODE_REQUEST.toString(),
                //     reason: AuthState.STEAM_GUARD_CODE_REQUEST,
                //     sessionKey: sessionKey,
                // });

                let session = wrapper.session;

                session.on('authenticated', async () => {
                    console.log(`session: Logged into Steam as ${session.accountName}`);
                    let webCookies = await session.getWebCookies();
                    console.log('Web cookies:', webCookies);

                    if (webCookies) {
                        var steamLoginData: SteamLoginData = {
                            accessToken: session.accessToken,
                            refreshToken: session.refreshToken,
                            webCookies: webCookies
                        };

                        console.log('Steam login data:', steamLoginData);

                        // console.log('Saving login data');
                        // await reader.saveLoginData(steamLoginData);

                        client.on('loggedOn', function (details) {
                            console.log("client: Logged into Steam as " + client.steamID?.getSteam3RenderedID());
                            client.setPersona(SteamUser.EPersonaState.Online);  // needed to update {@link client.users}
                            succeed();
                        });

                        client.on('error', function (e) {
                            // Some error occurred during logon
                            console.log("Error during logon: " + e);
                            fail();
                            // client.logOff();
                        });

                        console.log('client: Logging on to Steam')
                        client.logOn({
                            refreshToken: steamLoginData.refreshToken,
                        });
                    } else {
                        console.log('No web cookies received');
                        fail();
                        client.logOff();
                    }
                });

                session.on('timeout', () => {
                    console.log('This login attempt has timed out.');
                    fail();
                });

                session.on('error', (err) => {
                    // This should ordinarily not happen. This only happens in case there's some kind of unexpected error while
                    // polling, e.g. the network connection goes down or Steam chokes on something.
                    console.log(`ERROR: This login attempt has failed! ${err.message}`);
                    fail();
                });
            }

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

            function succeed() {
                let resolve = wrapper.resolve!;
                resolve(new AuthResponse({
                    success: true,
                    reasonStr: AuthState.SUCCESS.toString(),
                    reason: AuthState.SUCCESS,
                    sessionKey: sessionKey,
                }));
            }

            function handleStartResult(startResult: StartSessionResponse) {
                if (!startResult.actionRequired) {
                    return;
                }
                console.log('Action is required from you to complete this login');

                // We want to process the non-prompting guard types first, since the last thing we want to do is prompt the
                // user for input. It would be needlessly confusing to prompt for input, then print more text to the console.
                let promptingGuardTypes = [EAuthSessionGuardType.EmailCode, EAuthSessionGuardType.DeviceCode];
                let promptingGuards = startResult.validActions!.filter(action => promptingGuardTypes.includes(action.type));
                let nonPromptingGuards = startResult.validActions!.filter(action => !promptingGuardTypes.includes(action.type));

                let printGuard = async ({ type, detail }: StartSessionResponseValidAction) => {
                    try {
                        switch (type) {
                            case EAuthSessionGuardType.EmailCode:
                                console.log(`A login code has been sent to your email address at ${detail}`);
                                requestSteamGuardCode();
                                break;

                            case EAuthSessionGuardType.DeviceCode:
                                console.log('You may confirm this login by providing a Steam Guard Mobile Authenticator code');
                                requestSteamGuardCode();
                                break;

                            case EAuthSessionGuardType.EmailConfirmation:
                                console.log('You may confirm this login by email');
                                break;

                            case EAuthSessionGuardType.DeviceConfirmation:
                                console.log('You may confirm this login by responding to the prompt in your Steam mobile app');
                                break;
                        }
                    } catch (ex: any) {
                        if (ex.eresult == SteamUser.EResult.TwoFactorCodeMismatch) {
                            console.log('Incorrect Steam Guard code');
                            printGuard({ type, detail });
                        } else {
                            throw ex;
                        }
                    }
                    // await printGuard(startResult.validActions![0] as { type: any; detail: any; });
                }

                nonPromptingGuards.forEach(printGuard);
                promptingGuards.forEach(printGuard);
            }

            console.log("Session key", sessionKey);
            let client = wrapper.client;
            let session = wrapper.session;

            return new Promise<AuthResponse>(async (resolve) => {
                wrapper.resolve = resolve;
                // https://github.com/DoctorMcKay/node-steam-session/blob/master/examples/login-with-password.ts
                if (isNew) {
                    console.log("Start with credentials");
                    var startResult = await session.startWithCredentials({
                        accountName: call.username,
                        password: call.password,
                        steamGuardCode: call.steamGuardCode,
                    });
                    handleStartResult(startResult);
                    if (startResult.actionRequired) {
                        return;
                    }
                    return;
                }
                
                if (call.steamGuardCode) {
                    console.log("Using provided Steam Guard code:", call.steamGuardCode)
                    try {
                        await session.submitSteamGuardCode(call.steamGuardCode!);
                    } catch (ex: any) {
                        console.log(typeof ex);
                        console.error(ex);
                        console.log("Invalid Steam Guard code");
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
}


(async () => {
    await main();
})();