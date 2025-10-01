# Build type: can be overridden, defaults to "dev"
BUILD ?= dev
BIN_DIR := src/bin/$(BUILD)

CFLAGS_dev     = -g -O0       # debug flags
CFLAGS_release = -O2 -s       # release flags
CFLAGS         = $(CFLAGS_$(BUILD))

all: $(BIN_DIR)/ws_server $(BIN_DIR)/ws_client

# Targets for server and client
$(BIN_DIR)/ws_server: src/ws_server.c | $(BIN_DIR)
	gcc $(CFLAGS) $< -o $@

$(BIN_DIR)/ws_client: src/ws_client.c | $(BIN_DIR)
	gcc $(CFLAGS) $< -o $@

# Windows UI build
windows_ui: $(BIN_DIR)/ui.exe

$(BIN_DIR)/ui.exe: ui.c | $(BIN_DIR)
	x86_64-w64-mingw32-gcc $(CFLAGS) $< -o $@ -mwindows

# Ensure bin dir exists
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Cleanup
clean:
	rm -rf $(BIN_DIR)

# Force rebuild shortcut
.PHONY: all clean rebuild windows_ui
rebuild: clean all
