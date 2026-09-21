#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ ! -f "$ROOT_DIR/build/chess_gui" ]]; then
    echo "Build not found. Running build first..."
    "$ROOT_DIR/scripts/build_gui.sh"
fi

exec "$ROOT_DIR/build/chess_gui"
