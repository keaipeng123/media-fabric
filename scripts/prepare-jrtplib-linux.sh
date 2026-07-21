#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="${1:-${GB28181_JRTPLIB_SOURCE_DIR:-}}"
BUILD_DIR="${GB28181_JRTPLIB_BUILD_DIR:-$ROOT_DIR/build-jrtplib}"
INSTALL_DIR="${GB28181_JRTPLIB_INSTALL_DIR:-$BUILD_DIR/install}"
OUTPUT_LIB="$ROOT_DIR/3rd/lib/libjrtp.a"

usage() {
    cat <<'USAGE'
Usage:
  scripts/prepare-jrtplib-linux.sh /path/to/jrtplib-3.11.2

Environment:
  GB28181_JRTPLIB_SOURCE_DIR=/path/to/jrtplib-3.11.2
  GB28181_JRTPLIB_BUILD_DIR=build-jrtplib
  GB28181_JRTPLIB_INSTALL_DIR=build-jrtplib/install

Notes:
  - This script is intended for the Linux target environment.
  - It does not download sources; provide a local JRTPLIB source directory.
  - It copies the generated static library to 3rd/lib/libjrtp.a.
USAGE
}

is_linux() {
    [[ "$(uname -s)" == "Linux" ]]
}

require_command() {
    local name="$1"
    if ! command -v "$name" >/dev/null 2>&1; then
        echo "error: missing command $name" >&2
        exit 2
    fi
}

find_static_lib() {
    local root="$1"
    local candidate

    for candidate in \
        "$root/lib/libjrtp.a" \
        "$root/lib64/libjrtp.a" \
        "$root/lib/libjrtplib.a" \
        "$root/lib64/libjrtplib.a"; do
        if [[ -f "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || "${1:-}" == "help" ]]; then
    usage
    exit 0
fi

if ! is_linux; then
    echo "error: prepare-jrtplib-linux requires the Linux target environment." >&2
    echo "current uname: $(uname -s)" >&2
    exit 3
fi

if [[ -z "$SOURCE_DIR" ]]; then
    usage >&2
    exit 1
fi

if [[ ! -d "$SOURCE_DIR" ]]; then
    echo "error: JRTPLIB source directory not found: $SOURCE_DIR" >&2
    exit 1
fi

if [[ ! -f "$SOURCE_DIR/CMakeLists.txt" ]]; then
    echo "error: JRTPLIB source directory does not contain CMakeLists.txt: $SOURCE_DIR" >&2
    exit 1
fi

require_command cmake

mkdir -p "$ROOT_DIR/3rd/lib"

echo "prepare-jrtplib: source=$SOURCE_DIR"
echo "prepare-jrtplib: build=$BUILD_DIR"
echo "prepare-jrtplib: install=$INSTALL_DIR"

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

cmake --build "$BUILD_DIR"
cmake --install "$BUILD_DIR"

if ! static_lib="$(find_static_lib "$INSTALL_DIR")"; then
    echo "error: static JRTPLIB library not found under $INSTALL_DIR" >&2
    echo "expected one of: lib/libjrtp.a, lib64/libjrtp.a, lib/libjrtplib.a, lib64/libjrtplib.a" >&2
    exit 4
fi

cp "$static_lib" "$OUTPUT_LIB"

echo "prepare-jrtplib: copied $static_lib"
echo "prepare-jrtplib: output=$OUTPUT_LIB"
echo "prepare-jrtplib: run MEDIA_FABRIC_PREFLIGHT_STRICT=1 scripts/verify-milestone4-linux.sh preflight"
