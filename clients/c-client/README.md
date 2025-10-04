# C WebSocket Client

A lightweight WebSocket client implementation in C with JSON message support for connecting to WebSocket servers.

## Features

- WebSocket protocol implementation (RFC 6455)
- JSON message formatting and parsing
- Non-blocking I/O with poll()
- Automatic reconnection handling
- Message broadcasting and private messaging
- Username management
- Interactive chat mode
- Test mode for automated messaging

## Building

```bash
# From project root
make ws_client

# Build test client
make ws_client_test

# The binaries will be in bin/
```

## Usage

### Interactive Client

```bash
# Connect to default server (127.0.0.1:9999)
./bin/ws_client

# Connect to custom server
./bin/ws_client 192.168.1.100 8080 MyUsername
```

### Test Client

```bash
# Send a single test message
./bin/ws_client_test
```

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

## Commands

Once connected, you can use these commands:

- **Regular message** - Type any text and press Enter
- **/nick <username>** - Change your username
- **/quit** - Exit the client
- **/help** - Show available commands

## Implementation Details

### Key Components

- **WebSocket Handshake**: HTTP upgrade request with proper headers
- **Frame Encoding**: WebSocket frame construction with masking
- **JSON Formatting**: Message serialization to JSON
- **Non-blocking I/O**: Uses poll() for responsive input/output

### Data Structures

```c
#define BUFFER_SIZE 4096
#define WS_MASK_KEY_SIZE 4

// Message flags
#define WS_NO_BROADCAST (1 << 0)
#define WS_SEND_BACK (1 << 1)
#define WS_CHANGE_USERNAME (1 << 2)
```

### Network Flow

1. **Connect**: TCP socket connection to server
2. **Handshake**: WebSocket upgrade request
3. **Communication**: Send/receive JSON messages
4. **Frame Parsing**: Decode incoming WebSocket frames
5. **Display**: Show messages in chat format

## Example Session

```
$ ./bin/ws_client 127.0.0.1 9999 TestUser
Connected to 127.0.0.1:9999
TestUser joined the chat!
Hello everyone!
OtherUser: Hi there!
/nick NewName
Username changed to NewName
/quit
Disconnected.
```

## Dependencies

- Standard C libraries (stdio, stdlib, string, unistd, etc.)
- POSIX sockets (sys/socket.h, netdb.h)
- Poll for multiplexing (poll.h)

## Testing

The client works with any of the server implementations:

- C server (servers/c-server/)
- Zig server (servers/zig-server-damon/)
- Any RFC 6455 compliant WebSocket server

## Error Handling

- Connection failures with retry attempts
- WebSocket handshake validation
- Frame decode error handling
- Graceful shutdown on signals