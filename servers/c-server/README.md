# C WebSocket Server

A high-performance WebSocket server implementation in C with JSON message parsing and broadcasting capabilities.

## Features

- WebSocket protocol implementation (RFC 6455)
- JSON message parsing and formatting
- Client connection management
- Message broadcasting and routing
- Username management
- Message flags for special behaviors
- Debug and error logging
- Non-blocking I/O with poll()

## Building

```bash
# From project root
make c-server

# Or directly in this directory
make
```

## Usage

```bash
# Run the server on default port 9999
./bin/ws_server

# Run on custom port
./bin/ws_server 8080
```

## Message Protocol

The server expects JSON messages in the following format:

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

### Message Flags (info field)

- `0` - Regular message (broadcast to all clients)
- `1` (WS_NO_BROADCAST) - Don't broadcast this message
- `2` (WS_SEND_BACK) - Send message only back to sender
- `5` (WS_CHANGE_USERNAME | WS_NO_BROADCAST) - Change username

## Dependencies

- `ws_json.c` - JSON parsing library
- `ws_client_lib.c` - WebSocket frame utilities
- `ws_defines.h` - Common definitions
- `ws_json.h` - JSON API
- `ws_globals.h` - Global constants

## Architecture

- **Main loop**: Uses `poll()` for non-blocking I/O
- **Frame parsing**: Handles WebSocket frame encoding/decoding
- **JSON processing**: Parses incoming messages and builds responses
- **Client management**: Tracks connected clients with file descriptors
- **Message routing**: Broadcasts messages based on flags

## Testing

Use any of the provided clients to test the server:

```bash
# C client
./bin/ws_client

# JavaScript client
cd clients/js-client && node client.js

# Python client
cd clients/python-client && python3 ws_client.py

# C# client (Windows)
cd clients/Windows-Client && dotnet run
```