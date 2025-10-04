# JavaScript WebSocket Client

A Node.js WebSocket client implementation with JSON message support and interactive chat interface.

## Features

- WebSocket protocol implementation (RFC 6455)
- JSON message formatting and parsing
- Interactive command-line interface
- Username management
- Message logging
- Headless mode for testing
- Multiple connection modes

## Requirements

- Node.js (version 12 or higher)

## Installation

```bash
npm install
```

## Usage

### Basic connection
```bash
node client.js -h localhost
```

### Connection with username
```bash
node client.js -h localhost -n Alice
```

### Headless mode (send a test message and exit)
```bash
node client.js -h localhost -m "Hello, World!"
```

### With chat logging
```bash
node client.js -h localhost -n Bob -s
```

### Remote connection
```bash
node client.js -h your-domain.com
```

## Command-line Options

- `-h <host>` - Server host to connect to (default: localhost)
- `-p <port>` - Server port (defaults: 9999 for localhost, 80 for remote)
- `-n <name>` - Set your username
- `-m <message>` - Send test message and exit (headless mode)
- `-s` - Enable chat logging to `chat_log.log`

## Message Protocol

The client sends messages in JSON format:

```json
{
  "user": {
    "name": "username"
  },
  "message": {
    "text": "message content",
    "text_len": 16,
    "info": 0
  }
}
```

### Message Flags

- `0` - Regular message (broadcast to all)
- `1` - No broadcast (server only)
- `2` - Send back to sender
- `5` - Username change (no broadcast)

## Implementation Details

### Core Components

- **WebSocket Connection**: Uses Node.js net module with WebSocket protocol
- **Frame Encoding**: Implements WebSocket frame encoding with masking
- **JSON Handling**: Native JSON.stringify() for message formatting
- **CLI Interface**: process.stdin for interactive input
- **Event Handling**: Event-driven architecture for message handling

### Key Functions

```javascript
// Create JSON message
function createJsonMessage(username, text, flags = 0)

// Encode WebSocket frame
function encodeFrame(data, opcode = 0x1)

// Decode WebSocket frame
function decodeFrame(buffer)

// Send message
function sendMessage(text, flags = 0)
```

## Interactive Commands

Once connected in interactive mode:

- **Regular message** - Type any text and press Enter
- **/nick <username>** - Change your username
- **Type 'exit' or Ctrl+C** - Exit the client

## Examples

### Local Chat Session

Terminal 1 (server):
```bash
./bin/ws_server
```

Terminal 2 (client):
```bash
node client.js -h localhost -n Alice
```

### Testing with headless mode
```bash
node client.js -h localhost -n TestUser -m "Test message\n"
```

### Chat with logging
```bash
node client.js -h localhost -n Bob -s
# Messages will be saved to chat_log.log
```

## Error Handling

- Connection timeout handling
- WebSocket handshake validation
- Frame decode error recovery
- Graceful shutdown on SIGINT

## Files

- `client.js` - Main client implementation
- `client-raw.js` - Raw message version
- `client-ws.js` - WebSocket library version
- `index.html` - Browser client
- `serve.js` - Simple HTTP server for browser client

## Testing

Works with any WebSocket server that accepts JSON messages:

```bash
# Test with C server
cd ../.. && make c-server && ./bin/ws_server

# Test with Zig server
cd ../.. && make zig-server && ~/zig-server-build/zig-server
```
