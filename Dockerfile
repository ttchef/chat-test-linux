# Use GCC base image for building
FROM gcc:latest

# Set working directory
WORKDIR /app

# Copy source files
COPY src/ ./src/

# Build the server
WORKDIR /app/src
RUN gcc ws_server.c -o ws_server

# Expose WebSocket port
EXPOSE 9999

# Run the server
CMD ["./ws_server"]