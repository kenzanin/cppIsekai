#!/bin/sh
# Install the .desktop launcher with this checkout's absolute path substituted.
# Keeps the committed template path-independent; rerun after moving the repo.
set -e
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST="${XDG_DATA_HOME:-$HOME/.local/share}/applications/cppIsekai.desktop"
mkdir -p "$(dirname "$DEST")"
sed "s|@APP_DIR@|$ROOT|g" "$ROOT/packaging/cppIsekai.desktop" > "$DEST"
chmod 644 "$DEST"
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$(dirname "$DEST")" >/dev/null 2>&1 || true
fi
echo "installed $DEST (APP_DIR=$ROOT)"
