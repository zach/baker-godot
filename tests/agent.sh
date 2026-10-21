#!/usr/bin/env bash
# An agent's loop over the sample, as the agent skill (.claude/skills/sample-agent) runs it:
# the game's own tool commands, headless, and its loopback console while it runs. On a copy
# of sample/, so nothing here changes the repository.
#
#   tests/agent.sh path/to/godot
#
#  1. check, and the vocabulary with the bound properties as tweaks; check fails a value of
#     the wrong type or out of its property's range, and warns of a misspelled key;
#  2. the baseline: sim over every scenario and seeds 1-3; `reach` fails on the shipped data;
#  3. a proposal that overshoots: compare names the objective it broke, and exits 1;
#     the one that does not: check passes, diff shows it, compare names only a fix;
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
sim() {  # sim OUTDIR: every scenario over seeds 1-3, baker.sim/1.1 in $work/OUTDIR
    timeout 120 "$godot" --headless --fixed-fps 60 --path "$work" -- --baker sim --seeds 1-3 --results "$work/$1" \
        >/dev/null 2>&1
}
compare() {  # compare BEFORE AFTER OUT: baker.compare/1 in $work/OUT
    timeout 60 "$godot" --headless --path "$work" -- --baker compare --before "$work/$1" \
        --after "$work/$2" --out "$work/$3" >/dev/null 2>&1
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
json ref.json '[(t["range"], t["step"]) for t in d["tweaks"]["tweaks"] if t["path"] == "player.jump.height"] == [([16, 320], 1)]'
check $? "  with the range and step of each property's @export_range"
# check holds the data to the bound properties' tweaks: a value of the wrong type, or out of
# the property's range, fails it; a misspelled key beside them is a warning.
bad() {  # bad BODY OUT: an agent's proposal, checked into $work/OUT; its exit status
    printf '%s\n' "$1" > "$work/data/agent.dta"
    tool "$2" check --json; local status=$?
    rm -f "$work/data/agent.dta"
    return $status
}
bad '(player (jump (height "tall")))' bad1.json; [ $? -eq 1 ]
check $? "  a string for a float fails check (exit 1)"
json bad1.json 'not d["passed"] and [(e["file"], e["line"]) for e in d["tweak_errors"]] == [("data/agent.dta", 1)]'
check $? "    located in agent.dta"
bad '(player (jump (height 1000)))' bad2.json; [ $? -eq 1 ]
check $? "  a height outside its range fails check: $("$py" -c 'import json,sys; print(json.load(open(sys.argv[1]))["tweak_errors"][0]["message"])' "$work/bad2.json" 2>/dev/null)"
bad '(player (jump (hieght 120)))' bad3.json; [ $? -eq 0 ]
check $? "  a misspelled key passes check"
json bad3.json 'd["passed"] and any("hieght" in w["message"] for w in d["tweak_warnings"])'
check $? "    with a warning that names it"

# 2.
sim before; status=$?
[ "$status" -eq 1 ]; check $? "the baseline: reach fails on the shipped data (sim exits 1)"
json before/results.json 'd["schema"] == "baker.sim/1.1" and d["status"] == "failed"'
check $? "  as baker.sim/1.1 results"
json before/results.json '[(r["scenario"], r["seed"]) for r in d["runs"]] == [("jump", s) for s in (1, 2, 3)] + [("reach", s) for s in (1, 2, 3)]'
check $? "  every scenario, each seed a run of its own"
json before/results.json '[o["value"] for r in d["runs"] for o in r["objectives"]] == [96.06] * 9'
check $? "  the same apex in every run: 96.06"
tool layers.json eval --json "{config.layers}"
json layers.json '[l["layer"] for l in d["value"]] == ["default", "agent", "floaty", "scratch"]'
check $? "  the tool commands leave out the local layer"
# A scenario is data: check holds its calls to the game's functions.
cp "$work/sim/scenarios.dta" "$work/scenarios.dta.bak"
printf '(scenario typo (seconds 1) (at 0.5 {player.hold_jmup}))\n' >> "$work/sim/scenarios.dta"
tool typo.json check --json; [ $? -eq 1 ]; check $? "  a scenario calling a function the game lacks fails check"
json typo.json '[x["name"] for x in d["demand"]] == ["player.hold_jmup"]'
check $? "    which names it"
mv "$work/scenarios.dta.bak" "$work/sim/scenarios.dta"

# 3.
printf '(player (jump (height 200)))\n' > "$work/data/agent.dta"
sim over; compare before over over.json; status=$?
[ "$status" -eq 1 ]; check $? "a jump of 200: compare exits 1"
json over.json 'd["schema"] == "baker.compare/1" and d["broke"] == 1 and {o["name"]: o["moved"] for o in d["objectives"]} == {"jump.apex": "same", "reach.first_platform": "fixed", "reach.not_second_platform": "broke"}'
check $? "  it fixed the first platform and broke the second: not done"
printf '; The agent: a higher jump, to clear the first platform.\n(player (jump (height 120)))\n' \
    > "$work/data/agent.dta"
tool check2.json check --json; check $? "a jump of 120: check passes"
tool diff.txt diff; grep -q "player.jump.height  96 -> 120" "$work/diff.txt"
check $? "  diff: $(grep player "$work/diff.txt" | xargs)"
sim after; check $? "  sim passes"
compare before after after.json; check $? "  compare exits 0"
json after.json 'd["broke"] == 0 and d["fixed"] == 1'; check $? "  one objective fixed, none broken"

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
