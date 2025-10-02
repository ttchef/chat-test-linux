using System;
using System.IO;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

class Program
{
    private const int BUFFER_SIZE = 4096;

    static async Task Main(string[] args)
    {
        // Parse command-line arguments (mimicking client.c behavior from lines 212-256)
        string? testMsg = null;
        bool headless = false;
        bool chatLog = false;
        string logFileName = "chat_log.log";
        string? name = null;
        bool nameType = false;
        string? host = null;
        string? port = null;
        bool portSpecified = false;
        bool hostSpecified = false;

        for (int i = 0; i < args.Length; i++)
        {
            // -m flag: test message for headless mode
            if (args[i] == "-m" && i + 1 < args.Length)
            {
                testMsg = args[++i];
                headless = true;
            }
            // -n flag: username/display name
            else if (args[i] == "-n" && i + 1 < args.Length)
            {
                nameType = true;
                name = args[++i];
            }
            // -s flag: enable chat logging
            else if (args[i] == "-s")
            {
                Console.WriteLine("Running client with logging");
                chatLog = true;
            }
            // -h flag: server host
            else if (args[i] == "-h" && i + 1 < args.Length)
            {
                host = args[++i];
                hostSpecified = true;
            }
            // -p flag: server port
            else if (args[i] == "-p" && i + 1 < args.Length)
            {
                port = args[++i];
                portSpecified = true;
            }
        }

        // Set default host if not specified (mimicking lines 259-262)
        if (!hostSpecified)
        {
            host = "127.0.0.1"; // Default to localhost (mimicking IP from ip.h)
        }

        // Set default port based on host if not specified (mimicking lines 265-276)
        if (!portSpecified)
        {
            if (host == "localhost" || host == "127.0.0.1" || host == "0.0.0.0")
            {
                port = "9999"; // Use port 9999 for local connections
            }
            else
            {
                port = "80"; // Use port 80 for remote connections (Cloudflare tunnels)
            }
        }

        // Create log file if chat logging is enabled (mimicking lines 279-292)
        if (chatLog)
        {
            try
            {
                File.WriteAllText(logFileName, "");
                Console.WriteLine("Created file");
            }
            catch (Exception)
            {
                Console.Error.WriteLine($"Failed to create {logFileName} file try running without -s flag!");
                return;
            }
        }

        // Build WebSocket URI (ws:// for unencrypted connections)
        Uri serverUri = new Uri($"ws://{host}:{port}");

        using (ClientWebSocket ws = new ClientWebSocket())
        {
            // Disable server certificate validation for development (allows self-signed certs)
            ws.Options.RemoteCertificateValidationCallback = (sender, certificate, chain, sslPolicyErrors) => true;

            try
            {
                // Connection with timeout (mimicking lines 318-376)
                Console.WriteLine($"Connecting to server at {host}:{port}");

                var connectTask = ws.ConnectAsync(serverUri, CancellationToken.None);
                var timeoutTask = Task.Delay(10000); // 10 second timeout

                if (await Task.WhenAny(connectTask, timeoutTask) == timeoutTask)
                {
                    Console.Error.WriteLine($"Connection timeout to {host}:{port}");
                    return;
                }

                await connectTask; // Ensure connection completed successfully

                Console.WriteLine($"Connected to server at {host}:{port}");
                Console.WriteLine("WebSocket handshake complete");

                // Send username if provided (mimicking lines 394-407)
                if (nameType && !string.IsNullOrEmpty(name))
                {
                    string idMsg = $"[ID]{name}";
                    byte[] idBuffer = Encoding.UTF8.GetBytes(idMsg);
                    await ws.SendAsync(new ArraySegment<byte>(idBuffer), WebSocketMessageType.Text, true, CancellationToken.None);
                }

                // Send test message if in headless mode (mimicking lines 410-420)
                if (headless && !string.IsNullOrEmpty(testMsg))
                {
                    byte[] msgBuffer = Encoding.UTF8.GetBytes(testMsg);
                    await ws.SendAsync(new ArraySegment<byte>(msgBuffer), WebSocketMessageType.Text, true, CancellationToken.None);
                    Console.Write($"Sent: {testMsg}");
                    Console.Out.Flush();
                }

                // Start listening in background
                var receivedResponse = false;
                var cts = new CancellationTokenSource();
                var listeningTask = ReceiveMessages(ws, chatLog, logFileName, () => receivedResponse = true, cts.Token);

                // Create a single input task that will be reused
                Task<string?>? inputTask = null;

                // Main client loop (mimicking lines 432-509)
                while (true)
                {
                    // Set timeout based on mode (line 436)
                    int timeout = headless ? 5000 : 50000;

                    // Create input task only if we don't have one already
                    if (inputTask == null)
                    {
                        inputTask = Task.Run(() => Console.ReadLine());
                    }

                    // Wait for input or timeout
                    var timeoutCheckTask = Task.Delay(timeout);
                    var completedTask = await Task.WhenAny(inputTask, timeoutCheckTask);

                    // In headless mode, exit after receiving response and timeout expires (lines 441-445)
                    if (completedTask == timeoutCheckTask && headless && receivedResponse)
                    {
                        Console.WriteLine("Test complete");
                        break;
                    }

                    // Check if we got input
                    if (completedTask == inputTask)
                    {
                        string? input = await inputTask;

                        // Reset inputTask so a new one will be created on next iteration
                        inputTask = null;

                        if (string.IsNullOrEmpty(input))
                            continue;

                        if (input.Equals("exit", StringComparison.OrdinalIgnoreCase))
                        {
                            await ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "Client closing", CancellationToken.None);
                            break;
                        }

                        // Send input with newline (mimicking lines 456-459)
                        byte[] buffer = Encoding.UTF8.GetBytes(input + "\n");
                        await ws.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
                    }
                }

                cts.Cancel(); // Signal the receive task to stop
                try
                {
                    await listeningTask; // Wait for listener to finish
                }
                catch (OperationCanceledException)
                {
                    // Expected when cancelling
                }
            }
            catch (WebSocketException ex)
            {
                Console.Error.WriteLine($"Connection failed to {host}:{port}: {ex.Message}");
                return;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Error: {ex.Message}");
                return;
            }
        }
    }

    static async Task ReceiveMessages(ClientWebSocket ws, bool chatLog, string logFileName, Action onMessageReceived, CancellationToken cancellationToken)
    {
        var buffer = new byte[BUFFER_SIZE];
        var messageBuffer = new List<byte>();

        try
        {
            while (ws.State == WebSocketState.Open && !cancellationToken.IsCancellationRequested)
            {
                messageBuffer.Clear();
                WebSocketReceiveResult result;

                // Read all fragments of the message
                do
                {
                    result = await ws.ReceiveAsync(new ArraySegment<byte>(buffer), cancellationToken);

                    // If recv returns 0, server disconnected gracefully (line 471-474)
                    if (result.Count == 0 || result.MessageType == WebSocketMessageType.Close)
                    {
                        Console.WriteLine("Server disconnected");
                        return;
                    }

                    // Append this fragment to the message buffer
                    messageBuffer.AddRange(new ArraySegment<byte>(buffer, 0, result.Count));

                } while (!result.EndOfMessage);

                if (result.MessageType == WebSocketMessageType.Text)
                {
                    string payload = Encoding.UTF8.GetString(messageBuffer.ToArray());

                    // Log to file if chat logging is enabled (lines 487-499)
                    if (chatLog && !string.IsNullOrEmpty(payload))
                    {
                        try
                        {
                            File.AppendAllText(logFileName, payload);
                        }
                        catch (Exception)
                        {
                            Console.Error.WriteLine($"Failed to open {logFileName} try running without -s flag!");
                            return;
                        }
                    }

                    // Print the received message (lines 502-505)
                    Console.Write($"Received: {payload}");
                    Console.Out.Flush();

                    // Mark that we've received at least one response
                    onMessageReceived();
                }
            }
        }
        catch (OperationCanceledException)
        {
            // Normal cancellation, ignore
        }
        catch (WebSocketException ex)
        {
            if (ws.State != WebSocketState.Closed && ws.State != WebSocketState.Aborted)
            {
                Console.Error.WriteLine($"Error in receive loop: {ex.Message}");
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Error in receive loop: {ex.Message}");
        }
    }
}
