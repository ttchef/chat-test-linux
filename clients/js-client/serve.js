#!/usr/bin/env node
import { createServer } from 'http';
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const server = createServer((req, res) => {
    if (req.url === '/' || req.url === '/index.html') {
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(readFileSync(join(__dirname, 'index.html')));
    } else {
        res.writeHead(404);
        res.end('Not found');
    }
});

server.listen(8080, () => {
    console.log('HTTP server running at http://localhost:8080/');
    console.log('Open this URL in your browser to use the WebSocket client');
});
