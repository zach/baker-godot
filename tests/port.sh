#!/usr/bin/env bash
# The skirmish's C# port against the GDScript skirmish: sim over seeds 1-5 with each, the
# same scenarios and objectives, on a copy of sample/ in Godot's .NET build.
#
#   tests/port.sh path/to/godot-mono
#
#  1. every run's outcome is the same, run by run (scenario, seed, how it ended, its steps,
#     each objective's value), and compare says nothing moved;
#  2. a balance change in the agent's layer (a dearer tank) fails both the same way: the
#     port reads the data, it does not hold it.
set -uo pipefail
py="$(command -v python3 || command -v python)"
godot="$1"
here="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
. "$here/tests/copy_sample.sh"
copy_sample "$work"
rm -rf "$work/data/agent.dta"
"$godot" --headless --path "$work" --import >/dev/null 2>&1 || true
"$godot" --headless --path "$work" --import >/dev/null 2>&1

failures=0
check() {  # check CONDITION-STATUS DESCRIPTION
    if [ "$1" -eq 0 ]; then echo "ok   $2"; else echo "FAIL $2"; failures=$((failures + 1)); fi
}
sim() {  # sim OUTDIR [--skirmish cs]: every scenario over seeds 1-5, results in $work/OUTDIR
    local out="$1"; shift
    timeout 300 "$godot" --headless --fixed-fps 60 --path "$work" -- "$@" --baker sim --baker-seeds 1-5 \
        --baker-results "$work/$out" >/dev/null 2>&1
}
same() {  # same A B: the runs of two results files, outcome by outcome
    "$py" - "$work/$1/results.json" "$work/$2/results.json" <<'PY'
import json, sys
def rows(path):
    d = json.load(open(path))
    return [(r["scenario"], r["seed"], r["ended"], r["steps"], r["passed"],
             [(o["name"], o["value"], o["pass"]) for o in r["objectives"]]) for r in d["runs"]]
a, b = rows(sys.argv[1]), rows(sys.argv[2])
for x, y in zip(a, b):
    if x != y:
        print("     differs:", x, "\n              ", y)
sys.exit(0 if a == b and len(a) == 30 else 1)
PY
}

# 1.
sim gd; gd=$?
sim cs --skirmish cs; cs=$?
[ "$gd" -eq 1 ] && [ "$cs" -eq 1 ]; check $? "sim over seeds 1-5 with each skirmish: both exit 1 (the platformer's reach, as shipped)"
same gd cs; check $? "  every run the same, run by run: $(grep -c '"scenario"' "$work/cs/results.json") runs"
timeout 120 "$godot" --headless --path "$work" -- --baker compare --baker-before "$work/gd" \
    --baker-after "$work/cs" --baker-output "$work/cmp.json" >/dev/null 2>&1
check $? "  compare, GDScript to C#, exits 0"
"$py" -c "import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if d['broke']==0 and d['fixed']==0 and all(o['moved']=='same' for o in d['objectives']) else 1)" "$work/cmp.json"
check $? "  and nothing moved"
"$py" -c "import json,sys; d=json.load(open(sys.argv[1])); e=[r['steps'] for r in d['runs'] if r['scenario']=='economy']; sys.exit(0 if len(set(e))>1 else 1)" "$work/cs/results.json"
check $? "  the seeds play differently in C# too: $("$py" -c "import json,sys; print([r['steps'] for r in json.load(open(sys.argv[1]))['runs'] if r['scenario']=='economy'])" "$work/cs/results.json")"

# 2.
printf '(units (tank (price 61)))\n' > "$work/data/agent.dta"
sim gd2; sim cs2 --skirmish cs
same gd2 cs2; check $? "a dearer tank in the agent's layer: the port reads the data, and fails as the GDScript does"
"$py" -c "import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if any(not r['passed'] for r in d['runs'] if r['scenario']=='economy') else 1)" "$work/cs2/results.json"
check $? "  (the economy fails)"

echo "$failures failure(s)"
exit "$failures"
