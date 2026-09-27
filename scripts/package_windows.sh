#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-build-win}"
QT_MINGW_PREFIX="${QT_MINGW_PREFIX:-/opt/qt/6.8.3/mingw_64}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-cmake/toolchain-mingw-w64.cmake}"
DIST_DIR="${DIST_DIR:-chess_windows_release}"

if [[ ! -d "$QT_MINGW_PREFIX" ]]; then
    echo "Qt for Windows (mingw) not found at $QT_MINGW_PREFIX" >&2
    echo "Install it first, e.g.:" >&2
    echo "  pip install aqtinstall" >&2
    echo "  python3 -m aqt install-qt windows desktop 6.8.3 win64_mingw -O /opt/qt" >&2
    echo "  python3 -m aqt install-qt linux desktop 6.8.3 linux_gcc_64 -O /opt/qt" >&2
    exit 1
fi

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
cmake --build "$BUILD_DIR" --config Release -- -j"$(nproc)"

rm -rf "$DIST_DIR/platforms"
mkdir -p "$DIST_DIR/platforms"
rm -f "$DIST_DIR"/*.exe "$DIST_DIR"/*.dll

cp "$BUILD_DIR/chess_gui.exe" "$DIST_DIR/"
cp "$BUILD_DIR/chess_console.exe" "$DIST_DIR/"
cp "$BUILD_DIR/relay_server.exe" "$DIST_DIR/"
cp "$ROOT_DIR/README.md" "$DIST_DIR/README.md"

for dll in Qt6Core.dll Qt6Gui.dll Qt6Network.dll Qt6Widgets.dll libwinpthread-1.dll libgcc_s_seh-1.dll libstdc++-6.dll; do
    cp "$QT_MINGW_PREFIX/bin/$dll" "$DIST_DIR/"
done

cp "$QT_MINGW_PREFIX/plugins/platforms/qwindows.dll" "$DIST_DIR/platforms/"

(cd "$(dirname "$DIST_DIR")" && zip -qr "${DIST_DIR}.zip" "$(basename "$DIST_DIR")")

echo "Windows package created: $ROOT_DIR/$DIST_DIR (and ${DIST_DIR}.zip)"
