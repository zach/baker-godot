#!/usr/bin/env bash
# An exported release build: the shipping library, the data packed and read as text.
# Exports a copy of sample/ with the preset given, runs its scenarios, and checks the data's
# values arrived (the jump peaks at tuning.dta's height, not the scene's) and that the game
# ran as a shipping build. Needs Godot's export template for the preset's platform
# installed, and the template_release library built.
#
#   tests/export.sh path/to/godot [Linux|Windows]
set -uo pipefail
py="$(command -v python3 || command -v python)"
godot="$1"
preset="${2:-Linux}"
here="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/project"
cp -r "$here/sample/." "$work/project/"
rm -rf "$work/project/.godot"
"$godot" --headless --path "$work/project" --import >/dev/null 2>&1 || true
"$godot" --headless --path "$work/project" --import >/dev/null 2>&1
mkdir -p "$work/out"
case "$preset" in
    Windows) game="$work/out/baker-platformer.exe" ;;
    *) game="$work/out/baker-platformer.x86_64" ;;
esac
timeout 300 "$godot" --headless --path "$work/project" --export-release "$preset" "$game" 2>&1 \
    | grep -E "ERROR|error" | grep -v "^$" | head -5
failures=0
check() {
    if [ "$1" -eq 0 ]; then echo "ok   $2"; else echo "FAIL $2"; failures=$((failures + 1)); fi
}
[ -f "$game" ]; check $? "exports $preset"
timeout 60 "$game" --headless -- --scenario jump --out "$work/sim" >/dev/null 2>&1
check $? "the exported game runs the jump scenario"
"$py" - "$work/sim/results.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
print("     ", json.dumps(d["host"]), json.dumps(d["runs"][0]["objectives"]))
assert d["status"] == "passed" and d["host"]["shipping"], d
assert d["host"]["layers"] == ["default", "agent"], d  # a scenario leaves out local, in exports too
assert abs(d["runs"][0]["objectives"][0]["value"] - 96) <= 2, d  # tuning.dta's, not the scene's 80
PY
check $? "  a shipping build, tuning.dta's values read from the package, without local"
echo "$failures failure(s)"
exit $((failures > 0))
