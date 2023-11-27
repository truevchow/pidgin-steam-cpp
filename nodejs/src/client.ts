import * as readline from 'readline';
import { createPromiseClient } from "@connectrpc/connect";
import { createConnectTransport } from "@connectrpc/connect-node";
import { AuthService } from './protobufs/comm_protobufs/auth_connect'
import { AuthState } from './protobufs/comm_protobufs/auth_pb'

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

    // NOTE: using Steam Guard code initially results in DuplicateRequest error
    // var steamGuardCode: string | undefined = await genericPrompt("Steam guard code", process.env.STEAM_GUARD_CODE);
    // if (steamGuardCode == "") {
    //     steamGuardCode = undefined;
    // }
    // console.log("Username:", username);
    // console.log("Password", password);
    // console.log("Steam guard code:", steamGuardCode);
    // return;

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
            break;
        }
    }
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});