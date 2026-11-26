#!/usr/bin/env bash
set -euo pipefail

usage()
{
    cat <<'EOF'
Usage: ProfileNsight.sh [game arguments...]

Launch the Linux x64 OptDebug build without recording. Load a level, then
press Enter in the same terminal. Recording starts after 3 seconds and lasts
60 seconds. The game stays running after capture.

A51_DATA_ROOT  Directory containing the installed game assets.
A51_BINARY     Override the default OptDebug executable.
NSYS           Override the path to the Nsight Systems CLI.

A51_RENDER_RIGID_INDIRECT_WORKER=0 runs rigid indirect preparation serially
for comparisons with the default worker path.
EOF
}

case "${1:-}" in
    --help|-h) usage; exit 0 ;;
esac
if [[ ! -t 0 ]]; then
    printf 'Run this script in an interactive terminal so Enter can start recording.\n' >&2
    exit 1
fi

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
report_dir="$repo_dir/build/profiling/nsight"

nsys=${NSYS:-nsys}
if [[ -z "${NSYS:-}" ]] && ! command -v nsys >/dev/null; then
    shopt -s nullglob
    for candidate in "$HOME"/nsight-systems-*/bin/nsys; do
        if [[ -x "$candidate" ]]; then
            nsys=$candidate
        fi
    done
fi
if ! command -v -- "$nsys" >/dev/null; then
    printf 'Nsight Systems CLI not found. Add nsys to PATH or set NSYS=/path/to/nsys.\n' >&2
    exit 1
fi

binary=${A51_BINARY:-"$repo_dir/build/linux-gcc-x64/Apps/GameApp/OptDebug/A51"}
if [[ ! -f "$binary" || ! -x "$binary" ]]; then
    printf 'Game executable not found: %s\nBuild it with: cmake --build --preset linux-gcc-x64-optdebug --target A51\n' "$binary" >&2
    exit 1
fi
if [[ -n "${A51_DATA_ROOT:-}" && ! -d "$A51_DATA_ROOT" ]]; then
    printf 'Game assets directory not found: %s\n' "$A51_DATA_ROOT" >&2
    exit 1
fi

mkdir -p -- "$report_dir"
run_dir=$(mktemp -d -- "$report_dir/A51-OptDebug-$(date +%Y%m%d-%H%M%S).XXXXXX")
session=${run_dir##*/}
report="$run_dir/capture.nsys-rep"
launch_log="$run_dir/game.log"
report_log="$run_dir/report.log"

A51_PROFILE_NVTX=${A51_PROFILE_NVTX:-1} setsid --wait \
    "$nsys" launch --session-new="$session" --trace=vulkan,nvtx \
    --vulkan-gpu-workload=batch "$binary" "$@" \
    </dev/null >"$launch_log" 2>&1 &
launcher=$!

deadline=$((SECONDS + 15))
until "$nsys" status --session="$session" >/dev/null 2>&1; do
    if ! kill -0 "$launcher" 2>/dev/null || (( SECONDS >= deadline )); then
        printf 'Game did not start under Nsight. Check %s\n' "$launch_log" >&2
        exit 1
    fi
    sleep 0.2
done

if ! IFS= read -r -p 'Load the level, then press Enter here to arm the 60-second capture: '; then
    printf '\nCapture cancelled before recording.\n' >&2
    exit 1
fi

printf 'Recording starts in 3 seconds. Switch back to the game now.\n'
sleep 3

if ! "$nsys" start --session="$session" --sample=process-tree --backtrace=dwarf \
    --discard-environment=true --output="$run_dir/capture"; then
    printf 'Could not start recording. Check whether the game is still running and see %s\n' "$launch_log" >&2
    exit 1
fi

stop_capture()
{
    printf 'Stopping recording. It is safe to keep playing.\n'
    if "$nsys" stop --session="$session" --defer-report >"$report_log" 2>&1; then
        printf 'Recording finished. Nsight is saving the report; this can take a few minutes.\n'
        if ! "$nsys" finalize --session="$session" >>"$report_log" 2>&1; then
            printf 'Report conversion failed. Raw capture may still be available. Check %s\n' "$report_log" >&2
            return 1
        fi
    elif [[ ! -s "$report" ]]; then
        printf 'Capture could not be stopped cleanly. Check %s\n' "$report_log" >&2
        return 1
    fi
    if [[ ! -s "$report" ]]; then
        printf 'Nsight did not produce a report in %s\n' "$run_dir" >&2
        return 1
    fi
    printf 'Nsight report: %s\n' "$report"
}

trap 'stop_capture; exit 130' INT
trap 'stop_capture; exit 143' TERM
printf 'Recording for 60 seconds. Leave the game running.\n'
sleep 60
trap - INT TERM
stop_capture
