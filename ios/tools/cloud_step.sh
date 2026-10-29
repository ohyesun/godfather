#!/bin/bash
# Run one cloud build step with a full log plus a compact, paste-friendly digest.
# Usage: bash tools/cloud_step.sh <log-name> <command> [args...]
# Exit code is the wrapped command's exit code. Diagnostics go to build/logs/.
set -uo pipefail
task_root="$(cd "$(dirname "$0")/.." && pwd)"
task_name="${1:?log name required}"
shift
task_logdir="$task_root/build/logs"
mkdir -p "$task_logdir"
task_log="$task_logdir/$task_name.log"
task_digest="$task_logdir/$task_name.errors.txt"

printf '\n===== step: %s =====\n' "$task_name"
"$@" 2>&1 | tee "$task_log"
task_status="${PIPESTATUS[0]}"

if [ "$task_status" -ne 0 ]; then
  {
    printf 'step=%s\nexit=%s\n' "$task_name" "$task_status"
    printf 'xcode=%s\n' "$(xcodebuild -version 2>/dev/null | tr '\n' ' ')"
    printf '\n--- errors (first 80 unique) ---\n'
    grep -nE 'error:|fatal error:' "$task_log" | sort -u | head -80
    printf '\n--- last 40 lines ---\n'
    tail -40 "$task_log"
  } > "$task_digest"
  printf '\n##### %s FAILED (exit %s) #####\n' "$task_name" "$task_status"
  cat "$task_digest"
  exit "$task_status"
fi

printf '##### %s OK #####\n' "$task_name"
