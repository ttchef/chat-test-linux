const std = @import("std");

// C JSON library bindings
pub const wsJsonType = enum(c_int) {
    WS_JSON_STRING = 0,
    WS_JSON_NUMBER = 1,
    WS_JSON_OBJECT = 2,
    WS_JSON_BOOL = 3,
    WS_JSON_ARRAY = 4,
    WS_JSON_NULL = 5,
};

pub const wsJson = extern struct {
    key: [256]u8,
    type: wsJsonType,
    // Union data follows, but we'll access it through helper functions
};

// External C functions from the JSON library
extern fn wsStringToJson(string: [*c]const [*c]const u8) ?*wsJson;
extern fn wsJsonGet(obj: ?*wsJson, key: [*c]const u8) ?*wsJson;
extern fn wsJsonGetString(obj: ?*wsJson, key: [*c]const u8) [*c]const u8;
extern fn wsJsonGetNumber(obj: ?*wsJson, key: [*c]const u8) f64;
extern fn wsJsonToString(obj: ?*wsJson, out: [*c]u8, size: i32) i32;
extern fn wsJsonFree(obj: ?*wsJson) void;
extern fn wsJsonInitChild(key: [*c]const u8) ?*wsJson;
extern fn wsJsonInitString(key: [*c]const u8, val: [*c]const u8) ?*wsJson;
extern fn wsJsonInitNumber(key: [*c]const u8, val: f64) ?*wsJson;
extern fn wsJsonInitBool(key: [*c]const u8, val: bool) ?*wsJson;
extern fn wsJsonAddField(parent: ?*wsJson, child: ?*wsJson) void;

// Helper functions for safer Zig usage
pub fn parseJson(json_str: []const u8) ?*wsJson {
    var ptr: [*c]const u8 = json_str.ptr;
    return wsStringToJson(&ptr);
}

pub fn getObject(obj: ?*wsJson, key: []const u8) ?*wsJson {
    const c_key = @as([*c]const u8, @ptrCast(key.ptr));
    return wsJsonGet(obj, c_key);
}

pub fn getString(obj: ?*wsJson, key: []const u8) ?[]const u8 {
    const c_key = @as([*c]const u8, @ptrCast(key.ptr));
    const result = wsJsonGetString(obj, c_key);
    if (result == null) return null;
    return std.mem.span(result);
}

pub fn getNumber(obj: ?*wsJson, key: []const u8) f64 {
    const c_key = @as([*c]const u8, @ptrCast(key.ptr));
    return wsJsonGetNumber(obj, c_key);
}

pub fn toString(obj: ?*wsJson, buffer: []u8) i32 {
    const c_buf = @as([*c]u8, @ptrCast(buffer.ptr));
    return wsJsonToString(obj, c_buf, @intCast(buffer.len));
}

pub fn free(obj: ?*wsJson) void {
    wsJsonFree(obj);
}

pub fn initChild(key: []const u8) ?*wsJson {
    const c_key = @as([*c]const u8, @ptrCast(key.ptr));
    return wsJsonInitChild(c_key);
}

pub fn initString(key: []const u8, value: []const u8) ?*wsJson {
    const c_key = @as([*c]const u8, @ptrCast(key.ptr));
    const c_value = @as([*c]const u8, @ptrCast(value.ptr));
    return wsJsonInitString(c_key, c_value);
}

pub fn initNumber(key: []const u8, value: f64) ?*wsJson {
    const c_key = @as([*c]const u8, @ptrCast(key.ptr));
    return wsJsonInitNumber(c_key, value);
}

pub fn initBool(key: []const u8, value: bool) ?*wsJson {
    const c_key = @as([*c]const u8, @ptrCast(key.ptr));
    return wsJsonInitBool(c_key, value);
}

pub fn addField(parent: ?*wsJson, child: ?*wsJson) void {
    wsJsonAddField(parent, child);
}
