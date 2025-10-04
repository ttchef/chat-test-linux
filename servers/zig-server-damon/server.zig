const std = @import("std");
const net = std.net;
const crypto = std.crypto;
const base64 = std.base64;
const mem = std.mem;
const json = @import("json_bindings.zig");

const MAX_CLIENTS = 10;
const PORT = 9999;
const BUFFER_SIZE = 4096;

// Message flags
const WS_NO_BROADCAST: u64 = 1 << 0;
const WS_SEND_BACK: u64 = 1 << 1;
const WS_CHANGE_USERNAME: u64 = 1 << 2;

const Client = struct {
    id: i32,
    stream: net.Stream,
    username: []const u8,
    handshake_done: bool,
    allocator: mem.Allocator,

    fn init(id: i32, stream: net.Stream, allocator: mem.Allocator) Client {
        return Client{
            .id = id,
            .stream = stream,
            .username = "Anonym",
            .handshake_done = false,
            .allocator = allocator,
        };
    }

    fn setUsername(self: *Client, name: []const u8) !void {
        const owned = try self.allocator.dupe(u8, name);
        self.username = owned;
    }
};

fn sha1Hash(input: []const u8) [20]u8 {
    var hash: [20]u8 = undefined;
    crypto.hash.Sha1.hash(input, &hash, .{});
    return hash;
}

fn base64Encode(allocator: mem.Allocator, input: []const u8) ![]u8 {
    const encoder = base64.standard.Encoder;
    const encoded_len = encoder.calcSize(input.len);
    const result = try allocator.alloc(u8, encoded_len);
    _ = encoder.encode(result, input);
    return result;
}

fn handleWebSocketHandshake(stream: net.Stream, buffer: []const u8, allocator: mem.Allocator) !void {
    std.debug.print("Received handshake:\n{s}\n", .{buffer});

    // Extract Sec-WebSocket-Key
    const key_header = "Sec-WebSocket-Key:";
    const key_start = std.mem.indexOf(u8, buffer, key_header) orelse return error.NoWebSocketKey;

    var key_line_start = key_start + key_header.len;
    while (key_line_start < buffer.len and (buffer[key_line_start] == ' ' or buffer[key_line_start] == '\t')) {
        key_line_start += 1;
    }

    const key_line_end = std.mem.indexOfPos(u8, buffer, key_line_start, "\r\n") orelse buffer.len;
    const client_key = buffer[key_line_start..key_line_end];

    std.debug.print("Extracted WebSocket key: '{s}' (len={d})\n", .{ client_key, client_key.len });

    // Create accept key
    const magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const concat_key = try std.fmt.allocPrint(allocator, "{s}{s}", .{ client_key, magic_string });
    defer allocator.free(concat_key);

    // Hash the concatenated key
    const hash = sha1Hash(concat_key);

    // Base64 encode the hash
    const accept_key = try base64Encode(allocator, &hash);
    defer allocator.free(accept_key);

    std.debug.print("Sending handshake response with key: {s}\n", .{accept_key});

    // Build and send response
    const response = try std.fmt.allocPrint(
        allocator,
        "HTTP/1.1 101 Switching Protocols\r\n" ++
            "Upgrade: websocket\r\n" ++
            "Connection: Upgrade\r\n" ++
            "Sec-WebSocket-Accept: {s}\r\n\r\n",
        .{accept_key},
    );
    defer allocator.free(response);

    _ = try stream.write(response);
    std.debug.print("Sent {d} bytes\n", .{response.len});
}

fn encodeWebSocketFrame(allocator: mem.Allocator, payload: []const u8) ![]u8 {
    const len = payload.len;
    var frame_size: usize = 0;
    var header_size: usize = 2;

    if (len <= 125) {
        frame_size = 2 + len;
    } else if (len <= 65535) {
        frame_size = 4 + len;
        header_size = 4;
    } else {
        return error.PayloadTooLarge;
    }

    const frame = try allocator.alloc(u8, frame_size);
    frame[0] = 0x81; // FIN + text frame

    if (len <= 125) {
        frame[1] = @intCast(len);
        @memcpy(frame[2..], payload);
    } else {
        frame[1] = 126;
        frame[2] = @intCast((len >> 8) & 0xFF);
        frame[3] = @intCast(len & 0xFF);
        @memcpy(frame[4..], payload);
    }

    return frame;
}

fn decodeWebSocketFrame(buffer: []const u8, payload_buf: []u8) !usize {
    if (buffer.len < 2) return error.InvalidFrame;

    const opcode = buffer[0] & 0x0F;
    if (opcode == 0x8) return error.CloseFrame;

    const masked = (buffer[1] & 0x80) != 0;
    var payload_len: usize = buffer[1] & 0x7F;
    var pos: usize = 2;

    if (payload_len == 126) {
        if (buffer.len < 4) return error.InvalidFrame;
        payload_len = (@as(usize, buffer[2]) << 8) | buffer[3];
        pos = 4;
    } else if (payload_len == 127) {
        if (buffer.len < 10) return error.InvalidFrame;
        payload_len = 0;
        var i: usize = 0;
        while (i < 8) : (i += 1) {
            payload_len = (payload_len << 8) | buffer[2 + i];
        }
        pos = 10;
    }

    var mask: [4]u8 = undefined;
    if (masked) {
        if (buffer.len < pos + 4) return error.InvalidFrame;
        @memcpy(&mask, buffer[pos .. pos + 4]);
        pos += 4;
    }

    if (buffer.len < pos + payload_len) return error.InvalidFrame;

    var i: usize = 0;
    while (i < payload_len) : (i += 1) {
        if (masked) {
            payload_buf[i] = buffer[pos + i] ^ mask[i % 4];
        } else {
            payload_buf[i] = buffer[pos + i];
        }
    }

    return payload_len;
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const address = try net.Address.parseIp("0.0.0.0", PORT);
    var listener = try address.listen(.{
        .reuse_address = true,
    });
    defer listener.deinit();

    std.debug.print("WebSocket server listening on 0.0.0.0:{d}\n", .{PORT});

    var clients = try std.ArrayList(Client).initCapacity(allocator, MAX_CLIENTS);
    defer clients.deinit(allocator);

    var next_client_id: i32 = 1;

    var pollfds = try std.ArrayList(std.posix.pollfd).initCapacity(allocator, MAX_CLIENTS + 1);
    defer pollfds.deinit(allocator);

    // Add listener to pollfds
    try pollfds.append(allocator, .{
        .fd = listener.stream.handle,
        .events = std.posix.POLL.IN,
        .revents = 0,
    });

    while (true) {
        const ready = try std.posix.poll(pollfds.items, -1);
        if (ready == 0) continue;

        // Check if listener has new connection
        if (pollfds.items[0].revents & std.posix.POLL.IN != 0) {
            const conn = listener.accept() catch |err| {
                std.debug.print("Accept error: {}\n", .{err});
                continue;
            };

            if (clients.items.len >= MAX_CLIENTS) {
                std.debug.print("Max clients reached, rejecting connection\n", .{});
                conn.stream.close();
                continue;
            }

            const client_id = next_client_id;
            next_client_id += 1;

            const client = Client.init(client_id, conn.stream, allocator);
            try clients.append(allocator, client);

            try pollfds.append(allocator, .{
                .fd = conn.stream.handle,
                .events = std.posix.POLL.IN,
                .revents = 0,
            });

            std.debug.print("Client connected (id={d}, slot={d})\n", .{ client_id, clients.items.len - 1 });
        }

        // Check client connections
        var i: usize = 1;
        while (i < pollfds.items.len) {
            if (pollfds.items[i].revents & std.posix.POLL.IN != 0) {
                const client_idx = i - 1;
                var client = &clients.items[client_idx];

                var buffer: [BUFFER_SIZE]u8 = undefined;
                const len = client.stream.read(&buffer) catch |err| {
                    std.debug.print("Client disconnected (id={d}): {}\n", .{ client.id, err });
                    client.stream.close();
                    _ = clients.orderedRemove(client_idx);
                    _ = pollfds.orderedRemove(i);
                    continue;
                };

                if (len == 0) {
                    std.debug.print("Client disconnected (id={d})\n", .{client.id});
                    client.stream.close();
                    _ = clients.orderedRemove(client_idx);
                    _ = pollfds.orderedRemove(i);
                    continue;
                }

                if (!client.handshake_done) {
                    handleWebSocketHandshake(client.stream, buffer[0..len], allocator) catch |err| {
                        std.debug.print("Handshake error: {}\n", .{err});
                        client.stream.close();
                        _ = clients.orderedRemove(client_idx);
                        _ = pollfds.orderedRemove(i);
                        continue;
                    };
                    client.handshake_done = true;
                    std.debug.print("WebSocket handshake complete (id={d})\n", .{client.id});
                } else {
                    // Decode WebSocket frame
                    var payload: [BUFFER_SIZE]u8 = undefined;
                    const payload_len = decodeWebSocketFrame(buffer[0..len], &payload) catch |err| {
                        if (err == error.CloseFrame) {
                            std.debug.print("Client closed connection (id={d})\n", .{client.id});
                            client.stream.close();
                            _ = clients.orderedRemove(client_idx);
                            _ = pollfds.orderedRemove(i);
                        }
                        continue;
                    };

                    const payload_str = payload[0..payload_len];
                    std.debug.print("Server received Message: {s}\n", .{payload_str});

                    // Parse JSON message
                    const root = json.parseJson(payload_str);
                    if (root == null) {
                        std.debug.print("Failed to parse JSON message, skipping...\n", .{});
                        continue;
                    }
                    defer json.free(root);

                    // Extract user and message data
                    const user_obj = json.getObject(root, "user");
                    const name = json.getString(user_obj, "name");

                    const message_obj = json.getObject(root, "message");
                    const info = json.getNumber(message_obj, "info");
                    const flags: u64 = @intFromFloat(info);

                    // Handle username change
                    if (flags & WS_CHANGE_USERNAME != 0) {
                        std.debug.print("Change username message detected!\n", .{});
                        if (name) |n| {
                            try client.setUsername(n);
                            std.debug.print("Updated client: {d} name to: {s}\n", .{ client.id, n });
                        }
                    }

                    // Don't broadcast if NO_BROADCAST flag is set
                    if (flags & WS_NO_BROADCAST != 0) {
                        continue;
                    }

                    // Build new JSON with server-side username and cleared flags
                    var json_buffer: [BUFFER_SIZE]u8 = undefined;
                    const new_root = json.initChild("") orelse continue;
                    defer json.free(new_root);

                    // Create user object with server-side username
                    const new_user = json.initChild("user") orelse continue;
                    const user_name = json.initString("name", client.username) orelse continue;
                    json.addField(new_user, user_name);
                    json.addField(new_root, new_user);

                    // Create message object with text, text_len, and cleared info
                    const new_message = json.initChild("message") orelse continue;
                    if (json.getString(message_obj, "text")) |text| {
                        const msg_text = json.initString("text", text) orelse continue;
                        json.addField(new_message, msg_text);
                    }
                    const text_len = json.getNumber(message_obj, "text_len");
                    const msg_len = json.initNumber("text_len", text_len) orelse continue;
                    json.addField(new_message, msg_len);
                    const msg_info = json.initNumber("info", 0) orelse continue; // Clear flags
                    json.addField(new_message, msg_info);
                    json.addField(new_root, new_message);

                    const json_len = json.toString(new_root, &json_buffer);
                    const json_str = json_buffer[0..@intCast(json_len)];

                    // Encode and broadcast
                    const frame = try encodeWebSocketFrame(allocator, json_str);
                    defer allocator.free(frame);

                    for (clients.items, 0..) |*c, j| {
                        // Skip sender unless SEND_BACK flag is set
                        if (j == client_idx and (flags & WS_SEND_BACK == 0)) continue;

                        if (c.handshake_done) {
                            _ = c.stream.write(frame) catch |err| {
                                std.debug.print("Failed to send to client (id={d}): {}\n", .{ c.id, err });
                            };
                        }
                    }
                }
            }
            i += 1;
        }
    }
}
