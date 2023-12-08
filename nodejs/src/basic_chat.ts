// Adapted from https://github.com/DoctorMcKay/node-steam-user/blob/master/examples/basicbot.js

// var SteamUser = require('steam-user');
import SteamUser from 'steam-user';
// import SteamChatRoomClient from 'steam-user/components/chatroom';
// import {EAccountType, EPersonaState, SteamID} from 'steamid';
import SteamID from 'steamid';
import { EAuthSessionGuardType, EAuthTokenPlatformType, LoginSession } from 'steam-session';

import * as readline from 'readline';
import * as crypto from 'crypto';
import * as fs from 'fs';
import { StartSessionResponseValidAction } from 'steam-session/dist/interfaces-external';


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

class CredentialsReader {
	private readonly secretKey?: string;

	constructor(secretKey?: string, private username?: string, private password?: string) {
		this.username = username;
		this.password = password;
		this.secretKey = secretKey;
	}

	usernamePrompt() {
		const rl = readline.createInterface({
			input: process.stdin,
			output: process.stderr
		});

		let inputUsername: string | undefined = this.username;
		return new Promise<string>((resolve, reject) => {
			if (inputUsername) {
				console.log(`Using default username: ${inputUsername}`);
				rl.close();
				resolve(inputUsername);
			} else {
				rl.question('Enter your username: ', (username) => {
					inputUsername = username || '';
					rl.close();
					resolve(inputUsername);
				});
			}
		});
	};

	passwordPrompt() {
		const rl = readline.createInterface({
			input: process.stdin,
			output: process.stderr
		});

		let inputPassword: string | undefined = this.password;
		return new Promise<string>((resolve, reject) => {
			if (inputPassword) {
				console.log(`Using default password: ${'*'.repeat(inputPassword.length)}`);
				rl.close();
				resolve(inputPassword);
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
					inputPassword = password || '';
					rl.close();
					resolve(inputPassword);
				});
			}
		});
	};

	async readCredentials(): Promise<{ username: string, password: string }> {
		return this.usernamePrompt()
			.then((username) => this.passwordPrompt().then((password) => {
				return { username: username, password: password };
			}));
	}

	get loginDataPath(): string {
		// prefix with XDG_DATA_HOME/node-steamuser
		// https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
		const dataHome = process.env.XDG_DATA_HOME || (process.platform === 'win32' ? process.env.APPDATA : process.env.HOME + '/.local/share');
		const dataDir = dataHome + '/node-steamuser';
		if (!fs.existsSync(dataDir)) {
			console.debug(`Creating data directory ${dataDir}`);
			fs.mkdirSync(dataDir);
		}
		return dataDir + '/loginData.json';
	}

	async saveLoginData(steamLoginData: SteamLoginData | null): Promise<void> {
		if (!this.secretKey) {
			fs.writeFile(this.loginDataPath, JSON.stringify(steamLoginData), (err) => {
				if (err) {
					console.error('Error saving login data:', err);
				} else {
					console.log('Saved login data to file');
				}
			});
			return;
		}

		const iv = crypto.randomBytes(16);
		const cipher = crypto.createCipheriv('aes-256-cbc', this.secretKey, iv);
		const encrypted = Buffer.concat([cipher.update(JSON.stringify(steamLoginData)), cipher.final()]);
		const data = JSON.stringify({ iv: iv.toString('hex'), encryptedData: encrypted.toString('hex') });

		await new Promise<void>((resolve, reject) => {
			fs.writeFile(this.loginDataPath, data, (err) => {
				if (err) {
					console.error('Error saving login data:', err);
					reject(err);
				} else {
					console.log('Saved login data to file');
					resolve();
				}
			});
		});
	}

	async loadLoginData(): Promise<SteamLoginData | null> {
		const data = await new Promise<string>((resolve, reject) => {
			fs.readFile(this.loginDataPath, (err, data) => {
				if (err) {
					console.log('No login data found');
					resolve('');
				} else {
					resolve(data.toString());
				}
			});
		});

		if (!data) {
			return null;
		}
		if (!this.secretKey) {
			return JSON.parse(data);
		}

		const { iv, encryptedData } = JSON.parse(data);
		const decipher = crypto.createDecipheriv('aes-256-cbc', this.secretKey, Buffer.from(iv, 'hex'));
		const decrypted = Buffer.concat([decipher.update(Buffer.from(encryptedData, 'hex')), decipher.final()]);
		const loginData = JSON.parse(decrypted.toString());

		return loginData;
	}
}

var reader = new CredentialsReader(
	process.env.SECRET_KEY?.toString(),
	process.env.STEAM_USERNAME,
	process.env.STEAM_PASSWORD,
);

var client = new SteamUser();
// let session = new LoginSession(EAuthTokenPlatformType.SteamClient);
let session = new LoginSession(EAuthTokenPlatformType.WebBrowser);
reader.loadLoginData().then(async (loginData) => {
	if (loginData) {
		console.log('Logging in with saved login data');
		client.logOn({
			refreshToken: loginData.refreshToken,
		});
	} else {
		console.log('No saved login data found, logging in with credentials via SteamSession');

		session.on('polling', async () => {
			console.debug("polling")
			var steamGuardMachineToken = await genericPrompt('Steam Guard machine token');
			console.log(`Submitting Steam Guard machine token: ${steamGuardMachineToken}`);
			await session.submitSteamGuardCode(steamGuardMachineToken);
		});

		session.on('authenticated', async () => {
			console.log(`Logged into Steam as ${session.accountName}`);
			let webCookies = await session.getWebCookies();

			if (webCookies) {
				var steamLoginData: SteamLoginData = {
					accessToken: session.accessToken,
					refreshToken: session.refreshToken,
					webCookies: webCookies
				};

				console.log('Saving login data');
				await reader.saveLoginData(steamLoginData);

				client.logOn({
					refreshToken: steamLoginData.refreshToken,
				});
			}
		});

		session.on('timeout', () => {
			console.log('This login attempt has timed out.');
		});

		session.on('error', (err) => {
			// This should ordinarily not happen. This only happens in case there's some kind of unexpected error while
			// polling, e.g. the network connection goes down or Steam chokes on something.
			console.log(`ERROR: This login attempt has failed! ${err.message}`);
		});

		var credentials = await reader.readCredentials();
		var startResult = await session.startWithCredentials({
			accountName: credentials.username,
			password: credentials.password,
		});

		// https://github.com/DoctorMcKay/node-steam-session/blob/master/examples/login-with-password.ts
		if (startResult.actionRequired) {
			console.log('Action is required from you to complete this login');

			// We want to process the non-prompting guard types first, since the last thing we want to do is prompt the
			// user for input. It would be needlessly confusing to prompt for input, then print more text to the console.
			let promptingGuardTypes = [EAuthSessionGuardType.EmailCode, EAuthSessionGuardType.DeviceCode];
			let promptingGuards = startResult.validActions!.filter(action => promptingGuardTypes.includes(action.type));
			let nonPromptingGuards = startResult.validActions!.filter(action => !promptingGuardTypes.includes(action.type));

			let printGuard = async ({ type, detail }: StartSessionResponseValidAction) => {
				let code;
				try {
					switch (type) {
						case EAuthSessionGuardType.EmailCode:
							console.log(`A login code has been sent to your email address at ${detail}`);
							code = await genericPrompt('Code');
							if (code) {
								await session.submitSteamGuardCode(code);
							}
							break;

						case EAuthSessionGuardType.DeviceCode:
							console.log('You may confirm this login by providing a Steam Guard Mobile Authenticator code');
							code = await genericPrompt('Code');
							if (code) {
								await session.submitSteamGuardCode(code);
							}
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
			};

			nonPromptingGuards.forEach(printGuard);
			promptingGuards.forEach(printGuard);
			// await printGuard(startResult.validActions![0] as { type: any; detail: any; });
		}

	}
});

session.on('timeout', () => {
	console.log('This login attempt has timed out.');
});

session.on('error', (err) => {
	console.log(`ERROR: This login attempt has failed! ${err.message}`);
});


client.on('loggedOn', function (details) {
	console.log("Logged into Steam as " + client.steamID?.getSteam3RenderedID());
	client.setPersona(SteamUser.EPersonaState.Online);  // needed to update {@link client.users}
});

client.on('error', function (e) {
	// Some error occurred during logon
	console.log(e);
});

let friendsListPromise = new Promise<void>(r => {
	client.on('friendsList', () => {
		console.log("Friends list loaded");
		r();
	});
});

let friendPersonasLoadPromise = new Promise<void>(r => {
	client.on('friendPersonasLoaded', () => {
		console.log("Personas loaded");
		r();
	});
});

// wait immediately
(async () => await friendsListPromise)();
(async () => await friendPersonasLoadPromise)();


function getUser(target: string | SteamID): SteamClientUser | undefined {
	if (target instanceof SteamID) {
		target = target.getSteamID64();
	}
	return client.users[target];
}

const sleep = (ms: number | undefined) => new Promise(r => setTimeout(r, ms));

// Send a chat message to a user
function sendMessageWithDelay(target: string, message: string, delay: number) {
	client.chat.sendFriendTyping(target);
	setTimeout(function () {
		client.chat.sendFriendMessage(target, message).then(function (messageID) {
			console.log("Message sent successfully");
		}).catch(function (err) {
			console.log("Error sending message:", err);
		});
	}, delay);
}

function getChatHistoryVerbose(target: string) {
	console.log("Getting chat history for friend", target, getUser(target)?.player_name);
	client.chat.getFriendMessageHistory(target, {
		startTime: Date.now() - 1000 * 60 * 60 * 24 * 1, // 1 day ago
	}).then(({ messages, more_available }) => {
		console.log("Chat history:", messages);
		console.log("More available:", more_available);
	}).catch((err) => {
		console.log("Error getting chat history:", err);
		return;
	});
}

async function sendMessageExample(target: string) {
	var messages = [
		"Hello, friend!",
		"What's up?",
		"You're so funny and smart!",
		"Are you a human?"
	]
	for (var i = 0; i < 10; i++) {
		var randIndex = Math.floor(Math.random() * messages.length);
		var message = messages[randIndex];
		sendMessageWithDelay(target, message, 1000 + message.length * 100);

		// Wait 5 seconds before sending another message, do this asynchronously so we don't block the whole process
		await sleep(5000);
	}
}

client.on('webSession', function (sessionID, cookies) {
	console.log("Got web session");
	// Do something with these cookies if you wish
	// console.log(sessionID);
	// console.log(cookies);  // sessionID, steamLogin, steamLoginSecure

	(async () => {
		client.setPersona(SteamUser.EPersonaState.Online);  // needed to update {@link client.users}
		await friendsListPromise;
		await friendPersonasLoadPromise;  // TODO: somehow this is never fied (even if we setPersona)

		// List all friends
		var friends = client.myFriends;
		console.log("We have " + Object.keys(friends).length + " friends");
		Object.entries(friends).forEach(function ([steamID, relationship]) {
			console.log(` - ${steamID}: ${JSON.stringify(getUser(steamID)?.player_name)} ${relationship}`);
		});

		const target: string = process.env.STEAM_TARGET!;
		if (!target) {
			console.log("No target specified");
			return;
		}

		let activeSessions = await client.chat.getActiveFriendMessageSessions();
		console.debug(activeSessions);

		let friendSessions: Record<string, any> = {}
		for (let session of activeSessions.sessions) {
			friendSessions[session.steamid_friend] = session;
			console.log(`Session with ${getUser(session.steamid_friend)?.player_name} ${session.steamid_friend.getSteam3RenderedID()}: ${session.unread_message_count} unread messages, last message ${session.time_last_message}`);
		}

		// SteamChatRoomClient#friendMessage
		function formatMessage(message): string {
			return `[${message.server_timestamp}] ${getUser(message.sender || message.steamid_friend)?.player_name}: ${message.message_bbcode_parsed}`;
		} 
		function writeStringToFile(val: string) {
			console.log("writeStringToFile", val);
			const len = Buffer.byteLength(val, 'utf8');
			// Create a buffer with space for the length and the data
			const buf = Buffer.alloc(4 + len);
			buf.writeUInt32LE(len, 0);
			buf.write(val, 4, len, 'utf8');
			console.log("Wrote buffer", buf.length);
			// Append to a normal file
			fs.appendFileSync("/tmp/basic_steam_recv.txt", buf);
		}


		client.chat.on('friendMessage', (message) => {
			console.log(formatMessage(message));
			writeStringToFile(formatMessage(message));
		});

		// getChatHistoryVerbose(target);
		const session = friendSessions[target];
		console.log("Getting chat history for friend", target, getUser(target)?.player_name);

		// Create a writable stream for the output pipe
		// const recvMessagePipePath = '/tmp/basic_steam_recv.pipe';
		// const recvPipe = fs.openSync(recvMessagePipePath, 'w');

		await (client.chat.getFriendMessageHistory(target, {
			startTime: (session?.time_last_message || Date.now()) - 1000 * 60 * 60 * 24 * 1, // 1 day ago
			maxCount: 50,
		}).then(({ messages, more_available }) => {
			// console.log("Chat history:", messages);
			// console.log("More available:", more_available);
			messages.sort((a, b) => Number(a.server_timestamp) - Number(b.server_timestamp));
			for (let message of messages) {
				const val = formatMessage(message);
				console.log(val);
				writeStringToFile(val);
			}
		}).catch((err) => {
			console.log("Error getting chat history:", err);
			return;
		}))

		// Wait for user input from stdin
		// while (true) {
		// 	const input = await genericPrompt('> ', undefined, false);
		// 	if (input === '/exit') {
		// 		break;
		// 	}
		// 	sendMessageWithDelay(target, input, 0);
		// }

		// Read null-terminated message strings from a named pipe and send them using sendMessageWithDelay
		const sendMessagePipePath = "/tmp/basic_steam_send.pipe"  // a named pipe
		const sendPipe = fs.createReadStream(sendMessagePipePath, { highWaterMark: 65536 });
		let bufferStr = '';
		sendPipe.on('data', (data: Buffer) => {
			bufferStr += data.toString('utf8');
			let buffer = Buffer.from(bufferStr, 'utf8');
			let pos = 0;
			console.log("Source:", bufferStr);
			while (pos < buffer.length) {
				// Read the length as a uint32_t
				const len = buffer.readUInt32LE(pos);
				pos += 4;

				// Read the data as a string
				const val = buffer.toString('utf8', pos, pos + len);
				pos += len;

				// Handle the message
				console.log("Send message:", val);
				console.log("Send message length:", len);
				console.log("Stream pos", pos, "buffer length", buffer.length, "source length", bufferStr.length);
				sendMessageWithDelay(target, val.toString(), 0);
			}
			bufferStr = "";
			console.log("Done reading data");
			// const messages = buffer.split('\0');
			// buffer = messages.pop() || '';
			// for (const message of messages) {
			// 	sendMessageWithDelay(target, message, 0);
			// }
		});
		sendPipe.on('error', (err: Error) => {
			console.error(`Error reading from named pipe ${sendMessagePipePath}:`, err);
		});

		// Intercept Ctrl+C in the terminal and log off the client on the interrupt
		process.on('SIGINT', () => {
			console.log('Received interrupt signal, logging off');
			client.logOff();
			// exit
			process.exit(0);
		});
	})();
});

client.on('emailInfo', function (address, validated) {
	console.log("Our email address is " + address + " and it's " + (validated ? "validated" : "not validated"));
});

client.on('accountLimitations', function (limited, communityBanned, locked, canInviteFriends) {
	var limitations: string[] = [];

	if (limited) {
		limitations.push('LIMITED');
	}
	if (communityBanned) {
		limitations.push('COMMUNITY BANNED');
	}
	if (locked) {
		limitations.push('LOCKED');
	}
	if (limitations.length === 0) {
		console.log("Our account has no limitations.");
	} else {
		console.log("Our account is " + limitations.join(', ') + ".");
	}
	if (canInviteFriends) {
		console.log("Our account can invite friends.");
	}
});