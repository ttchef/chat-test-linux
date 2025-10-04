const WebSocket = require('ws');
const crypto = require('crypto');

const MAX_CLIENTS = 10;
const PORT = 9999;
const HOST = process.argv.includes('-h')
  ? process.argv[process.argv.indexOf('-h') + 1]
  : '0.0.0.0';

// Message flags
const WS_NO_BROADCAST = 1 << 0;
const WS_SEND_BACK = 1 << 1;
const WS_CHANGE_USERNAME = 1 << 2;

class Client {
  constructor(id, ws) {
    this.id = id;
    this.ws = ws;
    this.username = 'Anonym';
  }
}

const clients = [];
let nextClientId = 1;

const wss = new WebSocket.Server({
  host: HOST,
  port: PORT,
  maxPayload: 4096
});

console.log(`WebSocket server listening on ${HOST}:${PORT}`);

wss.on('connection', (ws, req) => {
  if (clients.length >= MAX_CLIENTS) {
    console.log('Max clients reached, rejecting connection');
    ws.close();
    return;
  }

  const clientId = nextClientId++;
  const client = new Client(clientId, ws);
  clients.push(client);

  const clientIndex = clients.length - 1;
  console.log(`Client connected (id=${clientId}, slot=${clientIndex})`);

  ws.on('message', (data) => {
    try {
      const payload = data.toString();
      console.log(`Server received Message: ${payload}`);

      const message = JSON.parse(payload);

      if (!message.user || !message.message) {
        console.log('Invalid message format');
        return;
      }

      const { user, message: msg } = message;
      const flags = msg.info || 0;

      // Handle username change
      if (flags & WS_CHANGE_USERNAME) {
        console.log('Change username message detected!');
        client.username = user.name;
        console.log(`Updated client: ${client.id} name to: ${client.username}`);
      }

      // Don't broadcast if NO_BROADCAST flag is set
      if (flags & WS_NO_BROADCAST) {
        return;
      }

      // Update username in the message
      message.user.name = client.username;

      // Clear info flags for broadcast
      message.message.info = 0;

      const broadcastPayload = JSON.stringify(message);

      // Broadcast to all clients
      clients.forEach((c, idx) => {
        // Skip sender unless SEND_BACK flag is set
        if (c.id === client.id && !(flags & WS_SEND_BACK)) {
          return;
        }

        if (c.ws.readyState === WebSocket.OPEN) {
          try {
            c.ws.send(broadcastPayload);
          } catch (err) {
            console.log(`Failed to send to client (id=${c.id}), will be disconnected`);
          }
        }
      });

    } catch (err) {
      console.error('Error processing message:', err);
    }
  });

  ws.on('close', () => {
    const idx = clients.findIndex(c => c.id === client.id);
    if (idx !== -1) {
      clients.splice(idx, 1);
      console.log(`Client disconnected (id=${client.id})`);
    }
  });

  ws.on('error', (err) => {
    console.error(`Client error (id=${client.id}):`, err.message);
  });
});

wss.on('error', (err) => {
  console.error('Server error:', err);
});

// Handle graceful shutdown
process.on('SIGINT', () => {
  console.log('\nShutting down server...');
  wss.close(() => {
    process.exit(0);
  });
});

process.on('SIGTERM', () => {
  console.log('\nShutting down server...');
  wss.close(() => {
    process.exit(0);
  });
});
