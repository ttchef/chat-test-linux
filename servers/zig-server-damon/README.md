# Zig WebSocket Server

A modern WebSocket server implementation in Zig with JSON message parsing, leveraging C libraries for compatibility.

## Features

- WebSocket protocol implementation (RFC 6455)
- JSON message parsing via C library bindings
- Client connection management
- Message broadcasting and routing
- Username management
- Message flags for special behaviors
- Memory-safe implementation with Zig
- FFI bindings to C JSON parser

## Building

```bash
# From project root (builds to ~/zig-server-build/)
make zig-server

# Or manually:
cd servers/zig-server-damon
zig build
```

## Usage

```bash
# Run the server
~/zig-server-build/zig-server

# The server will listen on 0.0.0.0:9999
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

## Architecture

### Zig Components

- **server.zig**: Main server implementation
- **json_bindings.zig**: FFI bindings to C JSON library
- **build.zig**: Build configuration with C library linking

### C Dependencies

- `../../lib/ws_json.c` - JSON parsing library
- `../../lib/ws_client_lib.c` - WebSocket frame utilities
- `../../lib/ws_defines.h` - Common definitions

### Key Features

- **Memory Safety**: Zig's memory management prevents buffer overflows
- **FFI Integration**: Seamless integration with existing C libraries
- **JSON Parsing**: Uses proven C JSON parser via bindings
- **Non-blocking I/O**: Efficient event-driven architecture

## JSON Bindings

The server uses `json_bindings.zig` to interface with the C JSON library:

```zig
// Parse JSON from string slice
const root = json.parseJson(json_str);

// Get JSON objects
const user_obj = json.getObject(root, "user");
const message_obj = json.getObject(root, "message");

// Extract values
const username = json.getString(user_obj, "name");
const info = json.getNumber(message_obj, "info");
```

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

## Build System

The `build.zig` file handles:
- C source file compilation
- Library linking
- Include paths
- Compiler flags for debug logging

## Advantages

- **Type Safety**: Zig's strong type system catches errors at compile time
- **Performance**: Optimized WebSocket frame handling
- **Interoperability**: Maintains compatibility with existing C clients
- **Modern Language**: Features like explicit error handling, comptime, etc.