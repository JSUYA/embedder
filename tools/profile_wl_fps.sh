#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   tools/profile_wl_fps.sh <pid|process_name> [seconds]
# Example:
#   tools/profile_wl_fps.sh flutter-tizen-app 15

TARGET="${1:-}"
DURATION="${2:-15}"

if [[ -z "$TARGET" ]]; then
  echo "Usage: $0 <pid|process_name> [seconds]" >&2
  exit 1
fi

if [[ "$TARGET" =~ ^[0-9]+$ ]]; then
  PID="$TARGET"
else
  PID="$(pidof "$TARGET" | awk '{print $1}')"
fi

if [[ -z "${PID:-}" ]]; then
  echo "Could not resolve pid for target: $TARGET" >&2
  exit 1
fi

OUT_DIR="${OUT_DIR:-/tmp/wl-fps-profile-$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$OUT_DIR"

echo "[profile] pid=$PID duration=${DURATION}s out=$OUT_DIR"

echo "[profile] perf top hotspots"
perf record -F 99 -g -p "$PID" -- sleep "$DURATION" || true
perf report --stdio > "$OUT_DIR/perf-report.txt" || true
mv perf.data "$OUT_DIR/" 2>/dev/null || true

echo "[profile] sched + drm + gpu trace"
trace-cmd record -o "$OUT_DIR/trace.dat" \
  -e sched:sched_switch \
  -e sched:sched_wakeup \
  -e drm:drm_vblank_event \
  -e irq:irq_handler_entry \
  -e irq:irq_handler_exit \
  sleep "$DURATION" || true

trace-cmd report "$OUT_DIR/trace.dat" > "$OUT_DIR/trace-report.txt" || true

echo "[done] $OUT_DIR"
