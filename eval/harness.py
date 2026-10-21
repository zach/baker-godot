#!/usr/bin/env python3
"""The sample's agent skill, tried by real agents: each run plants a fault in a fresh copy
of the sample, gives a headless Claude Code the skill and a person's complaint, and judges
the result by the game, not by the agent.

    python3 eval/harness.py --godot PATH [--runs 3] [--jobs 3] [--model M] [--results CSV]

A run succeeds if, afterwards: every file but data/agent.dta (and run output) is byte for
byte as planted; `--baker check` exits 0; `--baker sim --seeds 1-3` exits 0; and the task's
own condition holds. Recorded per run: success and its parts, turns, tool calls, tokens,
cost, wall time.
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


def fall_gravity(sample, godot):
    """The jump's gravity while falling, as the data now sets it: 2h/t^2 times the multiplier."""
    out = Path(tempfile.mkdtemp()) / "eval.json"
    run_godot(godot, sample, ["--baker", "eval", "--json", "{config.get player.jump.height}",
                              "{config.get player.jump.time_to_peak}",
                              "{config.get player.jump.fall_multiplier}", "--out", str(out)])
    h, t, m = (json.loads(line)["value"] for line in out.read_text().splitlines() if line.strip())
    return 2 * h / (t * t) * m


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
        "condition": lambda sample, godot, planted: fall_gravity(sample, godot) > planted * 1.05,
    },
}

PROMPT = """You are working in a copy of the baker-godot repository (your working directory).
Its sample game is in sample/. Use the `godot` command on PATH. Work only inside this
directory. A person writes:

"{complaint}"
"""


def run_godot(godot, sample, args, timeout=180):
    return subprocess.run([str(godot), "--headless", "--fixed-fps", "60", "--path", str(sample),
                           "--", *args],
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
    planted_gravity = fall_gravity(sample, godot) if name == "floaty-fall" else None
    env = dict(os.environ, PATH=f"{bin_dir}{os.pathsep}{os.environ['PATH']}")
    start = time.time()
    proc = subprocess.run(
        ["claude", "-p", PROMPT.format(complaint=task["complaint"]), "--model", model,
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
    simulated = run_godot(godot, sample, ["--baker", "sim", "--seeds", "1-3"]) == 0
    condition = True
    if task["condition"]:
        try:
            condition = bool(task["condition"](sample, godot, planted_gravity))
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
    args = ap.parse_args()
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
