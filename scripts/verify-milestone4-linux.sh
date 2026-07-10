#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-default}"
CAPTURE_AUDIT_TARGET="${2:-${GB28181_CAPTURE_AUDIT_TARGET:-}}"
CONFIG="${GB28181_CONFIG:-conf/gb28181-server.conf}"
LAST_CAPTURE_REPORT=""

usage() {
    cat <<'USAGE'
Usage:
  scripts/verify-milestone4-linux.sh preflight
  scripts/verify-milestone4-linux.sh default
  scripts/verify-milestone4-linux.sh pjsip-build
  scripts/verify-milestone4-linux.sh pjsip-smoke
  scripts/verify-milestone4-linux.sh jrtplib-build
  scripts/verify-milestone4-linux.sh jrtplib-smoke
  scripts/verify-milestone4-linux.sh full-build
  scripts/verify-milestone4-linux.sh full-smoke
  scripts/verify-milestone4-linux.sh full-capture
  scripts/verify-milestone4-linux.sh full-capture-pair
  scripts/verify-milestone4-linux.sh completion-gate
  scripts/verify-milestone4-linux.sh capture-audit [report-or-capture-dir]
  scripts/verify-milestone4-linux.sh all-build
  scripts/verify-milestone4-linux.sh capture-help

Environment:
  GB28181_CONFIG=conf/gb28181-server.conf
  GB28181_PREFLIGHT_STRICT=0
  GB28181_SMOKE_SECONDS=5
  GB28181_CAPTURE_SECONDS=10
  GB28181_CAPTURE_IFACE=lo
  GB28181_CAPTURE_DIR=artifacts/milestone4
  GB28181_CAPTURE_AUDIT_TARGET=artifacts/milestone4/milestone4-full-report-<timestamp>.txt

Notes:
  - preflight reports target-environment readiness; set GB28181_PREFLIGHT_STRICT=1 to fail on missing required items.
  - default runs the portable CTest self-test.
  - pjsip/jrtplib/full modes are intended for the Linux target environment.
  - jrtplib and full modes require 3rd/lib/libjrtp.a and 3rd/lib/libjthread.a.
  - completion-gate runs the Linux hard gates required before updating docs/milestone4-completion-audit.md.
  - capture-audit rechecks the latest full-capture report unless a report or capture directory is supplied.
USAGE
}

is_linux() {
    [[ "$(uname -s)" == "Linux" ]]
}

require_linux() {
    if ! is_linux; then
        echo "error: $MODE requires the Linux target environment." >&2
        echo "current uname: $(uname -s)" >&2
        exit 2
    fi
}

config_path() {
    if [[ "$CONFIG" = /* ]]; then
        printf '%s\n' "$CONFIG"
    else
        printf '%s/%s\n' "$ROOT_DIR" "$CONFIG"
    fi
}

media_stream_file() {
    local path="$1"
    awk -F= '
        /^[[:space:]]*stream_file[[:space:]]*=/ {
            value = $2
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
            print value
            exit
        }
    ' "$path"
}

check_file() {
    local label="$1"
    local path="$2"
    if [[ -f "$path" ]]; then
        echo "ok: $label: $path"
        return 0
    fi
    echo "missing: $label: $path"
    return 1
}

check_command() {
    local name="$1"
    if command -v "$name" >/dev/null 2>&1; then
        echo "ok: command $name"
        return 0
    fi
    echo "missing: command $name"
    return 1
}

resolve_path() {
    local path="$1"
    if [[ "$path" = /* ]]; then
        printf '%s\n' "$path"
    else
        printf '%s/%s\n' "$ROOT_DIR" "$path"
    fi
}

file_size() {
    local path="$1"
    if [[ -f "$path" ]]; then
        wc -c <"$path" | tr -d '[:space:]'
    else
        printf 'missing'
    fi
}

pcap_packets() {
    local path="$1"
    if [[ -f "$path" ]]; then
        local count
        set +e
        count="$(tcpdump -nn -r "$path" 2>/dev/null | wc -l | tr -d '[:space:]')"
        local status=$?
        set -e
        if [[ "$status" -ne 0 ]]; then
            printf 'unreadable'
        else
            printf '%s' "$count"
        fi
    else
        printf 'missing'
    fi
}

latest_capture_report() {
    local dir="$1"
    local reports=()
    local latest

    shopt -s nullglob
    reports=("$dir"/milestone4-full-report-*.txt)
    shopt -u nullglob

    if [[ "${#reports[@]}" -eq 0 ]]; then
        return 1
    fi

    latest="$(printf '%s\n' "${reports[@]}" | sort | tail -n 1)"
    printf '%s\n' "$latest"
}

resolve_capture_report() {
    local target="$1"
    local resolved

    if [[ -z "$target" ]]; then
        target="${GB28181_CAPTURE_DIR:-artifacts/milestone4}"
    fi

    resolved="$(resolve_path "$target")"
    if [[ -d "$resolved" ]]; then
        latest_capture_report "$resolved"
    else
        printf '%s\n' "$resolved"
    fi
}

report_value() {
    local report="$1"
    local key="$2"
    awk -F': ' -v key="$key" '$1 == key { print substr($0, index($0, ": ") + 2); exit }' "$report"
}

tshark_count() {
    local pcap="$1"
    local filter="$2"
    local count
    set +e
    count="$(tshark -r "$pcap" -Y "$filter" 2>/dev/null | wc -l | tr -d '[:space:]')"
    local status=$?
    set -e
    if [[ "$status" -ne 0 ]]; then
        printf 'unreadable'
    else
        printf '%s' "$count"
    fi
}

print_tshark_counter() {
    local label="$1"
    local pcap="$2"
    local filter="$3"
    echo "  $label=$(tshark_count "$pcap" "$filter")"
}

run_preflight() {
    local missing=0
    local cfg
    cfg="$(config_path)"

    echo "milestone4 preflight"
    echo "host: $(uname -s)"

    check_command cmake || missing=1
    check_command ctest || missing=1
    check_command c++ || missing=1
    check_command tcpdump || true

    check_file "config" "$cfg" || missing=1
    if [[ -f "$cfg" ]]; then
        local stream_file
        stream_file="$(media_stream_file "$cfg")"
        if [[ -z "$stream_file" ]]; then
            echo "missing: [media] stream_file in $cfg"
            missing=1
        else
            local stream_path="$stream_file"
            if [[ "$stream_path" != /* ]]; then
                stream_path="$ROOT_DIR/$stream_path"
            fi
            check_file "media stream_file" "$stream_path" || missing=1
        fi
    fi

    check_file "PJSIP libpjsip" "$ROOT_DIR/3rd/lib/libpjsip-x86_64-unknown-linux-gnu.a" || missing=1
    check_file "PJSIP libpj" "$ROOT_DIR/3rd/lib/libpj-x86_64-unknown-linux-gnu.a" || missing=1
    check_file "JRTPLIB libjthread" "$ROOT_DIR/3rd/lib/libjthread.a" || missing=1
    check_file "JRTPLIB libjrtp" "$ROOT_DIR/3rd/lib/libjrtp.a" || missing=1

    if [[ "$missing" -eq 0 ]]; then
        echo "preflight: ready"
        return 0
    fi

    echo "preflight: not-ready"
    if [[ "${GB28181_PREFLIGHT_STRICT:-0}" == "1" ]]; then
        return 4
    fi
    return 0
}

run_default() {
    cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -DGB28181_SELF_TEST_CONFIG="$CONFIG"
    cmake --build "$ROOT_DIR/build"
    ctest --test-dir "$ROOT_DIR/build" --output-on-failure -R gb28181-server-self-test
}

run_pjsip_build() {
    require_linux
    cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build-linux-pjsip" -DGB28181_ENABLE_PJSIP=ON
    cmake --build "$ROOT_DIR/build-linux-pjsip"
}

require_jrtplib_libs() {
    if [[ ! -f "$ROOT_DIR/3rd/lib/libjrtp.a" ]]; then
        echo "error: missing $ROOT_DIR/3rd/lib/libjrtp.a" >&2
        echo "build or copy JRTPLIB static library before enabling GB28181_ENABLE_JRTPLIB." >&2
        echo "see docs/third-party-libs.md" >&2
        exit 3
    fi
    if [[ ! -f "$ROOT_DIR/3rd/lib/libjthread.a" ]]; then
        echo "error: missing $ROOT_DIR/3rd/lib/libjthread.a" >&2
        exit 3
    fi
}

run_jrtplib_build() {
    require_linux
    require_jrtplib_libs
    cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build-linux-rtp" -DGB28181_ENABLE_JRTPLIB=ON
    cmake --build "$ROOT_DIR/build-linux-rtp"
}

run_full_build() {
    require_linux
    require_jrtplib_libs
    cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build-linux-full" \
        -DGB28181_ENABLE_PJSIP=ON \
        -DGB28181_ENABLE_JRTPLIB=ON
    cmake --build "$ROOT_DIR/build-linux-full"
}

run_smoke() {
    local binary="$1"
    local label="$2"
    local seconds="${GB28181_SMOKE_SECONDS:-5}"
    local cfg
    local log
    cfg="$(config_path)"
    log="$(mktemp "${TMPDIR:-/tmp}/gb28181-${label}-smoke.XXXXXX.log")"

    if ! command -v timeout >/dev/null 2>&1; then
        echo "error: $label smoke requires GNU timeout on Linux." >&2
        return 5
    fi

    echo "smoke: starting $label for ${seconds}s"
    set +e
    timeout "$seconds" "$binary" -c "$cfg" >"$log" 2>&1
    local status=$?
    set -e

    if grep -q "gb28181-server started" "$log"; then
        echo "smoke: $label started"
        echo "smoke log: $log"
        return 0
    fi

    echo "smoke: $label did not reach started state" >&2
    echo "smoke exit code: $status" >&2
    echo "smoke log: $log" >&2
    return 6
}

run_pjsip_smoke() {
    run_pjsip_build
    run_smoke "$ROOT_DIR/build-linux-pjsip/gb28181-server" "pjsip"
}

run_jrtplib_smoke() {
    run_jrtplib_build
    run_smoke "$ROOT_DIR/build-linux-rtp/gb28181-server" "jrtplib"
}

run_full_smoke() {
    run_full_build
    run_smoke "$ROOT_DIR/build-linux-full/gb28181-server" "full"
}

wait_for_server_started() {
    local log_file="$1"
    local timeout_seconds="${2:-30}"
    for ((i = 0; i < timeout_seconds; i++)); do
        if [[ -f "$log_file" ]] && grep -q "gb28181-server started" "$log_file" 2>/dev/null; then
            return 0
        fi
        sleep 1
    done
    return 1
}

run_full_capture_pair() {
    require_linux
    if ! command -v tcpdump >/dev/null 2>&1; then
        echo "error: full-capture-pair requires tcpdump." >&2
        return 7
    fi

    run_full_build

    local capture_seconds="${GB28181_CAPTURE_SECONDS:-10}"
    local iface="${GB28181_CAPTURE_IFACE:-lo}"
    local pair_dir
    local timestamp
    local sip_pcap
    local rtp_pcap
    local report
    local sup_log
    local sub_log
    local sup_pid
    local sub_pid

    pair_dir="$(resolve_path "${GB28181_CAPTURE_DIR:-artifacts/milestone4}/pair")"
    timestamp="$(date +%Y%m%d-%H%M%S)"
    sip_pcap="$pair_dir/milestone4-pair-sip-$timestamp.pcap"
    rtp_pcap="$pair_dir/milestone4-pair-rtp-$timestamp.pcap"
    report="$pair_dir/milestone4-pair-report-$timestamp.txt"
    sup_log="$pair_dir/sup-$timestamp.log"
    sub_log="$pair_dir/sub-$timestamp.log"

    mkdir -p "$pair_dir"

    cat >"$pair_dir/sup.conf" <<EOF
[node]
sip_id = 10000000002000000001
sip_ip = 127.0.0.1
sip_port = 5061
sip_realm = 1000000000
sip_usr = admin
sip_pwd = admin
rtp_port_begin = 30000
rtp_port_end = 30999

[peer.downstream.1]
sip_id = 11000000002000000001
remote_ip = 127.0.0.1
remote_port = 7101
allow_register = true

[media]
stream_file = $ROOT_DIR/SipSubService/conf/stream.file
rtp_payload_bytes = 1300
rtp_timestamp_increment = 3600
stream_send_interval_ms = 1000
stream_loop = true
EOF

    cat >"$pair_dir/sub.conf" <<EOF
[node]
sip_id = 11000000002000000001
sip_ip = 127.0.0.1
sip_port = 7101
sip_realm = 1000000000
sip_usr = admin
sip_pwd = admin
rtp_port_begin = 31000
rtp_port_end = 31999

[peer.upstream.1]
sip_id = 10000000002000000001
remote_ip = 127.0.0.1
remote_port = 5061
register_to = true
expires = 300
sip_realm = 1000000000
sip_usr = admin
sip_pwd = admin

[media]
stream_file = $ROOT_DIR/SipSubService/conf/stream.file
rtp_payload_bytes = 1300
rtp_timestamp_increment = 3600
stream_send_interval_ms = 1000
stream_loop = true
EOF

    echo "capture-pair: interface=$iface seconds=$capture_seconds"
    echo "capture-pair: dir=$pair_dir"
    echo "capture-pair: sip=$sip_pcap"
    echo "capture-pair: rtp=$rtp_pcap"

    echo "capture-pair: starting tcpdump before nodes to capture full flow"
    set +e
    timeout "$capture_seconds" tcpdump -i "$iface" -w "$sip_pcap" \
        'udp port 5061 or tcp port 5061 or udp port 7101 or tcp port 7101' >/dev/null 2>&1 &
    local sip_pid=$!
    timeout "$capture_seconds" tcpdump -i "$iface" -w "$rtp_pcap" \
        'udp portrange 20000-40000' >/dev/null 2>&1 &
    local rtp_pid=$!
    set -e

    "$ROOT_DIR/build-linux-full/gb28181-server" -c "$pair_dir/sup.conf" >"$sup_log" 2>&1 &
    sup_pid=$!

    if ! wait_for_server_started "$sup_log" 30; then
        echo "error: sup node failed to start within 30s" >&2
        echo "sup log: $sup_log" >&2
        kill "$sup_pid" 2>/dev/null || true
        wait "$sup_pid" 2>/dev/null || true
        return 10
    fi
    echo "capture-pair: sup node started (pid=$sup_pid)"

    "$ROOT_DIR/build-linux-full/gb28181-server" -c "$pair_dir/sub.conf" >"$sub_log" 2>&1 &
    sub_pid=$!

    if ! wait_for_server_started "$sub_log" 30; then
        echo "error: sub node failed to start within 30s" >&2
        echo "sub log: $sub_log" >&2
        kill "$sub_pid" 2>/dev/null || true
        wait "$sub_pid" 2>/dev/null || true
        kill "$sup_pid" 2>/dev/null || true
        wait "$sup_pid" 2>/dev/null || true
        return 10
    fi
    echo "capture-pair: sub node started (pid=$sub_pid)"

    echo "capture-pair: waiting $capture_seconds seconds for traffic"
    sleep "$capture_seconds"

    echo "capture-pair: stopping nodes"
    kill "$sub_pid" 2>/dev/null || true
    wait "$sub_pid" 2>/dev/null || true
    kill "$sup_pid" 2>/dev/null || true
    wait "$sup_pid" 2>/dev/null || true

    set +e
    wait "$sip_pid"
    local sip_status=$?
    wait "$rtp_pid"
    local rtp_status=$?
    set -e

    local capture_status=0
    if [[ "$sip_status" -ne 0 && "$sip_status" -ne 124 ]]; then
        echo "warning: SIP tcpdump exited with $sip_status" >&2
        capture_status=8
    fi
    if [[ "$rtp_status" -ne 0 && "$rtp_status" -ne 124 ]]; then
        echo "warning: RTP tcpdump exited with $rtp_status" >&2
        capture_status=8
    fi
    if [[ ! -s "$sip_pcap" ]]; then
        echo "error: SIP pcap missing or empty: $sip_pcap" >&2
        capture_status=8
    fi
    if [[ ! -s "$rtp_pcap" ]]; then
        echo "error: RTP pcap missing or empty: $rtp_pcap" >&2
        capture_status=8
    fi

    local sip_packets
    local rtp_packets
    sip_packets="$(pcap_packets "$sip_pcap")"
    rtp_packets="$(pcap_packets "$rtp_pcap")"
    if [[ "$sip_packets" == "0" || "$sip_packets" == "missing" || "$sip_packets" == "unreadable" ]]; then
        echo "error: SIP pcap has no readable packets: $sip_pcap" >&2
        capture_status=8
    fi
    if [[ "$rtp_packets" == "0" || "$rtp_packets" == "missing" || "$rtp_packets" == "unreadable" ]]; then
        echo "error: RTP pcap has no readable packets: $rtp_pcap" >&2
        capture_status=8
    fi

    {
        echo "milestone4 full capture pair report"
        echo "timestamp: $timestamp"
        echo "host: $(uname -a)"
        echo "sup_config: $pair_dir/sup.conf"
        echo "sub_config: $pair_dir/sub.conf"
        echo "interface: $iface"
        echo "capture_seconds: $capture_seconds"
        echo "capture_status: $capture_status"
        echo "sip_tcpdump_status: $sip_status"
        echo "rtp_tcpdump_status: $rtp_status"
        echo "sip_pcap: $sip_pcap"
        echo "sip_pcap_bytes: $(file_size "$sip_pcap")"
        echo "sip_pcap_packets: $sip_packets"
        echo "rtp_pcap: $rtp_pcap"
        echo "rtp_pcap_bytes: $(file_size "$rtp_pcap")"
        echo "rtp_pcap_packets: $rtp_packets"
        echo
        echo "manual SIP checks:"
        echo "- REGISTER 401 challenge"
        echo "- REGISTER Authorization retry"
        echo "- REGISTER 200 OK"
        echo "- Keepalive MESSAGE and 200 response"
        echo "- Catalog MESSAGE Query/Response"
        echo "- RecordInfo MESSAGE Query/Response"
        echo "- INVITE 200 OK"
        echo "- ACK with correct Call-ID/CSeq/From tag/To tag"
        echo "- BYE 200 OK"
        echo
        echo "manual RTP checks:"
        echo "- payload type 96"
        echo "- continuous sequence numbers"
        echo "- timestamp progression"
        echo "- marker on frame boundary"
        echo "- PS payload carrying Annex-B H.264/H.265 video PES"
        echo
        echo "logs:"
        echo "- sup log: $sup_log"
        echo "- sub log: $sub_log"
        echo
        echo "docs:"
        echo "- docs/wireshark.md"
        echo "- docs/milestone4-completion-audit.md"
    } >"$report"
    LAST_CAPTURE_REPORT="$report"

    echo "capture-pair: done"
    echo "capture-pair: report=$report"
    echo "capture-pair: inspect with docs/wireshark.md"

    if [[ "$capture_status" -ne 0 ]]; then
        return "$capture_status"
    fi
}

run_full_capture() {
    require_linux
    if ! command -v tcpdump >/dev/null 2>&1; then
        echo "error: full-capture requires tcpdump." >&2
        return 7
    fi

    run_full_build

    local capture_seconds="${GB28181_CAPTURE_SECONDS:-10}"
    local smoke_seconds="${GB28181_SMOKE_SECONDS:-5}"
    local iface="${GB28181_CAPTURE_IFACE:-lo}"
    local capture_dir
    local timestamp
    local sip_pcap
    local rtp_pcap
    local report
    local sip_packets
    local rtp_packets

    capture_dir="$(resolve_path "${GB28181_CAPTURE_DIR:-artifacts/milestone4}")"
    timestamp="$(date +%Y%m%d-%H%M%S)"
    sip_pcap="$capture_dir/milestone4-full-sip-$timestamp.pcap"
    rtp_pcap="$capture_dir/milestone4-full-rtp-$timestamp.pcap"
    report="$capture_dir/milestone4-full-report-$timestamp.txt"

    mkdir -p "$capture_dir"

    echo "capture: interface=$iface seconds=$capture_seconds"
    echo "capture: sip=$sip_pcap"
    echo "capture: rtp=$rtp_pcap"
    echo "capture: report=$report"

    set +e
    timeout "$capture_seconds" tcpdump -i "$iface" -w "$sip_pcap" \
        'udp port 5061 or tcp port 5061 or udp port 7101 or tcp port 7101' >/dev/null 2>&1 &
    local sip_pid=$!
    timeout "$capture_seconds" tcpdump -i "$iface" -w "$rtp_pcap" \
        'udp portrange 20000-40000' >/dev/null 2>&1 &
    local rtp_pid=$!
    set -e

    sleep 1
    set +e
    GB28181_SMOKE_SECONDS="$smoke_seconds" run_smoke "$ROOT_DIR/build-linux-full/gb28181-server" "full-capture"
    local smoke_status=$?
    set -e

    set +e
    wait "$sip_pid"
    local sip_status=$?
    wait "$rtp_pid"
    local rtp_status=$?
    set -e

    local capture_status=0
    if [[ "$sip_status" -ne 0 && "$sip_status" -ne 124 ]]; then
        echo "warning: SIP tcpdump exited with $sip_status" >&2
        capture_status=8
    fi
    if [[ "$rtp_status" -ne 0 && "$rtp_status" -ne 124 ]]; then
        echo "warning: RTP tcpdump exited with $rtp_status" >&2
        capture_status=8
    fi
    if [[ ! -s "$sip_pcap" ]]; then
        echo "error: SIP pcap missing or empty: $sip_pcap" >&2
        capture_status=8
    fi
    if [[ ! -s "$rtp_pcap" ]]; then
        echo "error: RTP pcap missing or empty: $rtp_pcap" >&2
        capture_status=8
    fi

    sip_packets="$(pcap_packets "$sip_pcap")"
    rtp_packets="$(pcap_packets "$rtp_pcap")"
    if [[ "$sip_packets" == "0" || "$sip_packets" == "missing" || "$sip_packets" == "unreadable" ]]; then
        echo "error: SIP pcap has no readable packets: $sip_pcap" >&2
        capture_status=8
    fi
    if [[ "$rtp_packets" == "0" || "$rtp_packets" == "missing" || "$rtp_packets" == "unreadable" ]]; then
        echo "error: RTP pcap has no readable packets: $rtp_pcap" >&2
        capture_status=8
    fi

    {
        echo "milestone4 full capture report"
        echo "timestamp: $timestamp"
        echo "host: $(uname -a)"
        echo "config: $(config_path)"
        echo "interface: $iface"
        echo "capture_seconds: $capture_seconds"
        echo "smoke_seconds: $smoke_seconds"
        echo "smoke_status: $smoke_status"
        echo "capture_status: $capture_status"
        echo "sip_tcpdump_status: $sip_status"
        echo "rtp_tcpdump_status: $rtp_status"
        echo "sip_pcap: $sip_pcap"
        echo "sip_pcap_bytes: $(file_size "$sip_pcap")"
        echo "sip_pcap_packets: $sip_packets"
        echo "rtp_pcap: $rtp_pcap"
        echo "rtp_pcap_bytes: $(file_size "$rtp_pcap")"
        echo "rtp_pcap_packets: $rtp_packets"
        echo
        echo "manual SIP checks:"
        echo "- REGISTER 401 challenge"
        echo "- REGISTER Authorization retry"
        echo "- REGISTER 200 OK"
        echo "- REGISTER replay or nc replay rejected"
        echo "- Keepalive/Catalog/RecordInfo MESSAGE and responses"
        echo "- INVITE 200 OK"
        echo "- ACK Call-ID/CSeq/From tag/To tag/Contact target"
        echo "- BYE 200 and wrong-dialog BYE 481"
        echo
        echo "manual RTP checks:"
        echo "- payload type 96"
        echo "- continuous sequence numbers"
        echo "- timestamp progression"
        echo "- marker on frame boundary"
        echo "- PS payload carrying Annex-B H.264/H.265 video PES"
        echo
        echo "docs:"
        echo "- docs/wireshark.md"
        echo "- docs/milestone4-completion-audit.md"
    } >"$report"
    LAST_CAPTURE_REPORT="$report"

    echo "capture: done"
    echo "capture: report=$report"
    echo "capture: inspect with docs/wireshark.md"

    if [[ "$smoke_status" -ne 0 ]]; then
        return "$smoke_status"
    fi
    if [[ "$capture_status" -ne 0 ]]; then
        return "$capture_status"
    fi
}

run_completion_gate() {
    require_linux

    echo "completion-gate: strict preflight"
    GB28181_PREFLIGHT_STRICT=1 run_preflight

    echo "completion-gate: portable self-test"
    run_default

    echo "completion-gate: PJSIP smoke"
    run_pjsip_smoke

    echo "completion-gate: JRTPLIB smoke"
    run_jrtplib_smoke

    echo "completion-gate: full smoke"
    run_full_smoke

    echo "completion-gate: full SIP/RTP capture"
    run_full_capture

    echo "completion-gate: capture artifact audit"
    CAPTURE_AUDIT_TARGET="$LAST_CAPTURE_REPORT" run_capture_audit

    echo "completion-gate: command gates passed"
    echo "completion-gate: inspect generated pcaps with docs/wireshark.md"
    echo "completion-gate: then update docs/milestone4-completion-audit.md"
}

run_capture_audit() {
    if ! command -v tcpdump >/dev/null 2>&1; then
        echo "error: capture-audit requires tcpdump." >&2
        return 9
    fi

    local report
    if ! report="$(resolve_capture_report "$CAPTURE_AUDIT_TARGET")"; then
        echo "error: no milestone4 full capture report found." >&2
        echo "hint: run scripts/verify-milestone4-linux.sh full-capture first." >&2
        return 9
    fi

    if [[ ! -f "$report" ]]; then
        echo "error: capture report not found: $report" >&2
        return 9
    fi

    local smoke_status
    local capture_status
    local sip_status
    local rtp_status
    local sip_pcap
    local rtp_pcap
    local sip_packets
    local rtp_packets
    local audit_status=0

    smoke_status="$(report_value "$report" "smoke_status")"
    capture_status="$(report_value "$report" "capture_status")"
    sip_status="$(report_value "$report" "sip_tcpdump_status")"
    rtp_status="$(report_value "$report" "rtp_tcpdump_status")"
    sip_pcap="$(report_value "$report" "sip_pcap")"
    rtp_pcap="$(report_value "$report" "rtp_pcap")"

    echo "capture-audit: report=$report"

    if [[ "$smoke_status" != "0" ]]; then
        echo "error: smoke_status=$smoke_status" >&2
        audit_status=9
    fi
    if [[ "$capture_status" != "0" ]]; then
        echo "error: capture_status=$capture_status" >&2
        audit_status=9
    fi
    if [[ "$sip_status" != "0" && "$sip_status" != "124" ]]; then
        echo "error: sip_tcpdump_status=$sip_status" >&2
        audit_status=9
    fi
    if [[ "$rtp_status" != "0" && "$rtp_status" != "124" ]]; then
        echo "error: rtp_tcpdump_status=$rtp_status" >&2
        audit_status=9
    fi

    if [[ ! -s "$sip_pcap" ]]; then
        echo "error: SIP pcap missing or empty: $sip_pcap" >&2
        audit_status=9
    fi
    if [[ ! -s "$rtp_pcap" ]]; then
        echo "error: RTP pcap missing or empty: $rtp_pcap" >&2
        audit_status=9
    fi

    sip_packets="$(pcap_packets "$sip_pcap")"
    rtp_packets="$(pcap_packets "$rtp_pcap")"
    echo "capture-audit: sip_pcap_packets=$sip_packets"
    echo "capture-audit: rtp_pcap_packets=$rtp_packets"

    if [[ "$sip_packets" == "0" || "$sip_packets" == "missing" || "$sip_packets" == "unreadable" ]]; then
        echo "error: SIP pcap has no readable packets: $sip_pcap" >&2
        audit_status=9
    fi
    if [[ "$rtp_packets" == "0" || "$rtp_packets" == "missing" || "$rtp_packets" == "unreadable" ]]; then
        echo "error: RTP pcap has no readable packets: $rtp_pcap" >&2
        audit_status=9
    fi

    if command -v tshark >/dev/null 2>&1; then
        echo "capture-audit: tshark SIP/RTP counters"
        print_tshark_counter "sip" "$sip_pcap" "sip"
        print_tshark_counter "register" "$sip_pcap" 'sip.Method == "REGISTER"'
        print_tshark_counter "message" "$sip_pcap" 'sip.Method == "MESSAGE"'
        print_tshark_counter "invite" "$sip_pcap" 'sip.Method == "INVITE"'
        print_tshark_counter "ack" "$sip_pcap" 'sip.Method == "ACK"'
        print_tshark_counter "bye" "$sip_pcap" 'sip.Method == "BYE"'
        print_tshark_counter "status_401" "$sip_pcap" "sip.Status-Code == 401"
        print_tshark_counter "status_200" "$sip_pcap" "sip.Status-Code == 200"
        print_tshark_counter "status_481" "$sip_pcap" "sip.Status-Code == 481"
        print_tshark_counter "rtp" "$rtp_pcap" "rtp"
    else
        echo "capture-audit: tshark not found; skipping SIP method/status counters"
    fi

    if [[ "$audit_status" -eq 0 ]]; then
        echo "capture-audit: basic capture evidence passed"
        echo "capture-audit: inspect protocol semantics with docs/wireshark.md"
    fi
    return "$audit_status"
}

capture_help() {
    cat <<'CAPTURE'
Wireshark/tshark filters:
  sip
  udp.port == 5061 || tcp.port == 5061 || udp.port == 7101 || tcp.port == 7101
  sip.Method == "REGISTER"
  sip.Method == "MESSAGE"
  sip.Method == "INVITE"
  sip.Method == "BYE"
  sip.Call-ID contains "<call-id>"
  rtp || rtcp
  udp portrange 20000-40000

Linux tcpdump examples:
  sudo tcpdump -i lo -w milestone4-sip.pcap 'udp port 5061 or tcp port 5061 or udp port 7101 or tcp port 7101'
  sudo tcpdump -i lo -w milestone4-rtp.pcap 'udp portrange 20000-40000'

Expected self-test markers:
  REGISTER_AUTH_RETRY=ok
  INVITE_RESPONSE=ok
  INVITE_ACK=ok
  routes=11
  sent_messages=24
  sessions=24

CTest:
  ctest --test-dir build --output-on-failure -R gb28181-server-self-test

Linux smoke:
  GB28181_SMOKE_SECONDS=5 scripts/verify-milestone4-linux.sh pjsip-smoke
  GB28181_SMOKE_SECONDS=5 scripts/verify-milestone4-linux.sh jrtplib-smoke
  GB28181_SMOKE_SECONDS=5 scripts/verify-milestone4-linux.sh full-smoke

Linux capture:
  GB28181_CAPTURE_SECONDS=10 GB28181_CAPTURE_IFACE=lo scripts/verify-milestone4-linux.sh full-capture

Linux completion gate:
  GB28181_CAPTURE_SECONDS=10 GB28181_CAPTURE_IFACE=lo scripts/verify-milestone4-linux.sh completion-gate

Capture artifact audit:
  scripts/verify-milestone4-linux.sh capture-audit artifacts/milestone4
  scripts/verify-milestone4-linux.sh capture-audit artifacts/milestone4/milestone4-full-report-<timestamp>.txt

Expected SIP capture checks:
  REGISTER -> 401 -> REGISTER with Authorization -> 200
  MESSAGE keepalive/Catalog/RecordInfo with 200 responses
  INVITE -> 200 OK -> ACK with same Call-ID/CSeq and From/To tags
  BYE wrong dialog -> 481, valid dialog -> 200

Expected RTP capture checks:
  payload type 96
  continuous sequence numbers
  same timestamp within one frame, increasing across frames
  marker bit on the last RTP packet of a frame
  PS payload carrying Annex-B H.264/H.265 video PES
CAPTURE
}

cd "$ROOT_DIR"

case "$MODE" in
    preflight)
        run_preflight
        ;;
    default)
        run_default
        ;;
    pjsip-build)
        run_pjsip_build
        ;;
    pjsip-smoke)
        run_pjsip_smoke
        ;;
    jrtplib-build)
        run_jrtplib_build
        ;;
    jrtplib-smoke)
        run_jrtplib_smoke
        ;;
    full-build)
        run_full_build
        ;;
    full-smoke)
        run_full_smoke
        ;;
    full-capture)
        run_full_capture
        ;;
    full-capture-pair)
        run_full_capture_pair
        ;;
    completion-gate)
        run_completion_gate
        ;;
    capture-audit)
        run_capture_audit
        ;;
    all-build)
        run_default
        run_pjsip_build
        run_jrtplib_build
        run_full_build
        ;;
    capture-help)
        capture_help
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
