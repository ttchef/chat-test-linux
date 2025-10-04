# Python WebSocket Client

A Python WebSocket client with JSON message support using ctypes to interface with the C WebSocket library.

## Features

- WebSocket protocol implementation via C library
- JSON message formatting and parsing
- Python ctypes interface to C shared library
- Command-line interface with argparse
- Callback-based message handling
- Thread-safe operation
- Interactive and test modes

## Requirements

- Python 3.6 or higher
- Compiled C shared library (libwsclient.so)

## Installation

The client requires the C shared library to be built first:

```bash
# From project root
make

# This builds bin/libwsclient.so which the Python client uses
```

## Usage

### Interactive Mode

```bash
python3 ws_client.py
```

Or with options:

```bash
python3 ws_client.py --host 127.0.0.1 --port 9999 --name YourName
```

### Test Mode (Send single message)

```bash
python3 ws_client.py --message "Hello from Python!" --name TestUser
```

## Command Line Options

- `--host <hostname>` - Server host (default: 127.0.0.1)
- `-p, --port <port>` - Server port (default: 9999)
- `-n, --name <username>` - Username (default: PythonUser)
- `-m, --message <message>` - Send message and exit (test mode)
- `--help` - Show help message

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

## API Usage

```python
from ws_client import WSClient
import time

# Create client
client = WSClient(ip="127.0.0.1", port="9999", username="MyUser")

# Define message handler
def handle_message(client, message, timestamp):
    print(f"Received: {message}")

# Set callback (raw mode)
client.set_message_callback(handle_message, use_json=False)

# Connect
if client.connect():
    # Send messages
    client.send_message("Hello, world!")

    # Listen for responses briefly
    for _ in range(5):
        client.listen()

    # Or run continuously (blocking)
    # client.run()

    # Clean up
    client.disconnect()
```

## Implementation Details

### Architecture

- **C Library Interface**: Uses ctypes to load and call C functions
- **Shared Library**: Loads `../../bin/libwsclient.so`
- **Callback Handling**: Python callbacks for incoming messages
- **JSON Handling**: Python's built-in json module
- **Threading**: Background thread for message listening

### Core Classes

```python
class WSClient:
    def __init__(self, ip="127.0.0.1", port="9999", username="PythonUser")
    def connect(self)
    def send_message(self, message)
    def set_message_callback(self, callback, use_json=False)
    def listen(self)
    def disconnect(self)
    def run(self)
```

### C Function Bindings

The client interfaces with these C functions:

```c
int32_t wsInitClient(wsClient* client, const char* ip, const char* port, const char* username);
int32_t wsSendMessage(wsClient* client, const char* message);
int32_t wsSetOnMessageCallback(wsClient* client, void* functionPtr, int32_t type);
int32_t wsClientListen(wsClient* client);
int32_t wsDeinitClient(wsClient* client);
```

### Callback Types

- **JSON Callback**: `(client, json_ptr, timestamp)`
- **Raw Callback**: `(client, message_string, timestamp)`

## Dependencies

- **Python Standard Library**:
  - ctypes (for C library interface)
  - json (for message formatting)
  - threading (for background operations)
  - argparse (for command line parsing)
  - time, os, sys

- **C Shared Library**:
  - `../../bin/libwsclient.so` (built from project source)

## Error Handling

- Library loading validation
- Connection error handling
- Message send failure detection
- Graceful shutdown on interrupts
- Callback error isolation

## Testing

```bash
# Test with C server
cd ../.. && make c-server && ./bin/ws_server &
cd clients/python-client && python3 ws_client.py -m "Test message"

# Test with Zig server
cd ../.. && make zig-server && ~/zig-server-build/zig-server &
cd clients/python-client && python3 ws_client.py -n TestUser
```

## Troubleshooting

### Library Not Found
```
Error: libwsclient.so not found at ../../bin/libwsclient.so
```
**Solution**: Run `make` from project root to build the shared library

### Connection Failed
```
Failed to connect to 127.0.0.1:9999
```
**Solution**: Ensure the WebSocket server is running and accessible

### Callback Errors
If a callback function raises an exception, it will be caught and logged without crashing the client.

## Thread Safety

- The client uses a background thread for message listening
- Send operations are thread-safe
- Callbacks should be non-blocking to avoid message delays

## Files

- `ws_client.py` - Main client implementation
- `example.py` - Simple usage example
- `__init__.py` - Python package initialization
