const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "zig-server-damon",
        .root_module = b.createModule(.{
            .root_source_file = b.path("server.zig"),
            .target = target,
            .optimize = optimize,
        }),
    });

    // Add C source files from lib directory
    exe.addCSourceFiles(.{
        .files = &[_][]const u8{
            "../../lib/ws_json.c",
            "../../lib/ws_client_lib.c",
        },
        .flags = &[_][]const u8{
            "-DWS_ENABLE_LOG_DEBUG",
            "-DWS_ENABLE_LOG_ERROR",
        },
    });

    // Add include path for lib headers
    exe.addIncludePath(b.path("../../lib"));

    // Link libc
    exe.linkLibC();

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the server");
    run_step.dependOn(&run_cmd.step);
}
