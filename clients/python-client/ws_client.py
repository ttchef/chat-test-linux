#!/usr/bin/env python3

import ctypes
import os
import sys
from ctypes import c_int32, c_char_p, c_void_p, c_size_t, CFUNCTYPE, POINTER, Structure
import time

# Define the callback function types
# typedef void (*wsOnMessageCallbackJsonPFN)(wsClient* client, time_t time, wsJson* root);
# typedef void (*wsOnMessageCallbackRawPFN)(wsClient* client, time_t time, const char* message);
MessageCallbackJsonType = CFUNCTYPE(None, c_void_p, c_int32, c_void_p)
MessageCallbackRawType = CFUNCTYPE(None, c_void_p, c_int32, c_char_p)

# Callback types
WS_MESSAGE_CALLBACK_JSON = 0
WS_MESSAGE_CALLBACK_RAW = 1

# Load the shared library
lib_path = os.path.join(os.path.dirname(__file__), '../../bin/libwsclient.so')
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

# int32_t wsSetOnMessageCallback(wsClient* client, wsOnMessageCallbackPFN functionPtr, wsOnMessageCallbackType type);
lib.wsSetOnMessageCallback.argtypes = [c_void_p, c_void_p, c_int32]
lib.wsSetOnMessageCallback.restype = c_int32

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
        """Send a message to the server in JSON format

        Args:
            message: Message string to send

        Returns:
            True on success, False on failure
        """
        import json
        # Build JSON message matching the expected format
        json_msg = json.dumps({
            "user": {"name": self.username.decode('utf-8')},
            "message": {
                "text": message if isinstance(message, str) else message.decode('utf-8'),
                "text_len": len(message if isinstance(message, str) else message.decode('utf-8')),
                "info": 0
            }
        })

        if isinstance(json_msg, str):
            json_msg = json_msg.encode('utf-8')
        result = lib.wsSendMessage(ctypes.byref(self.client), json_msg)
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

    def set_message_callback(self, callback, use_json=False):
        """Set callback function for incoming messages

        Args:
            callback: Function with signature (client, message, timestamp) for raw mode
                     or (client, json_ptr, timestamp) for JSON mode
            use_json: If True, use JSON callback mode

        Returns:
            True on success, False on failure
        """
        if use_json:
            def wrapper(client_ptr, timestamp, json_ptr):
                # For JSON mode, just pass the pointer - user must handle it
                callback(self, json_ptr, timestamp)

            self.callback = MessageCallbackJsonType(wrapper)
            callback_type = WS_MESSAGE_CALLBACK_JSON
        else:
            def wrapper(client_ptr, timestamp, message):
                msg_str = message.decode('utf-8') if message else ""
                callback(self, msg_str, timestamp)

            self.callback = MessageCallbackRawType(wrapper)
            callback_type = WS_MESSAGE_CALLBACK_RAW

        # Keep reference to prevent garbage collection
        result = lib.wsSetOnMessageCallback(
            ctypes.byref(self.client),
            ctypes.cast(self.callback, c_void_p),
            callback_type
        )
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

    # Define message callback (raw mode)
    def on_message(client, message, timestamp):
        print(f"{message}", end='')
        sys.stdout.flush()

    # Set callback (use raw mode for simplicity)
    if not client.set_message_callback(on_message, use_json=False):
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
