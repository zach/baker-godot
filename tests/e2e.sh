#!/usr/bin/env bash
# The demonstration, end to end: a headless editor plays the sample, tunes a value as the
# remote inspector does, stops, and keeps it. Runs on a copy of sample/, so Keep's write
# lands in the copy; then checks the copy's diff is one line.
#
#   tests/e2e.sh path/to/godot
set -euo pipefail
godot="$1"
here="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
. "$here/tests/copy_sample.sh"
copy_sample "$work"
rm -rf "$work/.godot"
# The game the editor starts runs headless too.
printf '\n[editor]\n\nrun/main_run_args="--headless"\n' >> "$work/project.godot"
# A fresh headless import can crash on exit (Godot 4.5.1); the second is clean.
"$godot" --headless --path "$work" --import >/dev/null 2>&1 || true
"$godot" --headless --path "$work" --import >/dev/null 2>&1
# The editor's exit status is the number of failures; take it from the pipe's first
# command (after `|| true`, PIPESTATUS would be true's). An error's next line says where.
set +e
BAKER_E2E=1 timeout 120 "$godot" --headless --editor --path "$work" 2>&1 |
  grep -A1 -E '^(ok|FAIL|[0-9]+ failure)|ERROR|SCRIPT ERROR'
status=${PIPESTATUS[0]}
set -e
echo "--- tuning.dta"
diff "$here/sample/data/tuning.dta" "$work/data/tuning.dta" || true
exit "$status"
