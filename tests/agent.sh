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
#  4. A/B in the running game over the console (on a free port): explain, toggle the agent
#     layer, journal;
#  5. promote --baker-patch writes a one-line patch to tuning.dta and changes nothing; it applies.
#     A patch or --baker-output it cannot write (no directory; on Linux, /dev/full) exits 2.
set -uo pipefail
py="$(command -v python3 || command -v python)"
godot="$1"
here="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'kill "$game" 2>/dev/null; rm -rf "$work"' EXIT
game=""
. "$here/tests/copy_sample.sh"
copy_sample "$work"
rm -rf "$work/.godot" "$work/data/agent.dta"
# A probe, for the game's functions: one reads the data through Baker.get_value, as game
# code does; one takes an optional argument. And two enum properties, bound to the data.
cat > "$work/probe.gd" <<'GD'
extends Node
@export_enum("light", "heavy") var attack: String = "light"
@export_enum("Easy", "Normal", "Hard:5") var difficulty: int = 1
func _ready() -> void:
	Baker.bind_function("probe.value", func(path: String) -> Variant: return Baker.get_value(path, -1.0))
	Baker.bind_function("probe.add", add)
	var binding := BakerBinding.new()
	binding.bindings = {"attack": "probe.attack", "difficulty": "probe.difficulty"}
	add_child(binding)
func add(a: float, b: float = 2.0) -> float:
	return a + b
GD
sed -i.bak 's|^Baker="\*res://addons/baker/baker.gd"$|&\nProbe="*res://probe.gd"|' "$work/project.godot"
"$godot" --headless --path "$work" --import >/dev/null 2>&1 || true
"$godot" --headless --path "$work" --import >/dev/null 2>&1

failures=0
check() {  # check CONDITION-STATUS DESCRIPTION
    if [ "$1" -eq 0 ]; then echo "ok   $2"; else echo "FAIL $2"; failures=$((failures + 1)); fi
}
tool() {  # tool OUT ARGS...: a tool command, its output in $work/OUT
    local out="$1"; shift
    timeout 60 "$godot" --headless --path "$work" -- --baker "$@" --baker-output "$work/$out" >/dev/null 2>&1
}
sim() {  # sim OUTDIR: every scenario over seeds 1-3, baker.sim/1.3 in $work/OUTDIR
    timeout 120 "$godot" --headless --fixed-fps 60 --path "$work" -- --baker sim --baker-seeds 1-3 --baker-results "$work/$1" \
        >/dev/null 2>&1
}
compare() {  # compare BEFORE AFTER OUT: baker.compare/1 in $work/OUT
    timeout 60 "$godot" --headless --path "$work" -- --baker compare --baker-before "$work/$1" \
        --baker-after "$work/$2" --baker-output "$work/$3" >/dev/null 2>&1
}
json() {  # json FILE PYTHON-EXPRESSION-OF-d: exit 0 if true
    "$py" -c "import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if ($2) else 1)" "$work/$1"
}

# 1.
tool check.json check --baker-json; check $? "check passes"
json check.json 'd["schema"].startswith("baker.check/") and d["passed"]'; check $? "  as baker.check JSON"
tool ref.json reference --baker-json; check $? "reference --baker-json"
json ref.json 'any(t["path"] == "player.jump.height" for t in d["tweaks"]["tweaks"])'
check $? "  lists the bound properties as tweaks"
json ref.json '[(t["range"], t["step"]) for t in d["tweaks"]["tweaks"] if t["path"] == "player.jump.height"] == [([16, 320], 1)]'
check $? "  with the range and step of each property's @export_range"
json ref.json '[(t["range"], t["value"], t["source"]["layer"]) for t in d["tweaks"]["tweaks"] if t["path"] == "minigames.cannon.gravity"] == [([100, 2000], "700.0", "cannon")]'
check $? "  and those of scenes no one has loaded: the cannon plugin's gravity, from its pack"
tool tweaks.csv csv; check $? "csv: the tweaks, a row each, for a spreadsheet"
"$py" - "$work/tweaks.csv" <<'PY'; check $? "  path, value, range and source, as a spreadsheet reads them"
import csv, sys
rows = {r["path"]: r for r in csv.DictReader(open(sys.argv[1], encoding="utf-8"))}
r = rows["player.jump.height"]
assert (r["value"], r["min"], r["max"], r["layer"], r["file"], r["line"]) == ("96.0", "16.0", "320.0", "default", "data/tuning.dta", "10"), r
assert rows["minigames.cannon.gravity"]["desc"] == "The cannonball's fall, px/s²", rows["minigames.cannon.gravity"]
assert {"bash", "fireball", "strike"} <= set(rows["cards.strike.upgrade"]["one_of"].split()), rows["cards.strike.upgrade"]
PY
# check holds the data to the bound properties' tweaks: a value of the wrong type, or out of
# the property's range, fails it; a misspelled key beside them is a warning.
bad() {  # bad BODY OUT: an agent's proposal, checked into $work/OUT; its exit status
    printf '%s\n' "$1" > "$work/data/agent.dta"
    tool "$2" check --baker-json; local status=$?
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

# The skirmish's tables are held to their shapes: a record with a field of the wrong type,
# out of range, or missing fails check; a field no shape declares is a warning.
# The cards are resources: their tunables are tweaks, a reference's allowed values the
# cards' names, and a curve held to being one.
bad '(cards (strike (upgrade bsh)))' card1.json; [ $? -eq 1 ]; check $? "a card's reference to no card fails check"
json card1.json '[(e["file"], e["line"]) for e in d["tweak_errors"]] == [("data/agent.dta", 1)]'
check $? "  located in agent.dta"
bad '(cards (strike (scaling (3 1) (1 2))))' card2.json; [ $? -eq 1 ]; check $? "  so does a curve whose x goes back"
bad '(cards (strike (upgrade fireball) (scaling (1 1) (2 4) (3 9)) (cost 0)))' card3.json
check $? "  another card, a new curve and a cost pass"
bad '(units (tank (hp "ten")))' shape1.json; [ $? -eq 1 ]; check $? "a unit's field of the wrong type fails check"
json shape1.json '[(e["file"], e["line"]) for e in d["shape_errors"]] == [("data/agent.dta", 1)]'
check $? "  located in agent.dta"
bad '(units (tank (hp 99)))' shape2.json; [ $? -eq 1 ]; check $? "  so does one out of its range"
bad '(units (scout (hp 5)))' shape3.json; [ $? -eq 1 ]; check $? "  and a new unit missing its required fields"
bad '(units (tank (attak 12)))' shape4.json; [ $? -eq 0 ]; check $? "  a field no shape declares passes"
json shape4.json 'any("attak" in w["message"] for w in d["shape_warnings"])'; check $? "    with a warning that names it"
bad '(units (scout (hp 6) (move 8) (attack 4) (armor 0) (price 30)))' shape5.json
check $? "  a whole new record passes"

# The game's functions, from a tool command: reads see the command's data, and a call is held
# to the function's parameters, as check holds the data's calls to them.
timeout 60 "$godot" --headless --path "$work" -- --baker eval --baker-json "{probe.value player.jump.height}" \
    "{player.hold_jump 1 2 3}" "{probe.add 1}" "{probe.add}" --baker-output "$work/calls.txt" >/dev/null 2>&1
"$py" - "$work/calls.txt" <<'PY'; check $? "the game's functions: get_value reads the data in a tool command; a wrong argument count is an error"
import json, sys
r = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
assert r[0]["ok"] and r[0]["value"] == 96, r[0]  # not get_value's fallback, -1
assert not r[1]["ok"], r[1]  # takes none
assert r[2]["ok"] and r[2]["value"] == 3, r[2]  # the default
assert not r[3]["ok"], r[3]  # needs one
PY
cp "$work/sim/objectives.dta" "$work/objectives.dta.bak"
printf '(objective probe.height (measure {probe.value player.jump.height}) (pass {== $value 96}))\n' \
    >> "$work/sim/objectives.dta"
printf '(scenario probe (seconds 0.5) (objectives probe.height))\n' > "$work/sim/scenarios/probe.dta"
timeout 60 "$godot" --headless --fixed-fps 60 --path "$work" -- --baker sim --baker-scenario probe >/dev/null 2>&1
check $? "  and get_value reads the run's data in sim"
rm "$work/sim/scenarios/probe.dta"; mv "$work/objectives.dta.bak" "$work/sim/objectives.dta"
# An enum property's choices are its tweak's allowed values: reference gives them, and check
# holds the data to them.
tool enums.json reference --baker-json
json enums.json '{t["path"]: (t["one_of"], [(c["value"], c["name"]) for c in t.get("choices", [])]) for t in d["tweaks"]["tweaks"] if t["path"].startswith("probe.")} == {"probe.attack": (["light", "heavy"], []), "probe.difficulty": ([], [(0, "Easy"), (1, "Normal"), (5, "Hard")])}'
check $? "an enum property's choices are its tweak's: a String's one_of, an int's named choices"
bad '(probe (attack "medium"))' enum1.json; [ $? -eq 1 ]; check $? "  a string outside them fails check"
bad '(probe (difficulty 3))' enum2.json; [ $? -eq 1 ]; check $? "  so does an int"
bad '(probe (attack "heavy") (difficulty 5))' enum3.json; check $? "  values among them pass"

# 2.
sim before; status=$?
[ "$status" -eq 1 ]; check $? "the baseline: reach fails on the shipped data (sim exits 1)"
json before/results.json 'd["schema"] == "baker.sim/1.3" and d["status"] == "failed"'
check $? "  as baker.sim/1.3 results"
json before/results.json '[(r["scenario"], r["seed"]) for r in d["runs"]] == [(n, s) for n in ("cards", "duel", "economy", "jump", "match", "reach") for s in (1, 2, 3)]'
check $? "  every scenario, each seed a run of its own"
json before/results.json '[o["value"] for r in d["runs"] if r["scenario"] in ("jump", "reach") for o in r["objectives"]] == [96.06] * 9'
check $? "  the same apex in every run: 96.06"
tool layers.json eval --baker-json "{config.layers}"
json layers.json '[l["layer"] for l in d["value"]] == ["default", "plugins", "cannon", "server", "agent", "floaty", "piercing", "scratch"]'
check $? "  the tool commands leave out the local layer"
# A scenario is data: check holds its calls to the game's functions, by name and by their
# parameters.
printf '(scenario typo (seconds 1) (at 0.5 {player.hold_jmup}))\n' > "$work/sim/scenarios/typo.dta"
tool typo.json check --baker-json; [ $? -eq 1 ]; check $? "  a scenario calling a function the game lacks fails check"
json typo.json '[x["name"] for x in d["demand"]] == ["player.hold_jmup"]'
check $? "    which names it"
printf '(scenario args (seconds 1) (at 0.5 {player.hold_jump 1 2 3}) (at 0.6 {player.move}))\n' \
    > "$work/sim/scenarios/typo.dta"
tool args.json check --baker-json; [ $? -eq 1 ]; check $? "  so does one calling a function with the wrong arguments"
json args.json 'len(d["errors"]) == 2 and all(e["file"].endswith("typo.dta") for e in d["errors"])'
check $? "    both calls, located"
rm "$work/sim/scenarios/typo.dta"

# 3.
printf '(player (jump (height 200)))\n' > "$work/data/agent.dta"
sim over; compare before over over.json; status=$?
[ "$status" -eq 1 ]; check $? "a jump of 200: compare exits 1"
json over.json 'd["schema"] == "baker.compare/1" and d["broke"] == 1 and {o["name"]: o["moved"] for o in d["objectives"]} == {"cards.fireball_cost": "same", "cards.strike_upgrades": "same", "duel.tank_holds": "same", "duel.tank_not_a_wall": "same", "economy.tank_by_turn_5": "same", "economy.tank_not_before_turn_4": "same", "economy.army_not_before_turn_6": "same", "economy.army_by_turn_11": "same", "match.turn_seconds": "same", "jump.apex": "same", "reach.first_platform": "fixed", "reach.not_second_platform": "broke"}'
check $? "  it fixed the first platform and broke the second: not done"
printf '; The agent: a higher jump, to clear the first platform.\n(player (jump (height 120)))\n' \
    > "$work/data/agent.dta"
tool check2.json check --baker-json; check $? "a jump of 120: check passes"
tool diff.txt diff; grep -q "player.jump.height  96 -> 120" "$work/diff.txt"
check $? "  diff: $(grep player "$work/diff.txt" | xargs)"
sim after; check $? "  sim passes"
compare before after after.json; check $? "  compare exits 0"
json after.json 'd["broke"] == 0 and d["fixed"] == 1'; check $? "  one objective fixed, none broken"
# A rule, not a number: the agent's layer defines the skirmish's damage rule again.
cp "$work/data/agent.dta" "$work/jump.dta"
cp "$work/data/experiments/piercing.dta" "$work/data/agent.dta"
tool check3.json check --baker-json; check $? "a layer that changes a rule: check passes"
sim rule; [ $? -eq 1 ]; check $? "  sim sees the rule: the duel fails (exits 1)"
compare before rule rule.json
json rule.json '{o["name"]: o["moved"] for o in d["objectives"]}["duel.tank_holds"] == "broke"'
check $? "  compare names the objective the rule broke"
# A balance change, judged in turns: a dearer tank comes a turn late.
printf '(units (tank (price 61)))\n' > "$work/data/agent.dta"
sim turns; [ $? -eq 1 ]; check $? "a dearer tank: the economy scenario fails (exits 1)"
compare before turns turns.json
json turns.json '{o["name"]: o["moved"] for o in d["objectives"]}["economy.tank_by_turn_5"] == "broke"'
check $? "  compare names the objective, judged in turns"
json before/results.json '[r["ended"] for r in d["runs"] if r["scenario"] == "economy"] == ["army_ready"] * 3'
check $? "  on the shipped data, each run ended when the army was ready, not at its length"
cp "$work/jump.dta" "$work/data/agent.dta"

# 4.
"$godot" --headless --path "$work" -- --console-port 0 > "$work/game.txt" 2>&1 &
game=$!  # on a free port, which it prints
port=""
for i in $(seq 1 100); do
    port=$(sed -n 's/^Baker: console on 127\.0\.0\.1:\([0-9]*\).*/\1/p' "$work/game.txt" | head -1)
    [ -n "$port" ] && break
    sleep 0.1
done
[ -n "$port" ]; check $? "the console takes a free port: $port"
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
tool promote.txt promote --baker-to default --baker-patch "$work/agent.patch"; check $? "promote --baker-patch"
[ "$before" == "$(cat "$work/data/tuning.dta")" ]; check $? "  writes no data"
[ "$(grep -c '^[-+] ' "$work/agent.patch")" -eq 2 ]; check $? "  one line changed: $(grep '^+ ' "$work/agent.patch" | xargs)"
fails() {  # fails ARGS...: a tool command that cannot write its output exits 2
    timeout 60 "$godot" --headless --path "$work" -- --baker "$@" >/dev/null 2>&1
    [ $? -eq 2 ]
}
fails promote --baker-patch "$work/no-such-dir/agent.patch"; check $? "  a patch it cannot write fails"
fails diff --baker-output "$work/no-such-dir/diff.txt"; check $? "  so does --baker-output"
if [ -w /dev/full ]; then  # Linux: every write fails, as on a full disk
    fails promote --baker-patch /dev/full; check $? "  so does a full disk"
fi
(cd "$work" && git apply -p1 agent.patch); check $? "  it applies with git apply -p1"
grep -q "(height 120)            ; apex above the takeoff" "$work/data/tuning.dta"
check $? "  and keeps the line's comment"

echo "$failures failure(s)"
exit $((failures > 0))
