#!/usr/bin/env bash
# OpenTibia status-query healthcheck.
# Sends the classic "info" status request (06 00 FF FF "info") and requires a <tsqp>
# serverinfo reply — this proves the game protocol is actually responding, not merely
# that the TCP port is bound.
cd /opt/evolution 2>/dev/null || exit 1
port="$(grep -E '^[[:space:]]*port[[:space:]]*=' config.lua | grep -oE '[0-9]+' | head -1)"
port="${port:-7171}"

exec 3<>"/dev/tcp/127.0.0.1/${port}" 2>/dev/null || exit 1
printf '\x06\x00\xff\xff\x69\x6e\x66\x6f' >&3
resp="$(timeout 3 head -c 256 <&3 2>/dev/null)"
exec 3>&- 2>/dev/null

case "$resp" in
    *"<tsqp"*) exit 0 ;;
    *)         exit 1 ;;
esac
