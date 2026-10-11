#!/usr/bin/env bash
# Smoke test for an installed server folder (cmake --install): the server must
# answer the status query, and SIGTERM must save everything and exit with 0.
# POSIX only (Linux/macOS). Usage: tools/smoke-test.sh <install-dir> [port]
set -u

dir="$1"
port="${2:-7171}"
cd "$dir" || exit 2

log=smoke-test.log
./evolutions > "$log" 2>&1 &
pid=$!

status_ok() {
	python3 - "$port" <<'EOF'
import socket, sys
try:
    s = socket.create_connection(("127.0.0.1", int(sys.argv[1])), timeout=3)
    s.sendall(b"\x06\x00\xff\xffinfo")
    data = b""
    while b"</tsqp>" not in data:
        chunk = s.recv(4096)
        if not chunk:
            break
        data += chunk
    sys.exit(0 if b"<tsqp" in data else 1)
except OSError:
    sys.exit(1)
EOF
}

fail() {
	echo "SMOKE TEST FAILED: $1"
	kill -KILL "$pid" 2>/dev/null
	cat "$log"
	exit 1
}

up=0
for _ in $(seq 1 180); do
	kill -0 "$pid" 2>/dev/null || fail "server exited during startup"
	if status_ok; then up=1; break; fi
	sleep 1
done
[ "$up" = 1 ] || fail "no status reply within 180 s"
echo "status query answered"

kill -TERM "$pid"
for _ in $(seq 1 60); do
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill -0 "$pid" 2>/dev/null && fail "still running 60 s after SIGTERM"
wait "$pid"
rc=$?

grep -q "Shutting down Server" "$log" || fail "no save/shutdown after SIGTERM"
grep -qiE "failed to save|SQLITE ERROR|MYSQL ERROR|SQL ERROR" "$log" && fail "errors in the log"
[ "$rc" = 0 ] || fail "exit code $rc"
echo "smoke test passed (status query, SIGTERM -> save -> exit 0)"
