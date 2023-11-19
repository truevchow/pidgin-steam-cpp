/*
import net from 'net';
import { Buffer } from 'buffer';
import protobuf from 'protobufjs';
import fs from 'fs';
*/
import grpc from '@grpc/grpc-js';
import { loadSync } from '@grpc/proto-loader';
import { promisify } from 'util';
import net from 'net';
import { Buffer } from 'buffer';
// import protobuf from 'protobufjs';
import fs from 'fs';

enum State {
    USERNAME,
    PASSWORD,
    STEAM_GUARD_CODE,
}

enum ProtocolHeader {
    USERNAME_REQUEST = 0x01,
    // PASSWORD_REQUEST = 0x02,
    STEAM_GUARD_CODE_REQUEST = 0x03,
    SUCCESS_RESPONSE = 0x04,
    FAILURE_RESPONSE = 0x05,
}

// Define the protobuf schema
// const schema = `...';

const STEAM_AUTH_SOCK = '/tmp/steam_auth.sock';

// Clean up socket if it exists
const cleanup = () => {
    if (fs.existsSync(STEAM_AUTH_SOCK)) {
        fs.unlinkSync(STEAM_AUTH_SOCK);
        console.log('Removed /tmp/steam_auth.sock');
    }
};

cleanup();

/*
// Load the protobuf schema
const root = protobuf.loadSync('../protobufs_src/comm_protobufs/auth.proto').root;
const AuthRequest = root.lookupType('steam.AuthRequest');
const AuthResponse = root.lookupType('steam.AuthResponse');

const server = net.createServer(async (socket) => {
    // Send a request for the username to the client
    // socket.write(Buffer.from([1]));

    // Handle incoming messages from the client
    socket.on('data', async (data) => {
        if (data && data.length > 0) {
            const header = data[0];

            switch (header) {
                case ProtocolHeader.USERNAME_REQUEST:
                    // Parse the username and password from the message
                    const { username, password } = AuthRequest.decode(data.subarray(1)).toJSON();
                    console.log('Username:', username);
                    console.log('Password:', password);

                    // Send a request for the password to the client
                    // socket.write(Buffer.from([ProtocolHeader.PASSWORD_REQUEST]));

                    // Send a request for the Steam Guard code to the client
                    socket.write(Buffer.from([ProtocolHeader.STEAM_GUARD_CODE_REQUEST]));

                    break;
                case ProtocolHeader.STEAM_GUARD_CODE_REQUEST:
                    // Parse the Steam Guard code from the message
                    const { steamGuardCode } = AuthRequest.decode(data.subarray(1)).toJSON();
                    console.log('Steam Guard code:', steamGuardCode);

                    // Send a success or failure message to the client
                    if (steamGuardCode === '123456') {
                        const response = AuthResponse.create({ success: true });
                        const buffer = AuthResponse.encode(response).finish();
                        socket.write(Buffer.concat([Buffer.from([ProtocolHeader.SUCCESS_RESPONSE]), buffer]));
                    } else {
                        const response = AuthResponse.create({ success: false });
                        const buffer = AuthResponse.encode(response).finish();
                        socket.write(Buffer.concat([Buffer.from([ProtocolHeader.FAILURE_RESPONSE]), buffer]));
                    }

                    break;
                default:
                    console.log('Unknown message received', data.toString('hex'));
                    break;
            }
        }
    });
});

server.listen(STEAM_AUTH_SOCK, () => {
    console.log('Server listening on /tmp/steam_auth.sock');
});
*/


import { AuthServiceService } from './protobufs/comm_protobufs/auth_grpc_pb';

// const PROTO_PATH = '../protobufs_src/comm_protobufs/auth.proto';
// const packageDefinition = loadSync(PROTO_PATH);
// const authProto = grpc.loadPackageDefinition(packageDefinition).steam;

const server = new grpc.Server();

server.addService(AuthServiceService, {
    authenticate: async (call) => {
        const { username, password, steamGuardCode } = call.request;

        console.log('Username:', username);
        console.log('Password:', password);
        console.log('Steam Guard code:', steamGuardCode);

        if (steamGuardCode === '123456') {
            return { success: true };
        } else {
            return { success: false };
        }
    },
});

const bindAsync = promisify(server.bindAsync).bind(server);
const startAsync = promisify(server.start).bind(server);

(async () => {
    await bindAsync('/tmp/steam_auth.sock', grpc.ServerCredentials.createInsecure());
    console.log('Server listening on /tmp/steam_auth.sock');
    await startAsync();
});


// Cleanup the socket file on exit
process.on('exit', cleanup);

process.on('SIGINT', () => {
    cleanup();
    process.exit(0);
});

/*
// Mock client
const client = net.createConnection(STEAM_AUTH_SOCK, () => {
console.log('Client connected to server');

// Send a username request to the server
const request = AuthRequest.create({ username: 'testuser', password: 'testpass', steamGuardCode: '' });
const buffer = AuthRequest.encode(request).finish();
client.write(Buffer.concat([Buffer.from([ProtocolHeader.USERNAME_REQUEST]), buffer]));
});

client.on('data', (data) => {
    const header = data[0];

    switch (header) {
        case ProtocolHeader.STEAM_GUARD_CODE_REQUEST:
            // Send a Steam Guard code to the server
            const request = AuthRequest.create({ username: '', password: '', steamGuardCode: '123456' });
            const buffer = AuthRequest.encode(request).finish();
            client.write(Buffer.concat([Buffer.from([ProtocolHeader.STEAM_GUARD_CODE_REQUEST]), buffer]));
            break;
        case ProtocolHeader.SUCCESS_RESPONSE:
            console.log('Authentication successful');
            client.end();
            break;
        case ProtocolHeader.FAILURE_RESPONSE:
            console.log('Authentication failed');
            client.end();
            break;
        default:
            console.log('Unknown message received', data.toString('hex'));
            client.end();
            break;
    }
});

client.on('end', () => {
    console.log('Client disconnected from server');
});
*/