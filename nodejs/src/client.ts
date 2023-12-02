import * as readline from 'readline';
import { PromiseClient, createPromiseClient } from "@connectrpc/connect";
import { createConnectTransport } from "@connectrpc/connect-node";
import { AuthService } from './protobufs/comm_protobufs/auth_connect'
import { AuthResponse_AuthState } from './protobufs/comm_protobufs/auth_pb'
import { MessageService } from './protobufs/comm_protobufs/message_connect'
import { MessageRequest, SendMessageResult, ResponseMessage, PollRequest, FriendsListRequest, FriendsListResponse } from './protobufs/comm_protobufs/message_pb'
import * as fs from 'fs';
import { AnyMessage, MethodInfoServerStreaming, MethodKind } from '@bufbuild/protobuf';
// import { MethodInfoServerStreaming } from "@connectrpc/connect/dist/cjs/promise-client";


function genericPrompt(prompt: string, knownValue?: string, prefix: boolean = true) {
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stderr
    });

    return new Promise<string>((resolve, reject) => {
        if (knownValue) {
            console.log(`Using default ${prompt}: ${knownValue}`);
            rl.close();
            resolve(knownValue);
        } else {
            if (prefix) {
                prompt = `Enter your ${prompt}: `;
            }
            rl.question(prompt, (value) => {
                rl.close();
                resolve(value || '');
            });
        }
    });
}


function passwordPrompt(knownValue?: string) {
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stderr
    });

    return new Promise<string>((resolve, reject) => {
        if (knownValue) {
            console.log(`Using default password: ${'*'.repeat(knownValue.length)}`);
            rl.close();
            resolve(knownValue);
        } else {
            // Hide the password as the user types it
            let hide = false;
            (<any>rl)._writeToOutput = function _writeToOutput(stringToWrite: string) {
                if (hide) {
                    // (<any>rl).output.write("*");
                } else {
                    (<any>rl).output.write(stringToWrite);
                    hide = true;
                }
            };

            rl.question('Enter your password: ', (password) => {
                rl.close();
                console.log();
                resolve(password);
            });
        }
    });
};

class AuthWrapper {
    private sessionKey: string | undefined;
    private client: any;

    constructor(private transport: any) {
        this.client = createPromiseClient(AuthService, this.transport);
    }

    private readRefreshTokens(): any {
        const filePath = `${process.env.HOME}/.local/share/node-steamuser/refreshTokens.json`;
        try {
            const data = fs.readFileSync(filePath, 'utf8');
            return JSON.parse(data);
        } catch (error) {
            console.error(`Failed to read refresh tokens: ${error}`);
            return {};
        }
    }

    private writeRefreshTokens(refreshTokens: any) {
        const filePath = `${process.env.HOME}/.local/share/node-steamuser/refreshTokens.json`;
        try {
            const data = JSON.stringify(refreshTokens, null, 2);
            fs.writeFileSync(filePath, data, 'utf8');
        } catch (error) {
            console.error(`Failed to write refresh tokens: ${error}`);
        }
    }

    private updateRefreshTokens(refreshTokens: any, username: string, refreshToken: string | undefined) {
        if (!refreshToken) {
            return;
        }
        refreshTokens[username] = refreshToken;
        this.writeRefreshTokens(refreshTokens);
    }

    public async login() {
        await this._login();
        return this.sessionKey!;
    }

    private async _login() {
        console.log("Session key:", this.sessionKey);
        let username = await genericPrompt("username", process.env.STEAM_USERNAME);
        let password = await passwordPrompt(process.env.STEAM_PASSWORD);
        var steamGuardCode: string | undefined;

        // Try auth once with refresh token
        const refreshTokens = this.readRefreshTokens();
        const refreshToken = refreshTokens[username];
        if (refreshToken) {
            console.log("Using refresh token");
            const res = await this.client.authenticate({
                username: username,
                refreshToken: refreshToken,
            });
            if (res.reason == AuthResponse_AuthState.SUCCESS) {
                console.log("Successfully logged on");
                this.sessionKey = res.sessionKey;
                this.updateRefreshTokens(refreshTokens, username, res.refreshToken);
                return;
            }
        }

        for (let tries = 1; tries <= 3; tries++) {
            console.log("Attempt", tries, "with session key", this.sessionKey);
            const res = await this.client.authenticate({
                username: username,
                password: password,
                steamGuardCode: steamGuardCode,
                sessionKey: this.sessionKey,
            });
            console.log("Got session key:", res.sessionKey);
            this.sessionKey = res.sessionKey;
            if (res.reason == AuthResponse_AuthState.STEAM_GUARD_CODE_REQUEST) {
                steamGuardCode = await genericPrompt("Steam guard code");
            } else if (res.reason == AuthResponse_AuthState.INVALID_CREDENTIALS) {
                console.log("Invalid credentials!");
                throw new Error("Invalid credentials");
            } else if (res.reason == AuthResponse_AuthState.SUCCESS) {
                console.log("Successfully logged on");
                this.updateRefreshTokens(refreshTokens, username, res.refreshToken);
                break;
            }
        }
        console.log("Too many tries!");
        throw new Error("Too many tries");
    }
}

class MessageClient {
    // private client: PromiseClient<{
    //     readonly typeName: "steam.MessageService";
    //     readonly methods: {
    //         readonly sendChatMessage: { readonly name: "SendChatMessage"; readonly I: typeof MessageRequest; readonly O: typeof SendMessageResult; readonly kind: MethodKind.Unary; };
    //         readonly pollChatMessages: MethodInfoServerStreaming<PollRequest, ResponseMessage>;
    //         readonly getFriendsList: { readonly name: "GetFriendsList"; readonly I: typeof FriendsListRequest; readonly O: typeof FriendsListResponse; readonly kind: MethodKind.Unary; };
    //     }; }>;
    private client = createPromiseClient(MessageService, this.transport);
    private sessionKey: string;

    constructor(private transport: any, sessionKey: string) {
        // this.client = createPromiseClient(MessageService, this.transport);
        this.sessionKey = sessionKey;
    }

    public async sendMessage(targetId: string, message: string) {
        console.log("Sending message:", message)
        const res = await this.client.sendChatMessage({
            sessionKey: this.sessionKey,
            targetId: targetId,
            message: message,
        });
        console.log("Message sent:", res);
    }

    public async pollMessages() {
        console.log("Polling messages")
        const res = this.client.pollChatMessages({
            sessionKey: this.sessionKey,
        });
        for await (const message of res) {
            console.log("Received message:", message);
        }
    }
    public async getFriendsList() {
        console.log("Getting friends list")
        const res = await this.client.getFriendsList({
            sessionKey: this.sessionKey,
        });
        console.log("Friends list:", res);
    }
}


const transport = createConnectTransport({
    httpVersion: "1.1",
    baseUrl: "http://localhost:8080/",
});

async function main() {
    const authWrapper = new AuthWrapper(transport);
    let sessionKey : string;
    try {
        sessionKey = await authWrapper.login();
        console.log("Login successful");
    } catch (error) {
        console.error("Login failed:", error);
        throw error;
    }

    await new Promise((resolve) => setTimeout(resolve, 2000)); // Wait for 2 seconds

    const messageClient = new MessageClient(transport, sessionKey);
    // await messageClient.sendMessage("Hello world!");
    // await messageClient.pollMessages();
    await messageClient.getFriendsList();
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});