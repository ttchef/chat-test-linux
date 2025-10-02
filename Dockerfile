# Use Debian with GCC for building
FROM debian:bookworm-slim AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    libc6-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy all source files and headers
COPY src/ ./src/

# Build the server
WORKDIR /app/src
RUN gcc -o ws_server ws_server.c

# Create minimal runtime image
FROM debian:bookworm-slim

# Install net-tools for healthcheck
RUN apt-get update && apt-get install -y --no-install-recommends \
    net-tools \
    && rm -rf /var/lib/apt/lists/*

# Copy built binary from builder
WORKDIR /app
COPY --from=builder /app/src/ws_server .

# Expose WebSocket port
EXPOSE 9999

# Run the server
CMD ["./ws_server"]