#!/usr/bin/env python3

import ctypes
import os
import sys
from ctypes import c_int32, c_char_p, c_void_p, c_size_t, CFUNCTYPE, POINTER, Structure
import time

# Define the callback function type
# typedef void (*wsMessageCallbackPFN)(wsClient* client, const char* message, const char* username, time_t time);
MessageCallbackType = CFUNCTYPE(None, c_void_p, c_char_p, c_char_p, c_int32)

# Load the shared library
lib_path = os.path.join(os.path.dirname(__file__), '../../libwsclient.so')
if not os.path.exists(lib_path):
    print(f"Error: libwsclient.so not found at {lib_path}")
    sys.exit(1)

lib = ctypes.CDLL(lib_path)

# Define function signatures
# int32_t wsInitClient(wsClient* client, const char* ip, const char* port, const char* username);
lib.wsInitClient.argtypes = [c_void_p, c_char_p, c_char_p, c_char_p]
lib.wsInitClient.restype = c_int32

# int32_t wsSendMessage(wsClient* client, const char* message);
lib.wsSendMessage.argtypes = [c_void_p, c_char_p]
lib.wsSendMessage.restype = c_int32

# int32_t wsSendMessageN(wsClient* client, const char* message, size_t n);
lib.wsSendMessageN.argtypes = [c_void_p, c_char_p, c_size_t]
lib.wsSendMessageN.restype = c_int32

# int32_t wsSetMessageCallback(wsClient* client, wsMessageCallbackPFN functionPtr);
lib.wsSetMessageCallback.argtypes = [c_void_p, MessageCallbackType]
lib.wsSetMessageCallback.restype = c_int32

# int32_t wsClientListen(wsClient* client);
lib.wsClientListen.argtypes = [c_void_p]
lib.wsClientListen.restype = c_int32

# int32_t wsDeinitClient(wsClient* client);
lib.wsDeinitClient.argtypes = [c_void_p]
lib.wsDeinitClient.restype = c_int32

# Constants
WS_ERROR = -1
WS_OK = 0

class WSClient:
    """Python wrapper for WebSocket client library"""

    def __init__(self, ip="127.0.0.1", port="9999", username="PythonUser"):
        """Initialize WebSocket client

        Args:
            ip: Server IP address or hostname
            port: Server port
            username: Username for this client
        """
        # Allocate memory for wsClient struct (approximately 256 bytes to be safe)
        self.client = ctypes.create_string_buffer(256)
        self.callback = None
        self.ip = ip.encode('utf-8')
        self.port = port.encode('utf-8')
        self.username = username.encode('utf-8')

    def connect(self):
        """Connect to the WebSocket server

        Returns:
            True on success, False on failure
        """
        result = lib.wsInitClient(
            ctypes.byref(self.client),
            self.ip,
            self.port,
            self.username
        )
        return result == WS_OK

    def send_message(self, message):
        """Send a message to the server

        Args:
            message: Message string to send

        Returns:
            True on success, False on failure
        """
        if isinstance(message, str):
            message = message.encode('utf-8')
        result = lib.wsSendMessage(ctypes.byref(self.client), message)
        return result == WS_OK

    def send_message_n(self, message, n):
        """Send first n bytes of a message to the server

        Args:
            message: Message string to send
            n: Number of bytes to send

        Returns:
            True on success, False on failure
        """
        if isinstance(message, str):
            message = message.encode('utf-8')
        result = lib.wsSendMessageN(ctypes.byref(self.client), message, n)
        return result == WS_OK

    def set_message_callback(self, callback):
        """Set callback function for incoming messages

        Args:
            callback: Function with signature (client, message, username, timestamp)

        Returns:
            True on success, False on failure
        """
        def wrapper(client_ptr, message, username, timestamp):
            msg_str = message.decode('utf-8') if message else ""
            user_str = username.decode('utf-8') if username else ""
            callback(self, msg_str, user_str, timestamp)

        # Keep reference to prevent garbage collection
        self.callback = MessageCallbackType(wrapper)
        result = lib.wsSetMessageCallback(ctypes.byref(self.client), self.callback)
        return result == WS_OK

    def listen(self):
        """Listen for incoming messages (blocking call with timeout)

        Returns:
            True on success, False on error
        """
        result = lib.wsClientListen(ctypes.byref(self.client))
        return result == WS_OK

    def disconnect(self):
        """Disconnect from the server

        Returns:
            True on success, False on failure
        """
        result = lib.wsDeinitClient(ctypes.byref(self.client))
        return result == WS_OK

    def run(self):
        """Run the client in a loop, listening for messages"""
        try:
            while True:
                if not self.listen():
                    print("Listen failed or timeout")
                    break
        except KeyboardInterrupt:
            print("\nShutting down...")
        finally:
            self.disconnect()


def main():
    """Example usage of the WebSocket client"""
    import argparse

    parser = argparse.ArgumentParser(description='WebSocket Python Client')
    parser.add_argument('--host', default='127.0.0.1', help='Server host (default: 127.0.0.1)')
    parser.add_argument('-p', '--port', default='9999', help='Server port (default: 9999)')
    parser.add_argument('-n', '--name', default='PythonUser', help='Username (default: PythonUser)')
    parser.add_argument('-m', '--message', help='Send message and exit (test mode)')

    args = parser.parse_args()

    # Create client instance
    client = WSClient(ip=args.host, port=args.port, username=args.name)

    # Define message callback
    def on_message(client, message, username, timestamp):
        print(f"{message}", end='')
        sys.stdout.flush()

    # Set callback
    if not client.set_message_callback(on_message):
        print("Failed to set message callback")
        return 1

    # Connect to server
    if not client.connect():
        print(f"Failed to connect to {args.host}:{args.port}")
        return 1

    print(f"Connected to {args.host}:{args.port}")

    # If test message provided, send and exit
    if args.message:
        client.send_message(args.message)
        print(f"Sent: {args.message}")
        # Listen for response briefly
        for _ in range(5):
            client.listen()
        client.disconnect()
        return 0

    # Interactive mode
    print("Type messages and press Enter to send. Ctrl+C to exit.")

    # Run in background thread for receiving
    import threading

    def listen_thread():
        client.run()

    listener = threading.Thread(target=listen_thread, daemon=True)
    listener.start()

    # Main thread handles user input
    try:
        while True:
            message = input()
            if message:
                client.send_message(message + '\n')
    except KeyboardInterrupt:
        print("\nDisconnecting...")
    except EOFError:
        pass
    finally:
        client.disconnect()

    return 0


if __name__ == '__main__':
    sys.exit(main())
