#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-build}"
PORT="${1:-9100}"

cmake --build "$BUILD_DIR" --target relay_server --config Release -- -j"$(nproc)"
exec "$BUILD_DIR/relay_server" --port "$PORT"
