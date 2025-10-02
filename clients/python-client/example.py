#!/usr/bin/env python3
"""
Simple example demonstrating the Python WebSocket client
"""

from ws_client import WSClient
import time

def main():
    # Create a client instance
    client = WSClient(
        ip="127.0.0.1",
        port="9999",
        username="PythonBot"
    )

    # Define a callback for incoming messages
    def on_message(client, message, username, timestamp):
        print(f"Received: {message}", end='')

    # Set the message callback
    if not client.set_message_callback(on_message):
        print("Failed to set message callback")
        return 1

    # Connect to the server
    print("Connecting to server...")
    if not client.connect():
        print("Failed to connect to server")
        return 1

    print("Connected! Sending messages...")

    # Send some test messages
    messages = [
        "Hello from Python!",
        "This is a test message",
        "WebSocket client working great!"
    ]

    for msg in messages:
        client.send_message(msg)
        print(f"Sent: {msg}")
        time.sleep(1)

        # Listen for server responses
        for _ in range(3):
            client.listen()

    # Clean disconnect
    print("\nDisconnecting...")
    client.disconnect()
    print("Done!")

    return 0


if __name__ == '__main__':
    exit(main())
