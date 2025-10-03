# Python WebSocket Client

A Python client for the WebSocket chat server using ctypes bindings to the C library.

## Requirements

- Python 3.6+
- libwsclient.so (built from lib/ws_client_lib.c)

## Building the Library

From the project root directory:

```bash
gcc -shared -fPIC -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR -o libwsclient.so lib/ws_client_lib.c
```

## Usage

### Interactive Mode

```bash
./clients/python-client/ws_client.py
```

Or with options:

```bash
./clients/python-client/ws_client.py --host 127.0.0.1 --port 9999 --name YourName
```

### Test Mode (Send single message)

```bash
./clients/python-client/ws_client.py --message "Hello from Python!" --name TestUser
```

## Command Line Options

- `--host`: Server hostname or IP (default: 127.0.0.1)
- `-p` / `--port`: Server port (default: 9999)
- `-n` / `--name`: Username (default: PythonUser)
- `-m` / `--message`: Send a single message and exit

## API Usage

```python
from ws_client import WSClient

# Create client
client = WSClient(ip="127.0.0.1", port="9999", username="MyUser")

# Define message handler
def handle_message(client, message, username, timestamp):
    print(f"[{username}] {message}")

# Set callback
client.set_message_callback(handle_message)

# Connect
if client.connect():
    # Send messages
    client.send_message("Hello, world!")

    # Listen for responses
    client.run()  # Runs until interrupted

    # Or manually listen
    while True:
        client.listen()
```

## Features

- Full ctypes bindings to the C WebSocket library
- Support for message callbacks
- Interactive and programmatic usage
- Connection timeout handling
- Debug logging support
