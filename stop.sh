#!/bin/bash

# Kill ws_server
if pgrep -f ws_server > /dev/null; then
    pkill -f ws_server
    echo "Stopped ws_server"
else
    echo "ws_server not running"
fi

# Kill ws_client
if pgrep -f ws_client > /dev/null; then
    pkill -f ws_client
    echo "Stopped ws_client"
else
    echo "ws_client not running"
fi