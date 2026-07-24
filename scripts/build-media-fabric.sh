#!/usr/bin/env sh
set -eu

# Builds the C++ media core first, then links it into the Go host. The output
# is one executable: <build-dir>/media-fabric.
if ! command -v go >/dev/null 2>&1; then
    echo "error: Go 1.23 or newer is required to link the final media-fabric binary." >&2
    echo "hint: install Go, ensure the go command is on PATH, then rerun this script." >&2
    exit 1
fi

build_dir="${MEDIA_FABRIC_BUILD_DIR:-build}"
cmake -S . -B "$build_dir" "$@"
cmake --build "$build_dir" --target media_fabric_core mfcli

root_dir=$(pwd)
case "$build_dir" in
    /*) output_path="$build_dir/media-fabric" ;;
    *) output_path="$root_dir/$build_dir/media-fabric" ;;
esac
build_library_dir=$(dirname "$output_path")
core_flags="-L${build_library_dir} -lmedia_fabric_core -lgb28181_common -L${root_dir}/3rd/lib -ltinyxml2 -ljrtp -ljthread -ljsoncpp -lpjsua2-x86_64-unknown-linux-gnu -lpjsua-x86_64-unknown-linux-gnu -lpjsip-ua-x86_64-unknown-linux-gnu -lpjsip-simple-x86_64-unknown-linux-gnu -lpjsip-x86_64-unknown-linux-gnu -lpjmedia-codec-x86_64-unknown-linux-gnu -lpjmedia-videodev-x86_64-unknown-linux-gnu -lpjmedia-audiodev-x86_64-unknown-linux-gnu -lpjmedia-x86_64-unknown-linux-gnu -lpjnath-x86_64-unknown-linux-gnu -lpjlib-util-x86_64-unknown-linux-gnu -lsrtp-x86_64-unknown-linux-gnu -lresample-x86_64-unknown-linux-gnu -lgsmcodec-x86_64-unknown-linux-gnu -lspeex-x86_64-unknown-linux-gnu -lilbccodec-x86_64-unknown-linux-gnu -lg7221codec-x86_64-unknown-linux-gnu -lyuv-x86_64-unknown-linux-gnu -lwebrtc-x86_64-unknown-linux-gnu -lpj-x86_64-unknown-linux-gnu -lpjsdp-x86_64-unknown-linux-gnu -lasound -lssl -lcrypto -lpthread -lm"

(cd go && CGO_ENABLED=1 CGO_LDFLAGS="$core_flags" \
    go build -ldflags='-linkmode=external' -o "$output_path" ./cmd/media-fabric)
