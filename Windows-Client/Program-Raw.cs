using System;
using System.IO;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Spectre.Console;

class Program
{
    private const int BUFFER_SIZE = 4096;
    private const string WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    static async Task Main(string[] args)
    {
        // Parse command-line arguments
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
            if (args[i] == "-m" && i + 1 < args.Length)
            {
                testMsg = args[++i];
                headless = true;
            }
            else if (args[i] == "-n" && i + 1 < args.Length)
            {
                nameType = true;
                name = args[++i];
            }
            else if (args[i] == "-s")
            {
                Console.WriteLine("Running client with logging");
                chatLog = true;
            }
            else if (args[i] == "-h" && i + 1 < args.Length)
            {
                host = args[++i];
                hostSpecified = true;
            }
            else if (args[i] == "-p" && i + 1 < args.Length)
            {
                port = args[++i];
                portSpecified = true;
            }
        }

        if (!hostSpecified)
        {
            host = "127.0.0.1";
        }

        if (!portSpecified)
        {
            if (host == "localhost" || host == "127.0.0.1" || host == "0.0.0.0")
            {
                port = "9999";
            }
            else
            {
                port = "80";
            }
        }

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

        using (var tcpClient = new TcpClient())
        {
            try
            {
                AnsiConsole.MarkupLine($"[yellow]Connecting to server at {host}:{port}[/]");

                var connectTask = tcpClient.ConnectAsync(host, int.Parse(port));
                var timeoutTask = Task.Delay(10000);

                if (await Task.WhenAny(connectTask, timeoutTask) == timeoutTask)
                {
                    AnsiConsole.MarkupLine($"[red]Connection timeout to {host}:{port}[/]");
                    return;
                }

                await connectTask;
                AnsiConsole.MarkupLine($"[green]Connected to server at {host}:{port}[/]");

                var stream = tcpClient.GetStream();

                // Perform WebSocket handshake with fixed key
                string key = "dGhlIHNhbXBsZSBub25jZQ==";
                string handshake = $"GET / HTTP/1.1\r\n" +
                                 $"Host: {host}\r\n" +
                                 $"Upgrade: websocket\r\n" +
                                 $"Connection: Upgrade\r\n" +
                                 $"Sec-WebSocket-Key: {key}\r\n" +
                                 $"Sec-WebSocket-Version: 13\r\n\r\n";

                byte[] handshakeBytes = Encoding.UTF8.GetBytes(handshake);
                await stream.WriteAsync(handshakeBytes, 0, handshakeBytes.Length);

                // Read handshake response
                byte[] responseBuffer = new byte[BUFFER_SIZE];
                int bytesRead = await stream.ReadAsync(responseBuffer, 0, responseBuffer.Length);
                string response = Encoding.UTF8.GetString(responseBuffer, 0, bytesRead);

                if (!response.Contains("101 Switching Protocols"))
                {
                    AnsiConsole.MarkupLine($"[red]WebSocket handshake failed[/]");
                    return;
                }

                AnsiConsole.MarkupLine("[green]WebSocket handshake complete[/]");

                // Send username if provided
                if (nameType && !string.IsNullOrEmpty(name))
                {
                    string idMsg = $"[ID]{name}";
                    byte[] idFrame = EncodeFrame(idMsg);
                    await stream.WriteAsync(idFrame, 0, idFrame.Length);
                }

                // Send test message if in headless mode
                if (headless && !string.IsNullOrEmpty(testMsg))
                {
                    byte[] msgFrame = EncodeFrame(testMsg);
                    await stream.WriteAsync(msgFrame, 0, msgFrame.Length);
                    Console.Write($"Sent: {testMsg}");
                    Console.Out.Flush();
                }

                // Start receiving messages
                var receivedResponse = false;
                var cts = new CancellationTokenSource();
                var receiveTask = ReceiveMessages(stream, chatLog, logFileName, () => receivedResponse = true, cts.Token);

                // Main loop
                Task<string?>? inputTask = null;
                while (true)
                {
                    int timeout = headless ? 5000 : 50000;

                    if (inputTask == null && !headless)
                    {
                        inputTask = Task.Run(() =>
                        {
                            return AnsiConsole.Prompt(
                                new TextPrompt<string>("[bold blue]>[/] ")
                                    .AllowEmpty()
                            );
                        });
                    }

                    var timeoutCheckTask = Task.Delay(timeout);
                    var completedTask = headless ? timeoutCheckTask : await Task.WhenAny(inputTask!, timeoutCheckTask);

                    if (completedTask == timeoutCheckTask && headless && receivedResponse)
                    {
                        AnsiConsole.MarkupLine("[yellow]Test complete[/]");
                        break;
                    }

                    if (!headless && completedTask == inputTask)
                    {
                        string? input = await inputTask!;
                        inputTask = null;

                        if (string.IsNullOrEmpty(input))
                            continue;

                        if (input.Equals("exit", StringComparison.OrdinalIgnoreCase))
                        {
                            break;
                        }

                        byte[] frame = EncodeFrame(input + "\n");
                        await stream.WriteAsync(frame, 0, frame.Length);
                        AnsiConsole.MarkupLine($"[green]Sent: {input.EscapeMarkup()}[/]");
                    }
                }

                cts.Cancel();
                try { await receiveTask; } catch (OperationCanceledException) { }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Error: {ex.Message}");
            }
        }
    }

    static byte[] EncodeFrame(string payload)
    {
        byte[] payloadBytes = Encoding.UTF8.GetBytes(payload);
        int len = payloadBytes.Length;

        // Generate random mask
        byte[] mask = new byte[4];
        Random.Shared.NextBytes(mask);

        byte[] frame;
        if (len <= 125)
        {
            frame = new byte[6 + len];
            frame[0] = 0x81; // FIN + text frame
            frame[1] = (byte)(0x80 | len); // MASK + length
            Array.Copy(mask, 0, frame, 2, 4);
            for (int i = 0; i < len; i++)
            {
                frame[6 + i] = (byte)(payloadBytes[i] ^ mask[i % 4]);
            }
        }
        else if (len <= 65535)
        {
            frame = new byte[8 + len];
            frame[0] = 0x81;
            frame[1] = (byte)(0x80 | 126);
            frame[2] = (byte)((len >> 8) & 0xFF);
            frame[3] = (byte)(len & 0xFF);
            Array.Copy(mask, 0, frame, 4, 4);
            for (int i = 0; i < len; i++)
            {
                frame[8 + i] = (byte)(payloadBytes[i] ^ mask[i % 4]);
            }
        }
        else
        {
            throw new Exception("Payload too large");
        }

        return frame;
    }

    static (string? payload, int consumed) DecodeFrame(byte[] data, int length)
    {
        if (length < 2) return (null, 0);

        int opcode = data[0] & 0x0F;
        if (opcode == 0x8) return (null, -1); // Close frame

        int payloadLen = data[1] & 0x7F;
        int pos = 2;

        if (payloadLen == 126)
        {
            payloadLen = (data[2] << 8) | data[3];
            pos = 4;
        }
        else if (payloadLen == 127)
        {
            payloadLen = 0;
            for (int i = 0; i < 8; i++)
            {
                payloadLen = (payloadLen << 8) | data[2 + i];
            }
            pos = 10;
        }

        if (length < pos + payloadLen) return (null, 0);

        byte[] payload = new byte[payloadLen];
        Array.Copy(data, pos, payload, 0, payloadLen);

        return (Encoding.UTF8.GetString(payload), pos + payloadLen);
    }

    static async Task ReceiveMessages(NetworkStream stream, bool chatLog, string logFileName, Action onMessageReceived, CancellationToken cancellationToken)
    {
        byte[] buffer = new byte[BUFFER_SIZE];
        byte[] frameBuffer = new byte[0];

        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                int bytesRead = await stream.ReadAsync(buffer, 0, buffer.Length, cancellationToken);

                if (bytesRead == 0)
                {
                    AnsiConsole.MarkupLine("[red]Server disconnected[/]");
                    return;
                }

                // Append to frame buffer
                byte[] newBuffer = new byte[frameBuffer.Length + bytesRead];
                Array.Copy(frameBuffer, 0, newBuffer, 0, frameBuffer.Length);
                Array.Copy(buffer, 0, newBuffer, frameBuffer.Length, bytesRead);
                frameBuffer = newBuffer;

                // Try to decode frames
                while (frameBuffer.Length > 0)
                {
                    var (payload, consumed) = DecodeFrame(frameBuffer, frameBuffer.Length);

                    if (consumed == -1)
                    {
                        AnsiConsole.MarkupLine("[red]Server disconnected[/]");
                        return;
                    }

                    if (consumed == 0) break; // Need more data

                    if (payload != null && chatLog)
                    {
                        try
                        {
                            File.AppendAllText(logFileName, payload);
                        }
                        catch (Exception)
                        {
                            Console.Error.WriteLine($"Failed to write to {logFileName}");
                        }
                    }

                    if (payload != null)
                    {
                        AnsiConsole.Markup($"[cyan]Received: {payload.EscapeMarkup()}[/]");
                        Console.Out.Flush();
                        onMessageReceived();
                    }

                    // Remove consumed bytes
                    byte[] remaining = new byte[frameBuffer.Length - consumed];
                    Array.Copy(frameBuffer, consumed, remaining, 0, remaining.Length);
                    frameBuffer = remaining;
                }
            }
        }
        catch (OperationCanceledException)
        {
            // Normal cancellation
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Error in receive loop: {ex.Message}");
        }
    }
}
