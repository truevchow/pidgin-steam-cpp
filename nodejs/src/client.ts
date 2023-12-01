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

const transport = createConnectTransport({
    httpVersion: "1.1",
    baseUrl: "http://localhost:8080/",
});

async function main() {
    const client = createPromiseClient(AuthService, transport);
    var sessionKey: string | undefined = process.env.SESSION_ID;
    console.log("Session key:", sessionKey);
    let username = await genericPrompt("username", process.env.STEAM_USERNAME);
    let password = await passwordPrompt(process.env.STEAM_PASSWORD);
    var steamGuardCode: string | undefined;


    interface RefreshTokens {
        [username: string]: string;
    }

    function readRefreshTokens(): RefreshTokens {
        const filePath = `${process.env.HOME}/.local/share/node-steamuser/refreshTokens.json`;
        try {
            const data = fs.readFileSync(filePath, 'utf8');
            return JSON.parse(data);
        } catch (error) {
            console.error(`Failed to read refresh tokens: ${error}`);
            return {};
        }
    }

    function writeRefreshTokens(refreshTokens: RefreshTokens) {
        const filePath = `${process.env.HOME}/.local/share/node-steamuser/refreshTokens.json`;
        try {
            const data = JSON.stringify(refreshTokens, null, 2);
            fs.writeFileSync(filePath, data, 'utf8');
        } catch (error) {
            console.error(`Failed to write refresh tokens: ${error}`);
        }
    }

    function updateRefreshTokens(refreshTokens: RefreshTokens, username: string, refreshToken: string | undefined) {
        if (!refreshToken) {
            return;
        }
        refreshTokens[username] = refreshToken;
        writeRefreshTokens(refreshTokens);
    }

    // Try auth once with refresh token
    const refreshTokens = readRefreshTokens();
    const refreshToken = refreshTokens[username];
    if (refreshToken) {
        console.log("Using refresh token");
        const res = await client.authenticate({
            username: username,
            refreshToken: refreshToken,
        });
        if (res.reason == AuthState.SUCCESS) {
            console.log("Successfully logged on");
            sessionKey = res.sessionKey;
            updateRefreshTokens(refreshTokens, username, res.refreshToken);
            return;
        }
    }

    var tries = 0;
    while (true) {
        tries++;
        if (tries > 3) {
            console.log("Too many tries!");
            return;
        }
        console.log("Attempt with session key", sessionKey);
        const res = await client.authenticate({
            username: username,
            password: password,
            steamGuardCode: steamGuardCode,
            sessionKey: sessionKey,
        });
        console.log("Got session key:", res.sessionKey);
        sessionKey = res.sessionKey;
        if (res.reason == AuthState.STEAM_GUARD_CODE_REQUEST) {
            steamGuardCode = await genericPrompt("Steam guard code");
        } else if (res.reason == AuthState.INVALID_CREDENTIALS) {
            console.log("Invalid credentials!");
            return;
        } else if (res.reason == AuthState.SUCCESS) {
            console.log("Successfully logged on");
            updateRefreshTokens(refreshTokens, username, res.refreshToken);
            break;
        }
    }
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});