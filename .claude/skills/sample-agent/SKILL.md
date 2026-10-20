---
name: sample-agent
description: Work on the Godot sample's tuning data as its agent - tune the platformer's values, write or run its scenarios, judge them headless with sim and compare, and hand the result to a person. Use when asked to tune, balance, test, or explain the sample game's feel (its jump, run, gravity), to run a scenario, or to explain why an objective fails.
---

# Working on the sample's data

The sample (`sample/`) is a Godot 4.5 platformer whose tuning is Baker data. Its game is
also its own tool: run headless, it checks, explains, simulates and compares. You work where
a technical designer works: in data, never in the game's scripts or scenes.

```
sample/data/tuning.dta            the game's data. Read it; never edit it.
sample/data/agent.dta             YOUR layer, over the data. Every experiment goes here.
sample/data/experiments/*.dta     experiment layers a person toggles (F1). Read only.
user://baker/local.dta            one person's tuning. Never read, write, or rely on it.
sample/scenarios/*.gd             scenarios and their objectives: what "good" means. Propose
                                  changes to an objective by pull request only, never in the
                                  same change as the data it judges.
sample/player.gd, *.tscn          the game. Not yours: its bound properties read the data.
```

## The commands

Build the extension once (README, "Building"). Then, from the repository's root, with
`G="godot --headless --fixed-fps 60 --path sample --"` (Godot 4.5; `--fixed-fps 60` runs the
game's frames as fast as it can, each 1/60 s of game time, so `sim` gives the same results
in a fraction of the time). Godot runs the game in `sample/`, so a
relative path you give (`--out runs/before`, `compare`'s directories, `--patch`) is under
`sample/`. Its banner comes first on stdout: read results from `--out` when you parse them.

| Command | What it tells you |
|---|---|
| `$G --baker reference` | Every script function, global, shape, and pack. `--json` gives the same, plus every tweak: each bound property, its path, type, default and current value. Read it first. |
| `$G --baker check --json` | Errors, load problems, and calls to undefined functions in the data. `passed` false and exit 1 mean fix before anything else. |
| `$G --baker sim [--scenario NAME] [--seeds 1-10] [--out DIR] [--json]` | Runs the scenarios headless (every one unless named), judges each run by its objectives. Exit 0: all passed; 1: an objective failed; 2: something could not run. `--out DIR` writes `DIR/results.json`. |
| `$G --baker compare BEFORE AFTER` | Which objectives moved between two `sim --out` directories, as JSON; exit 1 if one broke. |
| `$G --baker eval --json "{config.explain player.jump.height}"` | A value and every layer that sets it, with file and line; any `{config...}` command, answered as the console answers. |
| `$G --baker diff` | Exactly what `agent.dta` changes: path, old value, new value. |
| `$G --baker promote --patch FILE` | What promotion would write into `tuning.dta`, as a diff a person reviews and applies with `git apply -p1` from `sample/`. It writes nothing else: promotion is a person's decision. |
| `python3 tests/console.py PORT "(1 {cmd})"` | A command in a game a person is running with `-- --console-port PORT`. Use it only when asked to. |

`sim`, `check` and the other commands see `tuning.dta` and `agent.dta`, never `local`, so
results do not depend on whose machine runs them. Nothing in a run reads the clock or draws a
random number: the same data and scenario give the same result on every seed.

## The loop

1. **Read** `$G --baker reference --json` (the tweaks are the values you can tune),
   `sample/data/tuning.dta` (each value's comment says what it does), and the objectives in
   `sample/scenarios/*.gd`.
2. **Baseline:** `$G --baker sim --seeds 1-3 --out runs/before` before you change anything,
   over every scenario. Note what passes.
3. **Change** `sample/data/agent.dta`. Keep each change small and say in a `;` comment why:
   `(player (jump (height 120)))  ; clears the first platform with room`.
4. **`$G --baker check --json`**: exit 0, or fix it.
5. **`$G --baker sim --seeds 1-3 --out runs/after`**, then
   **`$G --baker compare runs/before runs/after`**: it names each objective that broke
   (passed on a scenario and seed, now fails) and exits 1. A fix that breaks another
   objective is not done; that is the most common way a change fails review. `sim`'s exit
   status is the verdict.
6. **Report** to the person: what you changed and why (`$G --baker diff`), which objectives
   moved (quote `compare`), what objectives cannot judge (how the jump feels), and how to
   see it: play the sample from the editor; the Baker dock's Layers tab toggles `agent`.
7. **Stop.** The person plays it, tunes it live, and keeps or drops your values. To promote,
   give them `$G --baker promote --patch agent.patch`; never edit `tuning.dta` yourself.

## The sample

- **Feel:** run (`player.run.speed`, `accel`, `friction`), air control
  (`player.air_control`), and the jump (`player.jump.height`, `time_to_peak`,
  `fall_multiplier`, `coyote`, `buffer`), with `player.max_fall`. Gravity follows from the
  jump's height and time to peak: changing the time changes the gravity, not the height.
- **Scenarios:** `jump` (one held jump peaks at `player.jump.height`, within 2 px) and
  `reach` (a jump from the floor clears the first platform, 100 px up, with 10 px to spare,
  and stays 20 px below the second, 180 px up: the climb takes two jumps).
- **Experiments:** `floaty` (in `data/experiments/`) is a person's A/B, off by default. Do
  not tune it unless asked; a scenario runs with it off.

## Rules

- An objective is a claim a person signed off on. If one seems wrong, say so in your report
  and propose the change separately; never change an objective to make data pass.
- Every claim you make about the game is backed by a run: quote `sim` or `compare`.
- Passing objectives is not the goal; a game that feels right is. Say what the objectives
  cannot see.
