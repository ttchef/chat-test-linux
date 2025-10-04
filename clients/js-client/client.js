#!/usr/bin/env node

import { connect } from 'net';
import { createInterface } from 'readline';
import { writeFileSync, appendFileSync } from 'fs';
import { createHash } from 'crypto';

// Constants
const BUFFER_SIZE = 4096;
const DEFAULT_IP = 'localhost';
const WS_GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

// Message flags
const WS_NO_BROADCAST = 1 << 0;
const WS_SEND_BACK = 1 << 1;
const WS_CHANGE_USERNAME = 1 << 2;

// Create JSON message for the server
function createJsonMessage(username, text, flags = 0) {
    return JSON.stringify({
        user: { name: username },
        message: {
            text: text,
            text_len: text.length,
            info: flags
        }
    });
}

// Parse command-line arguments
function parseArgs() {
    const args = {
        testMsg: null,
        headless: false,
        chatLog: false,
        logFileName: 'chat_log.log',
        name: null,
        nameType: false,
        host: null,
        port: null,
        portSpecified: false,
        hostSpecified: false
    };

    for (let i = 2; i < process.argv.length; i++) {
        if (process.argv[i] === '-m' && i + 1 < process.argv.length) {
            args.testMsg = process.argv[++i];
            args.headless = true;
        }
        else if (process.argv[i] === '-n' && i + 1 < process.argv.length) {
            args.nameType = true;
            args.name = process.argv[++i];
        }
        else if (process.argv[i] === '-s') {
            console.log('Running client with logging');
            args.chatLog = true;
        }
        else if (process.argv[i] === '-h' && i + 1 < process.argv.length) {
            args.host = process.argv[++i];
            args.hostSpecified = true;
        }
        else if (process.argv[i] === '-p' && i + 1 < process.argv.length) {
            args.port = process.argv[++i];
            args.portSpecified = true;
        }
    }

    if (!args.hostSpecified) {
        args.host = DEFAULT_IP;
    }

    if (!args.portSpecified) {
        if (args.host === 'localhost' || args.host === '127.0.0.1' || args.host === '0.0.0.0') {
            args.port = '9999';
        } else {
            args.port = '80';
        }
    }

    return args;
}

// Encode WebSocket frame (client-to-server with masking)
function encodeFrame(payload) {
    const payloadBuffer = Buffer.from(payload);
    const len = payloadBuffer.length;

    // Generate random mask
    const mask = Buffer.alloc(4);
    for (let i = 0; i < 4; i++) {
        mask[i] = Math.floor(Math.random() * 256);
    }

    let frame;
    if (len <= 125) {
        frame = Buffer.alloc(6 + len);
        frame[0] = 0x81; // FIN + text frame
        frame[1] = 0x80 | len; // MASK + length
        mask.copy(frame, 2);
        for (let i = 0; i < len; i++) {
            frame[6 + i] = payloadBuffer[i] ^ mask[i % 4];
        }
    } else if (len <= 65535) {
        frame = Buffer.alloc(8 + len);
        frame[0] = 0x81;
        frame[1] = 0x80 | 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        mask.copy(frame, 4);
        for (let i = 0; i < len; i++) {
            frame[8 + i] = payloadBuffer[i] ^ mask[i % 4];
        }
    } else {
        throw new Error('Payload too large');
    }

    return frame;
}

// Decode WebSocket frame (server-to-client, no mask)
function decodeFrame(data) {
    if (data.length < 2) return null;

    const opcode = data[0] & 0x0F;
    if (opcode === 0x8) return { type: 'close' };

    let payloadLen = data[1] & 0x7F;
    let pos = 2;

    if (payloadLen === 126) {
        payloadLen = (data[2] << 8) | data[3];
        pos = 4;
    } else if (payloadLen === 127) {
        payloadLen = 0;
        for (let i = 0; i < 8; i++) {
            payloadLen = (payloadLen * 256) + data[2 + i];
        }
        pos = 10;
    }

    if (data.length < pos + payloadLen) return null;

    const payload = data.slice(pos, pos + payloadLen);
    return { type: 'message', payload: payload.toString(), consumed: pos + payloadLen };
}

// Perform WebSocket handshake
function performHandshake(socket, host) {
    return new Promise((resolve, reject) => {
        const key = 'dGhlIHNhbXBsZSBub25jZQ==';

        const request = `GET / HTTP/1.1\r\n` +
                       `Host: ${host}\r\n` +
                       `Upgrade: websocket\r\n` +
                       `Connection: Upgrade\r\n` +
                       `Sec-WebSocket-Key: ${key}\r\n` +
                       `Sec-WebSocket-Version: 13\r\n\r\n`;

        socket.write(request);

        const timeout = setTimeout(() => {
            reject(new Error('Handshake timeout'));
        }, 10000);

        socket.once('data', (data) => {
            clearTimeout(timeout);
            const response = data.toString();
            console.error('Server response:\n' + response);

            if (response.includes('101 Switching Protocols')) {
                resolve();
            } else {
                reject(new Error('WebSocket handshake failed'));
            }
        });
    });
}

// Main client function
async function main() {
    const args = parseArgs();

    if (args.chatLog) {
        try {
            writeFileSync(args.logFileName, '');
            console.log('Created file');
        } catch (err) {
            console.error(`Failed to create ${args.logFileName} file try running without -s flag!`);
            process.exit(1);
        }
    }

    console.log(`Connecting to server at ${args.host}:${args.port}`);

    const socket = connect(parseInt(args.port), args.host);

    let receivedResponse = false;
    let timeoutId = null;
    let buffer = Buffer.alloc(0);
    let rl = null;

    let handshakeComplete = false;
    let currentUsername = args.name || 'Anonym';

    socket.on('connect', async () => {
        console.log(`Connected to server at ${args.host}:${args.port}`);

        try {
            await performHandshake(socket, args.host);
            console.log('WebSocket handshake complete');
            handshakeComplete = true;

            // Send username if provided
            if (args.nameType && args.name) {
                const jsonMsg = createJsonMessage(currentUsername, 'null', WS_CHANGE_USERNAME | WS_NO_BROADCAST);
                socket.write(encodeFrame(jsonMsg));
            }

            // Send test message if in headless mode
            if (args.headless && args.testMsg) {
                const jsonMsg = createJsonMessage(currentUsername, args.testMsg, 0);
                socket.write(encodeFrame(jsonMsg));
                process.stdout.write(`Sent: ${args.testMsg}`);
            }

            // Set up stdin reading in interactive mode
            if (!args.headless) {
                rl = createInterface({
                    input: process.stdin,
                    output: process.stdout,
                    terminal: true
                });

                rl.setPrompt('> ');
                rl.prompt();

                rl.on('line', (input) => {
                    if (!input) {
                        rl.prompt();
                        return;
                    }

                    if (input.toLowerCase() === 'exit') {
                        socket.end();
                        process.exit(0);
                    }

                    const jsonMsg = createJsonMessage(currentUsername, input, 0);
                    socket.write(encodeFrame(jsonMsg));
                    console.log(`Sent: ${input}`);
                    rl.prompt();
                });

                rl.on('close', () => {
                    socket.end();
                    process.exit(0);
                });
            } else {
                timeoutId = setTimeout(() => {
                    if (receivedResponse) {
                        console.log('Test complete');
                        socket.end();
                        process.exit(0);
                    }
                }, 5000);
            }
        } catch (err) {
            console.error(err.message);
            socket.end();
            process.exit(1);
        }
    });

    socket.on('data', (data) => {
        if (!handshakeComplete) return; // Handshake handler will process initial data

        buffer = Buffer.concat([buffer, data]);

        while (buffer.length > 0) {
            const result = decodeFrame(buffer);

            if (!result) break;

            if (result.type === 'close') {
                console.log('Server disconnected');
                socket.end();
                process.exit(0);
            }

            if (result.type === 'message') {
                const payload = result.payload;

                if (args.chatLog && payload !== '') {
                    try {
                        appendFileSync(args.logFileName, payload);
                    } catch (err) {
                        console.error(`Failed to open ${args.logFileName} try running without -s flag!`);
                        process.exit(1);
                    }
                }

                process.stdout.write(`Received: ${payload}`);
                receivedResponse = true;

                buffer = buffer.slice(result.consumed);
            }
        }
    });

    socket.on('error', (err) => {
        if (err.code === 'ETIMEDOUT' || err.code === 'ECONNREFUSED') {
            console.error(`Connection timeout to ${args.host}:${args.port}`);
        } else {
            console.error(`Connection failed to ${args.host}:${args.port}: ${err.message}`);
        }
        process.exit(1);
    });

    socket.on('end', () => {
        console.log('Server disconnected');
        if (timeoutId) clearTimeout(timeoutId);
        if (rl) rl.close();
        process.exit(0);
    });
}

main().catch((err) => {
    console.error('Error:', err.message);
    process.exit(1);
});
