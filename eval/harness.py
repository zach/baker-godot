#!/usr/bin/env python3
"""The sample's agent skill, tried by real agents: each run plants a fault in a fresh copy
of the sample, gives a headless Claude Code the skill and a person's complaint, and judges
the result by the game, not by the agent.

    python3 eval/harness.py --godot PATH [--runs 3] [--jobs 3] [--model M] [--results CSV]
                            [--skirmish cs]   # the C# port, with Godot's .NET build as PATH

A run succeeds if, afterwards: every file but data/agent.dta (and run output) is byte for
byte as planted; `--baker check` exits 0; `--baker sim` exits 0 over the task's seeds (1-3;
the skirmish's 1-5) and scenarios (the skirmish's: all but the platformer's `reach`); and
the task's own condition holds, which may read the agent's report, or run a scenario of
the judge's own over the data the agent left. Recorded per run: success and its parts,
turns, tool calls, tokens, cost, wall time.
"""
import argparse
import concurrent.futures as cf
import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
SKILL = HERE / ".claude/skills/sample-agent/SKILL.md"
TOOLS = "Bash,Read,Write,Edit,Glob,Grep"


def plant_height(value):
    def plant(sample: Path):
        t = sample / "data/tuning.dta"
        text = t.read_text()
        new = re.sub(r"\(height 96\)", f"(height {value})", text)
        assert new != text
        t.write_text(new)
    return plant


def plant_skirmish(old, new):
    def plant(sample: Path):
        t = sample / "data/skirmish.dta"
        text = t.read_text()
        changed = text.replace(old, new, 1)
        assert changed != text, old
        t.write_text(changed)
    return plant


def plant_all(*plants):
    def plant(sample: Path):
        for p in plants:
            p(sample)
    return plant


def evaluate(sample, godot, commands):
    """Each command's value in the data as planted and changed: {config...} or a rule; None
    for a command that failed (a path the data lacks)."""
    out = Path(tempfile.mkdtemp()) / "eval.json"
    run_godot(godot, sample, ["--baker", "eval", "--baker-json", *commands,
                              "--baker-output", str(out)])
    return [json.loads(line).get("value") for line in out.read_text().splitlines() if line.strip()]


def cards(sample, godot):
    """Every card's cost, damage and upgrade as the game reads them:
    {name: (cost, damage, upgrade)}, the upgrade a card's name or ''."""
    out = Path(tempfile.mkdtemp()) / "tweaks.csv"
    run_godot(godot, sample, ["--baker", "csv", "--baker-output", str(out)])
    rows = {r["path"]: r["value"] for r in csv.DictReader(out.open(encoding="utf-8"))}
    names = {p.split(".")[1] for p in rows if p.startswith("cards.")}
    return {n: (float(rows[f"cards.{n}.cost"]), float(rows[f"cards.{n}.damage"]),
                rows[f"cards.{n}.upgrade"].strip("'")) for n in names}


def cards_fixed(sample, godot, planted, report):
    """No cost changed; a card of cost 3 or more deals 4 a point or more; the rest as they were."""
    now = cards(sample, godot)
    for name, (cost, damage, _) in planted.items():
        c, d, _ = now[name]
        if c != cost or (d < 4 * cost if cost >= 3 else d != damage):
            return False
    return True


def free_cards_upgraded(sample, godot, planted, report):
    """Each free card upgrades into a card of cost 1; no other card's upgrade, and no card's
    cost or damage, changed."""
    now = cards(sample, godot)
    ones = {n for n, (c, _, _) in planted.items() if c == 1}
    for name, (cost, damage, upgrade) in planted.items():
        c, d, u = now[name]
        if (c, d) != (cost, damage):
            return False
        if (u not in ones) if cost == 0 else (u != upgrade):
            return False
    return True


def judge(sample, godot, scenario, seeds):
    """A scenario of the judge's own, with its objectives, run over the data as the agent
    left it: its runs (baker.sim/1.3). Written after the planted files were compared."""
    path = sample / "sim/scenarios/judge.dta"
    path.write_text(scenario)
    out = Path(tempfile.mkdtemp())
    try:
        run_godot(godot, sample, ["--baker", "sim", "--baker-scenario", "judge", "--baker-seeds",
                                  seeds, "--baker-results", str(out)], timeout=600)
    finally:
        path.unlink()
    return json.load((out / "results.json").open())["runs"]


MECH_BOUGHT = """(scenario judge (desc "A match, until blue's army is ready")
   (steps 12)
   (at step 0 {skirmish.start $seed})
   (until {skirmish.outcome})
   (objectives judge.mech_bought))
(objective judge.mech_bought (desc "Blue built a mech")
   (measure {skirmish.built_at mech})
   (pass {< $value 99}))
"""

ARTILLERY_FIRST = """(scenario judge (desc "A match, until blue builds its tank")
   (steps 12)
   (at step 0 {skirmish.start $seed})
   (until {< {skirmish.built_at tank} 99})
   (objectives judge.artillery_first))
(objective judge.artillery_first (desc "Blue built an artillery by the turn it built its tank")
   (measure {skirmish.built_at artillery})
   (pass {< $value 99}))
"""


def mech_added(sample, godot, planted, report):
    """A mech tougher and slower than a tank is in the unit table, and blue buys one in at
    least one of ten matches."""
    hp, move = evaluate(sample, godot, ["{config.get units.mech.hp}", "{config.get units.mech.move}"])
    if hp is None or move is None or not (hp > 10 and move < 6):
        return False
    return any(r["passed"] for r in judge(sample, godot, MECH_BOUGHT, "1-10"))


def artillery_second(sample, godot, planted, report):
    """Blue has built an artillery by the turn it builds its tank, in every match."""
    return all(r["passed"] for r in judge(sample, godot, ARTILLERY_FIRST, "1-5"))


def fall_gravity(sample, godot):
    """The jump's gravity while falling, as the data now sets it: 2h/t^2 times the multiplier."""
    out = Path(tempfile.mkdtemp()) / "eval.json"
    run_godot(godot, sample, ["--baker", "eval", "--baker-json", "{config.get player.jump.height}",
                              "{config.get player.jump.time_to_peak}",
                              "{config.get player.jump.fall_multiplier}", "--baker-output", str(out)])
    h, t, m = (json.loads(line)["value"] for line in out.read_text().splitlines() if line.strip())
    return 2 * h / (t * t) * m


# The skirmish's tasks are judged on every scenario but the platformer's `reach`, which the
# shipped data fails (the platformer tasks' baseline).
SKIRMISH = ["cards", "duel", "economy", "jump", "match"]

SLOW_HQ = plant_skirmish("(hq       (income 20)", "(hq       (income 12)")
SOFT_TANK = plant_skirmish("(tank      (hp 10) (move 6) (attack 10) (armor 2)",
                           "(tank      (hp 10) (move 6) (attack 10) (armor 0)")

TASKS = {
    "reach-short": {
        "plant": None,
        "complaint": "From the floor I can't get up onto the first platform: the jump comes up "
                     "just short. Can you fix the tuning?",
        "condition": None,
    },
    "skips-climb": {
        "plant": plant_height(190),
        "complaint": "One jump from the floor carries me past the first platform straight onto "
                     "the second one. The climb is supposed to take two jumps. Please fix it.",
        "condition": None,
    },
    "floaty-fall": {
        "plant": plant_height(120),
        "complaint": "The jump's height is fine, but the fall feels floaty: after the top of a "
                     "jump I hang in the air too long before landing. Can you make it feel "
                     "snappier?",
        # The fall's gravity went up from the planted data's.
        "snapshot": lambda sample, godot: fall_gravity(sample, godot),
        "condition": lambda sample, godot, planted, report:
            fall_gravity(sample, godot) > planted * 1.05,
    },
    # The skirmish: objectives that bound a value from both sides, so the obvious fix, made
    # large, breaks its twin.
    "slow-army": {
        "plant": SLOW_HQ,
        "complaint": "In the skirmish the economy crawls: the opening's tank shows up around "
                     "turn 8 and the army is never ready in time. Speed it up, but it "
                     "shouldn't turn into a tank rush either.",
        "condition": None,
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
    "fragile-tank": {
        "plant": SOFT_TANK,
        "complaint": "Tanks melt: two infantry hits and a tank is gone. Make them tougher, "
                     "but infantry should still be able to take one down.",
        "condition": None,
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
    "rule-armor": {
        "plant": None,
        "complaint": "Heavy armor makes infantry useless. We want armor to stop only half "
                     "its value in every match, the way the piercing experiment plays, not "
                     "as an experiment. Duels should still feel the way they do now.",
        # Armor 4 stops 2: the rule changed, in every match.
        "condition": lambda sample, godot, planted, report: evaluate(
            sample, godot, ["{skirmish.damage 6 4 0}"]) == [4],
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
    "two-complaints": {
        "plant": plant_all(SLOW_HQ, SOFT_TANK),
        "complaint": "Two things in the skirmish: the economy crawls (the tank comes around "
                     "turn 8, the army is late), and tanks melt to two infantry hits. Fix "
                     "both, without overshooting either way.",
        "condition": None,
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
    # The harder round: content at scale, and a request the objectives forbid.
    "card-scale": {
        "plant": None,
        "complaint": "Expensive cards are a trap: any card that costs 3 or more should deal "
                     "at least 4 damage per point of its cost. Don't change what any card "
                     "costs, and leave the cheaper cards alone.",
        "snapshot": cards,
        "condition": cards_fixed,
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
    # The round on the C# port: a table and a list the match reads whole from a published
    # snapshot, changed in shape and not only in value; records that reference records.
    "new-unit": {
        "plant": None,
        "complaint": "We want a fifth unit in the skirmish, the mech: more hit points than a "
                     "tank but slower, priced near a tank, and the AI should actually buy "
                     "one now and then after its opening. Keep the economy's pacing where "
                     "it is.",
        "condition": mech_added,
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
    "open-artillery": {
        "plant": None,
        "complaint": "Change the AI's opening so it builds an artillery second, between the "
                     "first infantry and the tank. The tank should still land on the turn "
                     "it lands now, and the army should be ready when it is now.",
        "condition": artillery_second,
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
    "free-upgrades": {
        "plant": None,
        "complaint": "The free cards, the ones that cost 0, are dead ends. Give each of them "
                     "an upgrade into a card that costs 1. Don't touch any other card, and "
                     "don't change what any card costs.",
        "snapshot": cards,
        "condition": free_cards_upgraded,
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
    "tank-on-3": {
        "plant": None,
        "complaint": "The skirmish's opening drags. I want the opening's tank out on turn 3, "
                     "every match. Don't change anything else about the economy.",
        # It cannot pass: economy.tank_not_before_turn_4 forbids it. The agent keeps every
        # objective passing and says which one stands in the way.
        "condition": lambda sample, godot, planted, report:
            "tank_not_before_turn_4" in report,
        "scenarios": SKIRMISH,
        "seeds": "1-5",
    },
}

PROMPT = """You are working in a copy of the baker-godot repository (your working directory).
Its sample game is in sample/. Use the `godot` command on PATH. Work only inside this
directory.{port} A person writes:

"{complaint}"
"""

# The skirmish's C# port (--skirmish cs): the game's own arguments, which the judge puts
# first after `--` in every command, and the agent is told to.
GAME_ARGS = []
PORT_NOTE = (" The skirmish's C# port is in use: put `--skirmish cs` first after `--` in every"
             " godot command, so G=\"godot --headless --fixed-fps 60 --path sample -- --skirmish cs\".")


def run_godot(godot, sample, args, timeout=180):
    return subprocess.run([str(godot), "--headless", "--fixed-fps", "60", "--path", str(sample),
                           "--", *GAME_ARGS, *args],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                          timeout=timeout).returncode


def digest(root: Path):
    """Every file but the agent's layer, run output, and Godot's cache."""
    skip = re.compile(r"(^|/)(\.godot|runs)(/|$)|\.patch$|^data/agent\.dta$")
    out = {}
    for p in sorted(root.rglob("*")):
        rel = p.relative_to(root).as_posix()
        if p.is_file() and not skip.search(rel):
            out[rel] = hashlib.sha256(p.read_bytes()).hexdigest()
    return out


def prepare(work: Path, godot: Path, task):
    shutil.copytree(HERE, work / "repo", ignore=shutil.ignore_patterns(
        ".git", "build*", "eval", ".godot", "runs", "*.patch"))
    repo = work / "repo"
    sample = repo / "sample"
    (sample / "data/agent.dta").unlink(missing_ok=True)
    if task["plant"]:
        task["plant"](sample)
    if (sample / "BakerSample.csproj").exists():  # the copy's C#, for Godot's .NET build
        subprocess.run(["dotnet", "build", str(sample / "BakerSample.csproj")],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
    for _ in range(2):  # a fresh headless import can crash on exit; the second is clean
        subprocess.run([str(godot), "--headless", "--path", str(sample), "--import"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300)
    bin_dir = work / "bin"
    bin_dir.mkdir()
    (bin_dir / "godot").symlink_to(godot)
    return repo, sample, bin_dir


def run_one(name, index, godot, model, root):
    task = TASKS[name]
    work = root / f"{name}-{index}"
    work.mkdir(parents=True)
    repo, sample, bin_dir = prepare(work, godot, task)
    planted = digest(sample)
    planted_value = task["snapshot"](sample, godot) if task.get("snapshot") else None
    env = dict(os.environ, PATH=f"{bin_dir}{os.pathsep}{os.environ['PATH']}")
    start = time.time()
    proc = subprocess.run(
        ["claude", "-p", PROMPT.format(complaint=task["complaint"],
                                       port=PORT_NOTE if GAME_ARGS else ""), "--model", model,
         "--append-system-prompt", SKILL.read_text(), "--allowedTools", TOOLS,
         "--max-turns", "60", "--output-format", "stream-json", "--verbose"],
        cwd=repo, env=env, stdin=subprocess.DEVNULL, capture_output=True, text=True,
        timeout=1800)
    wall = time.time() - start
    (work / "transcript.jsonl").write_text(proc.stdout)
    tool_calls, result = 0, {}
    for line in proc.stdout.splitlines():
        try:
            event = json.loads(line)
        except json.JSONDecodeError:
            continue
        if event.get("type") == "assistant":
            tool_calls += sum(1 for c in event["message"].get("content", [])
                              if c.get("type") == "tool_use")
        if event.get("type") == "result":
            result = event
    untouched = digest(sample) == planted
    checked = run_godot(godot, sample, ["--baker", "check"]) == 0
    only = [a for name in task.get("scenarios", []) for a in ("--baker-scenario", name)]
    seeds = task.get("seeds", "1-3")
    simulated = run_godot(godot, sample, ["--baker", "sim", "--baker-seeds", seeds, *only]) == 0
    condition = True
    if task["condition"]:
        try:
            condition = bool(task["condition"](sample, godot, planted_value,
                                               result.get("result", "")))
        except Exception:
            condition = False
    agent = sample / "data/agent.dta"
    usage = result.get("usage", {})
    tokens = sum(usage.get(k, 0) for k in ("input_tokens", "output_tokens",
                                            "cache_read_input_tokens", "cache_creation_input_tokens"))
    return {
        "task": name, "run": index,
        "success": int(untouched and checked and simulated and condition),
        "untouched": int(untouched), "check": int(checked), "sim": int(simulated),
        "condition": int(condition),
        "agent_dta": agent.read_text().replace("\n", " | ").strip() if agent.exists() else "",
        "turns": result.get("num_turns", ""), "tool_calls": tool_calls,
        "tokens": tokens, "output_tokens": usage.get("output_tokens", 0),
        "cost_usd": round(result.get("total_cost_usd", 0.0), 4), "wall_s": round(wall),
        "ended": result.get("subtype", "none"),
        "report": result.get("result", "").replace("\n", " | ")[:2000],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--godot", required=True, type=Path)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--model", default="claude-sonnet-5-5")
    ap.add_argument("--tasks", default=",".join(TASKS))
    ap.add_argument("--results", type=Path, default=HERE / "eval/results.csv")
    ap.add_argument("--work", type=Path, default=Path(tempfile.mkdtemp(prefix="baker-eval-")))
    ap.add_argument("--skirmish", choices=["gd", "cs"], default="gd",
                    help="the GDScript skirmish, or its C# port (Godot's .NET build)")
    args = ap.parse_args()
    if args.skirmish == "cs":
        GAME_ARGS[:] = ["--skirmish", "cs"]
    godot = args.godot.resolve()
    jobs = [(t, i) for t in args.tasks.split(",") for i in range(1, args.runs + 1)]
    rows = []
    with cf.ThreadPoolExecutor(args.jobs) as pool:
        futures = {pool.submit(run_one, t, i, godot, args.model, args.work): (t, i) for t, i in jobs}
        for f in cf.as_completed(futures):
            row = f.result()
            rows.append(row)
            print(json.dumps(row), flush=True)
    rows.sort(key=lambda r: (r["task"], r["run"]))
    with open(args.results, "w", newline="") as out:
        w = csv.DictWriter(out, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    print(f"work: {args.work}")


if __name__ == "__main__":
    main()
