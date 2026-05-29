#!/bin/bash

echo "=== End-to-End Remote Sync Test ==="

BIN="../build/sync-tool"
SERVER_DIR="/tmp/sync-server-e2e-test"
SRC_DIR="/tmp/sync-client-e2e-src"
PORT=$((19999 + RANDOM % 1000))

cleanup() {
    echo "Cleaning up..."
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
    sleep 1
    rm -rf "$SERVER_DIR" "$SRC_DIR"
    echo "Cleanup done."
}

trap cleanup EXIT

rm -rf "$SERVER_DIR" "$SRC_DIR"
mkdir -p "$SERVER_DIR" "$SRC_DIR"
mkdir -p "$SRC_DIR/subdir"

echo "test content 1" > "$SRC_DIR/file1.txt"
echo "test content 2" > "$SRC_DIR/file2.txt"
echo "test content 3" > "$SRC_DIR/subdir/file3.txt"

echo ""
echo "1. Starting server on port $PORT..."
$BIN --server --port $PORT --server-dir "$SERVER_DIR" &
SERVER_PID=$!
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start!"
    exit 1
fi
echo "Server started (PID: $SERVER_PID)"

echo ""
echo "2. Running remote sync (client -> server)..."
$BIN -s "$SRC_DIR" -d ./ -h 127.0.0.1 -p $PORT
CLIENT_EXIT=$?

echo ""
echo "3. Verifying received files..."

check_file() {
    local path="$1"
    local expected="$2"
    if [ -f "$path" ]; then
        local content=$(cat "$path")
        if [ "$content" = "$expected" ]; then
            echo "  PASS: $path"
        else
            echo "  FAIL: $path - expected '$expected', got '$content'"
            exit 1
        fi
    else
        echo "  FAIL: $path does not exist"
        exit 1
    fi
}

check_file "$SERVER_DIR/file1.txt" "test content 1"
check_file "$SERVER_DIR/file2.txt" "test content 2"
check_file "$SERVER_DIR/subdir/file3.txt" "test content 3"

echo ""
echo "4. Stopping server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== All tests passed! ==="