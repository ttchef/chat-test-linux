# Windows C# WebSocket Client

A C# WebSocket client implementation for Windows with JSON message support and interactive chat interface.

## Features

- WebSocket protocol implementation (RFC 6455)
- JSON message formatting using System.Text.Json
- Interactive console interface
- Username management
- Message history
- Asynchronous operations
- Cross-platform support (Windows, Linux, macOS)

## Requirements

- .NET 6.0 or higher
- Compatible runtime: Windows, Linux, or macOS

## Installation

The client is a .NET console application. No additional packages are required beyond the default dependencies.

## Building

```bash
# From the client directory
dotnet build

# Or from project root
cd clients/Windows-Client
dotnet build
```

## Usage

### Command Line

```bash
# Run with default settings (127.0.0.1:9999)
dotnet run

# Connect with custom username
dotnet run -- --username Alice

# Connect to custom server
dotnet run -- --host 192.168.1.100 --port 8080

# Send test message and exit
dotnet run -- --username TestUser --message "Hello from C#!"
```

### Command Line Options

- `--host <hostname>` - Server host (default: 127.0.0.1)
- `--port <port>` - Server port (default: 9999)
- `--username <name>` - Username (default: CSharpUser)
- `--message <text>` - Send message and exit (test mode)
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

## Implementation Details

### Core Classes

```csharp
class Program
{
    // Main entry point
    static async Task Main(string[] args)

    // Connection management
    static async Task ConnectAsync(string host, int port)
    static async Task DisconnectAsync()

    // Message handling
    static async Task SendMessageAsync(string message)
    static async Task ReceiveMessagesAsync()
    static void ProcessReceivedMessage(string message)

    // User interaction
    static void ShowHelp()
    static string CreateJsonMessage(string username, string text, int flags = 0)
}
```

### Data Structures

```csharp
// Message flags
class MessageFlags
{
    public const int WS_NO_BROADCAST = 1 << 0;
    public const int WS_SEND_BACK = 1 << 1;
    public const int WS_CHANGE_USERNAME = 1 << 2;
}

// JSON message structure
class WSMessage
{
    public User user { get; set; } = new User();
    public Message message { get; set; } = new Message();
}

class User
{
    public string name { get; set; } = "";
}

class Message
{
    public string text { get; set; } = "";
    public int text_len { get; set; }
    public int info { get; set; }
}
```

### WebSocket Implementation

The client implements WebSocket protocol from scratch:
- **HTTP Upgrade**: Sends proper WebSocket handshake
- **Frame Encoding**: Constructs WebSocket frames with masking
- **JSON Serialization**: Uses System.Text.Json for message formatting
- **Async/Await**: Non-blocking I/O for responsive UI

### Key Features

- **Manual WebSocket**: No external WebSocket library dependencies
- **Frame Parsing**: Handles binary frame encoding/decoding
- **Masking**: Implements client-side masking as required by protocol
- **Async Operations**: Uses Task-based async pattern

## Interactive Commands

Once connected:

- **Regular message** - Type any text and press Enter
- **/nick <username>** - Change your username
- **/quit** - Exit the client
- **/help** - Show available commands
- **/clear** - Clear the screen (Windows only)

## Example Session

```
> dotnet run --username Alice
Connecting to 127.0.0.1:9999...
Connected as Alice
Hello everyone!
[Bob] Hi Alice!
How are you doing?
[Bob] Great! How about you?
> /nick Alice2
Username changed to Alice2
> /quit
Disconnecting...
```

## Testing

Works with any WebSocket server that accepts JSON messages:

```bash
# Test with C server (on Windows)
cd ../.. && make c-server && ./bin/ws_server.exe

# Test with Zig server
cd ../.. && make zig-server && ~/zig-server-build/zig-server

# Test from other clients
# The C# client can connect to the same server as other clients
```

## Cross-Platform Support

While designed for Windows, this client works on:
- **Windows** - Native console application
- **Linux** - Via .NET runtime
- **macOS** - Via .NET runtime

## Error Handling

- Connection timeout handling
- WebSocket handshake validation
- Network error recovery
- Graceful shutdown on Ctrl+C
- JSON serialization error handling

## Project Structure

```
Windows-Client/
├── Program.cs           # Main client implementation
├── Windows-Client.csproj # Project configuration
├── bin/                 # Build output directory
├── obj/                 # Build intermediate files
└── README.md           # This file
```

## Dependencies

- **.NET 6.0** - Runtime framework
- **System.Text.Json** - JSON serialization (built-in)
- **System.Net.Sockets** - Network communication (built-in)
- **System.Console** - Console I/O (built-in)

## Performance

- **Memory Efficient**: Uses spans and memory pooling
- **Async I/O**: Non-blocking network operations
- **Minimal Dependencies**: No external NuGet packages required