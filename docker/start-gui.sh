#!/bin/sh
set -eu
export XDG_RUNTIME_DIR=/tmp/ascii-runtime
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
cleanup() { kill $(jobs -p) 2>/dev/null || true; }
trap cleanup EXIT INT TERM
Xvfb "$DISPLAY" -screen 0 1440x900x24 -nolisten tcp &
for attempt in $(seq 1 100); do
    [ -S /tmp/.X11-unix/X99 ] && break
    sleep 0.1
done
openbox &
x11vnc -display "$DISPLAY" -localhost -nopw -forever -shared -rfbport 5900 &
websockify --web=/usr/share/novnc 6080 localhost:5900 &
ascii-video-cpp "$@"
