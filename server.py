#!/usr/bin/env python3
import asyncio
import websockets
import sys
import signal

connected_clients = set()
test_message = None

async def chat_handler(websocket, path):
    # Register client
    connected_clients.add(websocket)
    print(f"Client connected. Total clients: {len(connected_clients)}")

    # Send test message if in headless mode
    if test_message:
        await websocket.send(test_message)
        print(f"Sent: {test_message.strip()}")

    try:
        # Handle incoming messages
        async for message in websocket:
            print(f"Received: {message.strip()}")

            # Broadcast to all other clients
            for client in connected_clients:
                if client != websocket:
                    try:
                        await client.send(message)
                    except:
                        pass

            # Send response if in headless mode
            if test_message:
                await websocket.send(test_message)
                print(f"Sent: {test_message.strip()}")
    except websockets.exceptions.ConnectionClosed:
        print("Client disconnected")
    finally:
        connected_clients.remove(websocket)
        print(f"Total clients: {len(connected_clients)}")

async def main():
    global test_message

    # Parse command line arguments
    if len(sys.argv) > 2 and sys.argv[1] == "-m":
        test_message = sys.argv[2]

    # Start WebSocket server on port 9999
    print("Server listening on 0.0.0.0:9999")
    async with websockets.serve(chat_handler, "0.0.0.0", 9999):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped")