#!/bin/sh
# run-cppIsekai.sh [--close|--restart] -- no arg: run in foreground.
# Bare run is singleton-guarded: it refuses when a daemon lives already, so a
# second terminal can never double-bind EADDRINUSE.
# --close/--restart: TERM the daemon first (its shutdown reaps the app-owned
# chrome via killOnDestruct); KILL fallback, then sweep orphaned temp-profile
# chrome and stray Xvfb displays from the xvfb-run wrapper (matched via the
# wrapper's distinctive -server-args string; never a bare Xvfb kill).
BIN=./build/linux/x86_64/release/cppIsekai
CFG=./data/cppisekai.ini
case "${1:-run}" in
  --close)
    pkill -TERM -x cppIsekai 2>/dev/null
    i=0; while pgrep -x cppIsekai >/dev/null && [ $i -lt 10 ]; do sleep 0.3; i=$((i+1)); done
    pgrep -x cppIsekai >/dev/null && pkill -KILL -x cppIsekai
    pkill -TERM -f 'user-data-dir=/tmp/cppisekai_chrome_' 2>/dev/null # orphan sweep; our prefix only
    pkill -TERM -f 'Xvfb :.*1280x1024x24' 2>/dev/null                # orphaned xvfb-run displays
    # process exit != listener freed: graceful shutdown (chrome reap) can
    # outlive the pgrep wait, and the next bind then aborts (seen 17:08 boot
    # 'Address already in use' -> dead daemon). wait the socket out, capped 6s.
    i=0; while ss -ltnH 2>/dev/null | grep -q ':8090 ' && [ $i -lt 15 ]; do sleep 0.4; i=$((i+1)); done
    # the xvfb sidecar tree inherits the daemon's listen fd; orphaned children
    # keep it LISTENing after daemon death -> next bind EADDRINUSE. free it.
    for p in $(ss -lnpH 2>/dev/null | grep ':8090 ' | grep -oE 'pid=[0-9]+' | cut -d= -f2 | sort -u); do kill "$p" 2>/dev/null; done
    ;;
  --restart)
    "$0" --close
    exec "$BIN" --config "$CFG"
    ;;
  *)
    if pgrep -x cppIsekai >/dev/null; then
      echo "cppIsekai already running (pid $(pgrep -x cppIsekai | head -1))."
      echo "  $0 --restart   # stop it and start fresh in this terminal"
      echo "  $0 --close     # just stop it"
      exit 0
    fi
    exec "$BIN" --config "$CFG"
    ;;
esac
