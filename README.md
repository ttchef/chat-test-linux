# chat-test-linux

A tripple A 3*A chat application

It supports 4k ui and raytracing

Experience real time chatting with your imaginary friends!

## Building

### Prerequisites
- GCC compiler
- Make

### Setup

1. Create an `ip.h` file in the `src` directory:
```bash
cd src
touch ip.h
```

2. Define your default IP in `ip.h`:
```c
#define IP "localhost"
```
**Note:** This IP is only used when no `-h` flag is provided to the client.

3. Build the project:
```bash
make
```

This will create two executables:
- `ws_server` - WebSocket server
- `ws_client` - WebSocket client

## Local Development

### Starting the Server

The server defaults to binding on `0.0.0.0:9999` (accessible from all interfaces):

```bash
./ws_server
```

Options:
- `-h <host>` - Bind to specific host (default: `0.0.0.0`)
- `-m <message>` - Send test message

Example binding to localhost only:
```bash
./ws_server -h localhost
```

### Connecting a Client

The client automatically selects the correct port based on the host:

**Local connections** (port 9999):
```bash
./ws_client -h localhost
./ws_client -h 127.0.0.1
./ws_client -h 0.0.0.0
```

**Remote connections** (port 80, for Cloudflare tunnels):
```bash
./ws_client -h your-domain.com
```

**Manual port override:**
```bash
./ws_client -h localhost -p 8080
```

Client Options:
- `-h <host>` - Server host to connect to
- `-p <port>` - Server port (defaults: 9999 for localhost, 80 for remote)
- `-n <name>` - Set your username
- `-m <message>` - Send test message and exit (headless mode)
- `-s` - Enable chat logging to `chat_log.log`

## Hosting with Cloudflare Tunnel

### Setup Cloudflare Tunnel

1. Install cloudflared:
```bash
# macOS
brew install cloudflare/cloudflare/cloudflared

# Linux
wget https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb
sudo dpkg -i cloudflared-linux-amd64.deb
```

2. Authenticate:
```bash
cloudflared tunnel login
```

3. Create a tunnel:
```bash
cloudflared tunnel create chat-server
```

4. Create a config file at `~/.cloudflared/config.yml`:
```yaml
tunnel: <your-tunnel-id>
credentials-file: /home/YOUR_USER/.cloudflared/<your-tunnel-id>.json

ingress:
  - hostname: your-domain.com
    service: http://localhost:9999
  - service: http_status:404
```

5. Route your domain:
```bash
cloudflared tunnel route dns chat-server your-domain.com
```

6. Start the tunnel:
```bash
cloudflared tunnel run chat-server
```

### Running the Server with Tunnel

1. Start the server:
```bash
./ws_server
```

2. Start the Cloudflare tunnel (in another terminal):
```bash
cloudflared tunnel run chat-server
```

3. Connect from anywhere:
```bash
./ws_client -h your-domain.com
```

The client will automatically use port 80 for remote hosts.

## Usage Examples

### Local Chat Session

Terminal 1 (server):
```bash
./ws_server
```

Terminal 2 (client 1):
```bash
./ws_client -h localhost -n Alice
```

Terminal 3 (client 2):
```bash
./ws_client -h localhost -n Bob
```

### Remote Chat via Tunnel

```bash
./ws_client -h your-domain.com -n Alice
```

### Headless Test

Send a message and exit:
```bash
./ws_client -h localhost -n TestBot -m "Hello World!\n"
```

### Chat with Logging

```bash
./ws_client -h localhost -n Alice -s
```

This saves all received messages to `chat_log.log`.

## Connection Timeout

The client has a 10-second connection timeout. If the server is unreachable, you'll see:
```
Connection timeout to <host>:<port>
```

This prevents the client from hanging indefinitely on bad connections.

## Port Summary

| Connection Type | Default Port | Override |
|----------------|--------------|----------|
| localhost/127.0.0.1/0.0.0.0 | 9999 | `-p <port>` |
| Remote hosts | 80 | `-p <port>` |
| Server binding | 9999 | N/A |
