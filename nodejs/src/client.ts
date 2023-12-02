import * as readline from 'readline';
import { createPromiseClient } from "@connectrpc/connect";
import { createConnectTransport } from "@connectrpc/connect-node";
import { AuthService } from './protobufs/comm_protobufs/auth_connect'
import { AuthState } from './protobufs/comm_protobufs/auth_pb'
import * as fs from 'fs';


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
            if (res.reason == AuthState.SUCCESS) {
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
            if (res.reason == AuthState.STEAM_GUARD_CODE_REQUEST) {
                steamGuardCode = await genericPrompt("Steam guard code");
            } else if (res.reason == AuthState.INVALID_CREDENTIALS) {
                console.log("Invalid credentials!");
                throw new Error("Invalid credentials");
            } else if (res.reason == AuthState.SUCCESS) {
                console.log("Successfully logged on");
                this.updateRefreshTokens(refreshTokens, username, res.refreshToken);
                break;
            }
        }
        console.log("Too many tries!");
        throw new Error("Too many tries");
    }
}

const transport = createConnectTransport({
    httpVersion: "1.1",
    baseUrl: "http://localhost:8080/",
});

async function main() {
    const authWrapper = new AuthWrapper(transport);
    try {
        await authWrapper.login();
        console.log("Login successful");
    } catch (error) {
        console.error("Login failed:", error);
        throw error;
    }
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});