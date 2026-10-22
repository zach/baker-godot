#!/usr/bin/env bash
# Two peers of a lockstep match, as two headless processes on one machine
# (sample/skirmish/peer.gd). On a copy of sample/, so nothing here changes the repository.
#
#   tests/lockstep.sh path/to/godot
#
#  1. peers on the same data agree, on one fingerprint;
#  2. a peer whose data differs (an agent layer the other lacks) is refused, by both. The
#     host is locked before the layer's file appears, so its data stays as it was.
set -uo pipefail
godot="$1"
here="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
host=""
trap 'kill "$host" 2>/dev/null; rm -rf "$work"' EXIT
cp -r "$here/sample/." "$work/"
rm -rf "$work/.godot" "$work/data/agent.dta"
"$godot" --headless --path "$work" --import >/dev/null 2>&1 || true
"$godot" --headless --path "$work" --import >/dev/null 2>&1

failures=0
check() {  # check CONDITION-STATUS DESCRIPTION
    if [ "$1" -eq 0 ]; then echo "ok   $2"; else echo "FAIL $2"; failures=$((failures + 1)); fi
}
start_host() {  # start_host OUT: a host on a free port, its output in $work/OUT; sets $port
    timeout 60 "$godot" --headless --path "$work" res://skirmish/peer.tscn -- --host 0 \
        > "$work/$1" 2>&1 &
    host=$!
    port=""
    for i in $(seq 1 100); do
        port=$(sed -n 's/^listening on \([0-9]*\).*/\1/p' "$work/$1" | head -1)
        [ -n "$port" ] && break
        sleep 0.1
    done
}
join() {  # join OUT: a peer joins the host on $port, its output in $work/OUT
    timeout 60 "$godot" --headless --path "$work" res://skirmish/peer.tscn -- --join "$port" \
        > "$work/$1" 2>&1
}
said() {  # said OUT PATTERN: the line PATTERN matches in $work/OUT
    grep -E "$2" "$work/$1" | head -1
}

# 1.
start_host host1.txt
[ -n "$port" ]; check $? "the host locks its data and listens: port $port"
join join1.txt; joined=$?
wait "$host"; hosted=$?; host=""
[ "$joined" -eq 0 ] && [ "$hosted" -eq 0 ]; check $? "peers on the same data agree (exits $hosted, $joined)"
a=$(said host1.txt '^agreed'); b=$(said join1.txt '^agreed')
[ -n "$a" ] && [ "$a" == "$b" ]; check $? "  on one fingerprint: $a"

# 2.
start_host host2.txt
printf '(units (tank (price 45)))\n' > "$work/data/agent.dta"
join join2.txt; joined=$?
wait "$host"; hosted=$?; host=""
[ "$joined" -eq 1 ] && [ "$hosted" -eq 1 ]; check $? "a peer whose data differs is refused, by both (exits $hosted, $joined)"
grep -q "^refused: mine [0-9a-f]\{16\}, theirs ${a#agreed }" "$work/join2.txt"
check $? "  the host's data stayed as it was locked, though the file appeared"

echo "$failures failure(s)"
exit "$failures"
