#!/bin/sh
# run-cppIsekai.sh [stop|restart] -- default: run in foreground.
# stop/restart: TERM the daemon first (its shutdown path reaps the app-owned
# lightpanda); KILL fallback after 3s, then sweep any orphaned lightpanda.
BIN=./build/linux/x86_64/release/cppIsekai
CFG=./data/cppisekai.ini
case "${1:-run}" in
  stop|restart)
    pkill -TERM -x cppIsekai 2>/dev/null
    i=0; while pgrep -x cppIsekai >/dev/null && [ $i -lt 10 ]; do sleep 0.3; i=$((i+1)); done
    pgrep -x cppIsekai >/dev/null && pkill -KILL -x cppIsekai
    pkill -TERM -x lightpanda 2>/dev/null   # orphan sweep (KILL path); no-op post-graceful
    [ "$1" = restart ] && exec "$BIN" --config "$CFG"
    ;;
  *)
    exec "$BIN" --config "$CFG"
    ;;
esac
