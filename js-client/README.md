# JavaScript WebSocket Chat Client

A Node.js WebSocket client for the chat application.

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

## Examples

### Local Chat Session

Terminal 1 (server):
```bash
./ws_server
```

Terminal 2 (client):
```bash
node client.js -h localhost -n Alice
```

### Testing with headless mode
```bash
node client.js -h localhost -n TestUser -m "Test message\n"
```
