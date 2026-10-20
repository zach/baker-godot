#!/usr/bin/env bash
# An agent's loop over the sample, through the surface plan A's bridge drives: the game's
# own tool commands, headless, and its loopback console while it runs. On a copy of
# sample/, so nothing here changes the repository.
#
#   tests/agent.sh path/to/godot
#
#  1. check, and the vocabulary with the bound properties as tweaks;
#  2. the `reach` scenario fails on the shipped data;
#  3. a proposal in agent.dta: check passes, diff shows it, `reach` and `jump` pass;
#  4. A/B in the running game over the console: explain, toggle the agent layer, journal;
#  5. promote --patch writes a one-line patch to tuning.dta and changes nothing; it applies.
#     A patch or --out it cannot write (no directory; on Linux, /dev/full) exits 2.
set -uo pipefail
py="$(command -v python3 || command -v python)"
godot="$1"
here="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'kill "$game" 2>/dev/null; rm -rf "$work"' EXIT
game=""
cp -r "$here/sample/." "$work/"
rm -rf "$work/.godot" "$work/data/agent.dta"
"$godot" --headless --path "$work" --import >/dev/null 2>&1 || true
"$godot" --headless --path "$work" --import >/dev/null 2>&1

failures=0
check() {  # check CONDITION-STATUS DESCRIPTION
    if [ "$1" -eq 0 ]; then echo "ok   $2"; else echo "FAIL $2"; failures=$((failures + 1)); fi
}
tool() {  # tool OUT ARGS...: a tool command, its output in $work/OUT
    local out="$1"; shift
    timeout 60 "$godot" --headless --path "$work" -- --baker "$@" --out "$work/$out" >/dev/null 2>&1
}
scenario() {  # scenario NAME OUT
    timeout 60 "$godot" --headless --path "$work" -- --scenario "$1" --out "$work/$2" >/dev/null 2>&1
}
json() {  # json FILE PYTHON-EXPRESSION-OF-d: exit 0 if true
    "$py" -c "import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if ($2) else 1)" "$work/$1"
}

# 1.
tool check.json check --json; check $? "check passes"
json check.json 'd["schema"].startswith("baker.check/") and d["passed"]'; check $? "  as baker.check JSON"
tool ref.json reference --json; check $? "reference --json"
json ref.json 'any(t["path"] == "player.jump.height" for t in d["tweaks"]["tweaks"])'
check $? "  lists the bound properties as tweaks"

# 2.
scenario reach before.json; status=$?
[ "$status" -eq 1 ]; check $? "reach fails on the shipped data"
json before.json 'd["objectives"][0]["value"] < 110 and d["layers"] == ["default", "agent"]'
check $? "  short of 110 px, run without the local layer"

# 3.
printf '; The agent: a higher jump, to clear the first platform.\n(player (jump (height 120)))\n' \
    > "$work/data/agent.dta"
tool check2.json check --json; check $? "check passes with the proposal"
tool diff.txt diff; grep -q "player.jump.height  96 -> 120" "$work/diff.txt"
check $? "diff: $(grep player "$work/diff.txt" | xargs)"
scenario reach after.json; check $? "reach passes with the proposal"
scenario jump jump.json; check $? "jump peaks at the proposed height"

# 4.
port=$("$py" -c "import socket; s=socket.socket(); s.bind(('127.0.0.1',0)); print(s.getsockname()[1])")
"$godot" --headless --path "$work" -- --console-port "$port" >/dev/null 2>&1 &
game=$!
"$py" "$here/tests/console.py" "$port" \
    '(1 {config.explain player.jump.height})' \
    '(2 {config.toggle agent})' \
    '(3 {config.get player.jump.height})' \
    '(4 {config.toggle agent})' \
    '(5 {config.set player.run.speed 240})' \
    '(6 {config.unsaved})' > "$work/live.txt"
check $? "the console answers"
kill "$game" 2>/dev/null; game=""
"$py" - "$work/live.txt" <<'PY'; check $? "A/B over the console: agent wins, toggles off and on, a live edit is unsaved"
import json, sys
r = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
assert all(x["schema"].startswith("baker.reply/") and x["ok"] for x in r), r
chain = r[0]["value"]
assert [e["layer"] for e in chain if e["standing"] == "wins"] == ["agent"], chain
assert r[2]["value"] == 96, r[2]
assert "player/run/speed" in json.dumps(r[5]["value"]), r[5]
PY

# 5.
before=$(cat "$work/data/tuning.dta")
tool promote.txt promote --to default --patch "$work/agent.patch"; check $? "promote --patch"
[ "$before" == "$(cat "$work/data/tuning.dta")" ]; check $? "  writes no data"
[ "$(grep -c '^[-+] ' "$work/agent.patch")" -eq 2 ]; check $? "  one line changed: $(grep '^+ ' "$work/agent.patch" | xargs)"
fails() {  # fails ARGS...: a tool command that cannot write its output exits 2
    timeout 60 "$godot" --headless --path "$work" -- --baker "$@" >/dev/null 2>&1
    [ $? -eq 2 ]
}
fails promote --patch "$work/no-such-dir/agent.patch"; check $? "  a patch it cannot write fails"
fails diff --out "$work/no-such-dir/diff.txt"; check $? "  so does --out"
if [ -w /dev/full ]; then  # Linux: every write fails, as on a full disk
    fails promote --patch /dev/full; check $? "  so does a full disk"
fi
(cd "$work" && git apply -p1 agent.patch); check $? "  it applies with git apply -p1"
grep -q "(height 120)            ; apex above the takeoff" "$work/data/tuning.dta"
check $? "  and keeps the line's comment"

echo "$failures failure(s)"
exit $((failures > 0))
