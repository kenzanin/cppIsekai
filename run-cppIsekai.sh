#!/bin/sh
# run-cppIsekai.sh [stop|restart] -- default: run in foreground.
# stop/restart: TERM the daemon first (its shutdown reaps the app-owned chrome
# via killOnDestruct); KILL fallback, then sweep orphaned temp-profile chrome
# and stray Xvfb displays from the xvfb-run wrapper (matched via the wrapper's
# distinctive -server-args string; never a bare Xvfb kill).
BIN=./build/linux/x86_64/release/cppIsekai
CFG=./data/cppisekai.ini
case "${1:-run}" in
  stop|restart)
    pkill -TERM -x cppIsekai 2>/dev/null
    i=0; while pgrep -x cppIsekai >/dev/null && [ $i -lt 10 ]; do sleep 0.3; i=$((i+1)); done
    pgrep -x cppIsekai >/dev/null && pkill -KILL -x cppIsekai
    pkill -TERM -f 'user-data-dir=/tmp/cppisekai_chrome_' 2>/dev/null # orphan sweep; our prefix only
    pkill -TERM -f 'Xvfb :.*1280x1024x24' 2>/dev/null                # orphaned xvfb-run displays
    [ "$1" = restart ] && exec "$BIN" --config "$CFG"
    ;;
  *)
    exec "$BIN" --config "$CFG"
    ;;
esac
