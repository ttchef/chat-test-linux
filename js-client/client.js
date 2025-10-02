#!/usr/bin/env node

import WebSocket from 'ws';
import { createInterface } from 'readline';
import { writeFileSync, appendFileSync } from 'fs';

// Constants
const BUFFER_SIZE = 4096;
const DEFAULT_IP = 'localhost'; // Default IP (matches IP from ip.h)

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
        // -m flag: test message for headless mode
        if (process.argv[i] === '-m' && i + 1 < process.argv.length) {
            args.testMsg = process.argv[++i];
            args.headless = true;
        }
        // -n flag: username/display name
        else if (process.argv[i] === '-n' && i + 1 < process.argv.length) {
            args.nameType = true;
            args.name = process.argv[++i];
        }
        // -s flag: enable chat logging
        else if (process.argv[i] === '-s') {
            console.log('Running client with logging');
            args.chatLog = true;
        }
        // -h flag: server host
        else if (process.argv[i] === '-h' && i + 1 < process.argv.length) {
            args.host = process.argv[++i];
            args.hostSpecified = true;
        }
        // -p flag: server port
        else if (process.argv[i] === '-p' && i + 1 < process.argv.length) {
            args.port = process.argv[++i];
            args.portSpecified = true;
        }
    }

    // Set default host if not specified
    if (!args.hostSpecified) {
        args.host = DEFAULT_IP;
    }

    // Set default port based on host if not specified
    if (!args.portSpecified) {
        if (args.host === 'localhost' || args.host === '127.0.0.1' || args.host === '0.0.0.0') {
            args.port = '9999'; // Use port 9999 for local connections
        } else {
            args.port = '80'; // Use port 80 for remote connections (Cloudflare tunnels)
        }
    }

    return args;
}

// Main client function
async function main() {
    const args = parseArgs();

    // Create log file if chat logging is enabled
    if (args.chatLog) {
        try {
            writeFileSync(args.logFileName, '');
            console.log('Created file');
        } catch (err) {
            console.error(`Failed to create ${args.logFileName} file try running without -s flag!`);
            process.exit(1);
        }
    }

    // Build WebSocket URI
    const serverUri = `ws://${args.host}:${args.port}`;

    console.log(`Connecting to server at ${args.host}:${args.port}`);

    // Create WebSocket connection
    const ws = new WebSocket(serverUri, {
        // Connection timeout (10 seconds)
        handshakeTimeout: 10000
    });

    // Track if we've received a response (for headless mode)
    let receivedResponse = false;
    let timeoutId = null;

    // Connection opened
    ws.on('open', () => {
        console.log(`Connected to server at ${args.host}:${args.port}`);
        console.log('WebSocket handshake complete');

        // Send username if provided
        if (args.nameType && args.name) {
            const idMsg = `[ID]${args.name}`;
            ws.send(idMsg);
        }

        // Send test message if in headless mode
        if (args.headless && args.testMsg) {
            ws.send(args.testMsg);
            process.stdout.write(`Sent: ${args.testMsg}`);
        }

        // Set up stdin reading in interactive mode
        if (!args.headless) {
            const rl = createInterface({
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
                    ws.close();
                    process.exit(0);
                }

                // Send input with newline (matching C client behavior)
                ws.send(input + '\n');
                console.log(`Sent: ${input}`);
                rl.prompt();
            });

            rl.on('close', () => {
                ws.close();
                process.exit(0);
            });
        } else {
            // In headless mode, set up timeout
            const timeout = 5000; // 5 seconds for headless mode
            timeoutId = setTimeout(() => {
                if (receivedResponse) {
                    console.log('Test complete');
                    ws.close();
                    process.exit(0);
                }
            }, timeout);
        }
    });

    // Handle incoming messages
    ws.on('message', (data) => {
        const payload = data.toString();

        // Log to file if chat logging is enabled
        if (args.chatLog && payload !== '') {
            try {
                appendFileSync(args.logFileName, payload);
            } catch (err) {
                console.error(`Failed to open ${args.logFileName} try running without -s flag!`);
                process.exit(1);
            }
        }

        // Print the received message
        process.stdout.write(`Received: ${payload}`);

        // Mark that we've received at least one response
        receivedResponse = true;
    });

    // Handle connection errors
    ws.on('error', (err) => {
        if (err.code === 'ETIMEDOUT' || err.code === 'ECONNREFUSED') {
            console.error(`Connection timeout to ${args.host}:${args.port}`);
        } else {
            console.error(`Connection failed to ${args.host}:${args.port}: ${err.message}`);
        }
        process.exit(1);
    });

    // Handle connection close
    ws.on('close', () => {
        console.log('Server disconnected');
        if (timeoutId) clearTimeout(timeoutId);
        process.exit(0);
    });
}

// Run the client
main().catch((err) => {
    console.error('Error:', err.message);
    process.exit(1);
});
