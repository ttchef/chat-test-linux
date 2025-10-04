# Use Debian with GCC for building
FROM debian:bookworm-slim AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    libc6-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy the C server source files and headers
COPY servers/c-server/ ./servers/c-server/
COPY lib/ ./lib/

# Build the server
WORKDIR /app/servers/c-server
RUN gcc -o ws_server ws_server.c ../../lib/ws_json.c ../../lib/ws_client_lib.c -I../../lib -DWS_ENABLE_LOG_DEBUG -DWS_ENABLE_LOG_ERROR -D_GNU_SOURCE

# Create minimal runtime image
FROM debian:bookworm-slim

# Install net-tools for healthcheck
RUN apt-get update && apt-get install -y --no-install-recommends \
    net-tools \
    && rm -rf /var/lib/apt/lists/*

# Copy built binary from builder
WORKDIR /app
COPY --from=builder /app/servers/c-server/ws_server .

# Expose WebSocket port
EXPOSE 9999

# Run the server
CMD ["./ws_server"]