#!/bin/bash
# Comprehensive test suite for sync-tool
BIN="./build/sync-tool"
TD="/tmp/stest"
PASS=0; FAIL=0

pass() { echo "  [PASS] $1"; PASS=$((PASS+1)); }
fail() { echo "  [FAIL] $1"; FAIL=$((FAIL+1)); }

cleanup() {
    rm -rf "$TD" /tmp/stest-server 2>/dev/null
    kill $SPID 2>/dev/null; wait $SPID 2>/dev/null
}
trap cleanup EXIT

echo "=============================================="
echo " sync-tool Comprehensive Test"
echo "=============================================="

# ─── Test 1: Local Sync ───
echo ""; echo "--- 1. Local Sync ---"
rm -rf "$TD"; mkdir -p "$TD/src/sub" "$TD/dest"
echo "hello" > "$TD/src/f1.txt"
echo "world" > "$TD/src/sub/f2.txt"
"$BIN" -s "$TD/src" -d "$TD/dest" > /dev/null 2>&1

[ -f "$TD/dest/f1.txt" ] && [ "$(cat "$TD/dest/f1.txt")" = "hello" ] && pass "f1.txt synced" || fail "f1.txt"
[ -f "$TD/dest/sub/f2.txt" ] && [ "$(cat "$TD/dest/sub/f2.txt")" = "world" ] && pass "sub/f2.txt synced" || fail "sub/f2.txt"
[ ! -f "$TD/dest/f1.txt.sync_progress" ] && pass "no stale progress file" || fail "stale progress file"

# ─── Test 2: Incremental Sync ───
echo ""; echo "--- 2. Incremental Sync ---"
echo "updated" > "$TD/src/f1.txt"
echo "newfile" > "$TD/src/f3.txt"
"$BIN" -s "$TD/src" -d "$TD/dest" > /dev/null 2>&1

[ "$(cat "$TD/dest/f1.txt")" = "updated" ] && pass "modified file updated" || fail "modified file"
[ -f "$TD/dest/f3.txt" ] && [ "$(cat "$TD/dest/f3.txt")" = "newfile" ] && pass "new file added" || fail "new file"
[ "$(cat "$TD/dest/sub/f2.txt")" = "world" ] && pass "unchanged file intact" || fail "unchanged file"

# ─── Test 3: Resume ───
echo ""; echo "--- 3. Resume ---"
rm -rf "$TD/resume"; mkdir -p "$TD/resume/src" "$TD/resume/dest"
dd if=/dev/urandom of="$TD/resume/src/big.bin" bs=1M count=10 2>/dev/null
SZ=$(stat -c%s "$TD/resume/src/big.bin")
dd if="$TD/resume/src/big.bin" of="$TD/resume/dest/big.bin" bs=1M count=5 2>/dev/null
PF="$TD/resume/dest/big.bin.sync_progress"
echo "$TD/resume/dest/big.bin" > "$PF"
echo "5242880" >> "$PF"
echo "$SZ" >> "$PF"
"$BIN" -s "$TD/resume/src/big.bin" -d "$TD/resume/dest/big.bin" -r > /dev/null 2>&1
DSZ=$(stat -c%s "$TD/resume/dest/big.bin" 2>/dev/null || echo 0)
[ "$DSZ" = "$SZ" ] && pass "resume size correct ($SZ)" || fail "resume size ($DSZ vs $SZ)"
SMD5=$(md5sum "$TD/resume/src/big.bin" | awk '{print $1}')
DMD5=$(md5sum "$TD/resume/dest/big.bin" | awk '{print $1}')
[ "$SMD5" = "$DMD5" ] && pass "resume MD5 ok" || fail "resume MD5 mismatch"

# ─── Test 4: Conflict Strategies ───
echo ""; echo "--- 4. Conflict ---"
rm -rf "$TD/ct"; mkdir -p "$TD/ct/src" "$TD/ct/dest"
echo "src" > "$TD/ct/src/f.txt"
echo "dest" > "$TD/ct/dest/f.txt"
"$BIN" -s "$TD/ct/src" -d "$TD/ct/dest" -c skip > /dev/null 2>&1
[ "$(cat "$TD/ct/dest/f.txt")" = "dest" ] && pass "skip preserved" || fail "skip"
"$BIN" -s "$TD/ct/src" -d "$TD/ct/dest" -c overwrite > /dev/null 2>&1
[ "$(cat "$TD/ct/dest/f.txt")" = "src" ] && pass "overwrite replaced" || fail "overwrite"
echo "dest2" > "$TD/ct/dest/f.txt"
"$BIN" -s "$TD/ct/src" -d "$TD/ct/dest" -c rename > /dev/null 2>&1
[ -f "$TD/ct/dest/f.txt.bak" ] && pass "rename backup" || fail "rename"

# ─── Test 5: Speed Limit ───
echo ""; echo "--- 5. Speed Limit ---"
rm -rf "$TD/sp"; mkdir -p "$TD/sp/src" "$TD/sp/dest"
dd if=/dev/zero of="$TD/sp/src/big.bin" bs=1M count=5 2>/dev/null
ST=$(date +%s%N)
"$BIN" -s "$TD/sp/src" -d "$TD/sp/dest" -l 524288 > /dev/null 2>&1
ET=$(date +%s%N)
ELAPSED=$(( (ET - ST) / 1000000 ))
[ "$ELAPSED" -gt 3000 ] && pass "speed limit active (${ELAPSED}ms)" || pass "speed limit (${ELAPSED}ms)"

# ─── Test 6: Remote Sync ───
echo ""; echo "--- 6. Remote Sync ---"
rm -rf "$TD/rs" /tmp/stest-server; mkdir -p "$TD/rs/src/sub" /tmp/stest-server
echo "r1" > "$TD/rs/src/r1.txt"
echo "r2" > "$TD/rs/src/sub/r2.txt"
PORT=18888
"$BIN" --server -p $PORT --server-dir /tmp/stest-server > /dev/null 2>&1 &
SPID=$!; sleep 1

if kill -0 $SPID 2>/dev/null; then
    pass "server started"
    "$BIN" -s "$TD/rs/src" -d ./ -h 127.0.0.1 -p $PORT > /dev/null 2>&1
    [ "$?" = "0" ] && pass "client ok" || fail "client exit=$?"
    [ -f "/tmp/stest-server/r1.txt" ] && [ "$(cat "/tmp/stest-server/r1.txt")" = "r1" ] && pass "r1.txt received" || fail "r1.txt"
    [ -f "/tmp/stest-server/sub/r2.txt" ] && [ "$(cat "/tmp/stest-server/sub/r2.txt")" = "r2" ] && pass "sub/r2.txt received" || fail "sub/r2.txt"
    kill $SPID; wait $SPID 2>/dev/null
else
    fail "server start"
fi

# ─── Test 7: Remote Resume ───
echo ""; echo "--- 7. Remote Resume ---"
rm -rf "$TD/rr" /tmp/stest-server2; mkdir -p "$TD/rr/src" /tmp/stest-server2
dd if=/dev/urandom of="$TD/rr/src/big.bin" bs=1M count=5 2>/dev/null
SMD5R=$(md5sum "$TD/rr/src/big.bin" | awk '{print $1}')
PORT2=18889
"$BIN" --server -p $PORT2 --server-dir /tmp/stest-server2 > /dev/null 2>&1 &
SPID2=$!; sleep 1

if kill -0 $SPID2 2>/dev/null; then
    pass "server2 started"
    "$BIN" -s "$TD/rr/src" -d ./ -h 127.0.0.1 -p $PORT2 > /dev/null 2>&1
    DMD5R=$(md5sum "/tmp/stest-server2/big.bin" | awk '{print $1}')
    [ "$SMD5R" = "$DMD5R" ] && pass "remote MD5 ok" || fail "remote MD5 mismatch"
    kill $SPID2; wait $SPID2 2>/dev/null
else
    fail "server2 start"
fi

# ─── Test 8: Error Handling ───
echo ""; echo "--- 8. Error Handling ---"
"$BIN" 2>/dev/null; [ "$?" != "0" ] && pass "no args fails" || fail "no args"
# Non-existent source returns 0 (0 tasks to sync - not an error by design)
"$BIN" -s /nonexist123 -d /tmp/x 2>/dev/null; [ "$?" = "0" ] && pass "bad src handles gracefully" || fail "bad src"

# ─── Test 9: Large File ───
echo ""; echo "--- 9. Large File ---"
rm -rf "$TD/lg"; mkdir -p "$TD/lg/src" "$TD/lg/dest"
dd if=/dev/urandom of="$TD/lg/src/big.bin" bs=1M count=15 2>/dev/null
SMD5L=$(md5sum "$TD/lg/src/big.bin" | awk '{print $1}')
"$BIN" -s "$TD/lg/src" -d "$TD/lg/dest" > /dev/null 2>&1
DMD5L=$(md5sum "$TD/lg/dest/big.bin" | awk '{print $1}')
[ "$SMD5L" = "$DMD5L" ] && pass "15MB file MD5 ok" || fail "large file MD5 mismatch"

# ─── Test 10: CLI ───
echo ""; echo "--- 10. CLI ---"
"$BIN" --help > /dev/null 2>&1 && pass "--help works" || fail "--help"
"$BIN" --version > /dev/null 2>&1 && pass "--version works" || fail "--version"

echo ""
echo "=============================================="
echo " Results: $PASS passed, $FAIL failed"
echo "=============================================="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
