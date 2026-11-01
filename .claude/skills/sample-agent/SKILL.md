---
name: sample-agent
description: Work on the Godot sample's tuning data as its agent - tune the platformer's values, write or run its scenarios, judge them headless with sim and compare, and hand the result to a person. Use when asked to tune, balance, test, or explain the sample game's feel (its jump, run, gravity), to run a scenario, or to explain why an objective fails.
---

# Working on the sample's data

The sample (`sample/`) is a Godot 4.7 platformer whose tuning is Baker data. Its game is
also its own tool: run headless, it checks, explains, simulates and compares. You work where
a technical designer works: in data, never in the game's scripts or scenes.

```
sample/data/tuning.dta            the game's data. Read it; never edit it.
sample/data/agent.dta             YOUR layer, over the data. Every experiment goes here.
sample/data/experiments/*.dta     experiment layers a person toggles (F1). Read only.
user://baker/local.dta            one person's tuning. Never read, write, or rely on it.
sample/sim/scenarios/*.dta        the scenarios, one a file: what a run does, as Baker data.
sample/sim/objectives.dta         the objectives: what "good" means. Propose a change to an
                                  objective or a scenario by pull request only, never in the
                                  same change as the data it judges.
sample/sim/driver.gd              the player's functions scenarios call ({player.hold_jump},
                                  {player.apex}, ...). Not yours.
sample/player.gd, *.tscn          the game. Not yours: its bound properties read the data.
```

## The commands

Build the extension once (README, "Building"). Then, from the repository's root, with
`G="godot --headless --fixed-fps 60 --path sample --"` (Godot 4.7; `--fixed-fps 60` runs the
game's frames as fast as it can, each 1/60 s of game time, so `sim` gives the same results
in a fraction of the time). Godot runs the game in `sample/`, so a
relative path you give (`--baker-results runs/before`, `compare`'s directories, `--baker-output`, `--baker-patch`)
is under `sample/`. Its banner comes first on stdout: read results from a file when you parse
them (`--baker-output runs/NAME` writes any command's output to one). Keep every file you write, other
than `data/agent.dta`, under `runs/`, which version control ignores: a stray file elsewhere
in the project is a change a person has to clean up.

| Command | What it tells you |
|---|---|
| `$G --baker reference` | Every script function, global, shape, and pack. `--baker-json` gives the same, plus every tweak: each bound property, its path, type, default and current value, and the range and step the game declares for it. Read it first, and keep a value you propose within its range: `check` fails a value outside it, or of the wrong type, and warns of a key no tweak reads, which is how a misspelling looks. |
| `$G --baker check --baker-json` | Errors, load problems, and calls to undefined functions in the data, the scenarios and the objectives. `passed` false and exit 1 mean fix before anything else. |
| `$G --baker sim [--baker-scenario NAME] [--baker-seeds 1-10] [--baker-results DIR] [--baker-json]` | Runs the scenarios headless (every one unless named), each from a fresh scene, and judges each run by its objectives. Exit 0: all passed; 1: an objective failed; 2: something could not run. `--baker-results DIR` writes `DIR/results.json` (`baker.sim/1.3`), which `compare` reads. |
| `$G --baker compare --baker-before DIR --baker-after DIR` | Which objectives moved between two `sim --baker-results` directories, as `baker.compare/1` JSON; exit 1 if one broke. |
| `$G --baker eval --baker-json "{config.explain player.jump.height}"` | A value and every layer that sets it, with file and line; any `{config...}` command, answered as the console answers. |
| `$G --baker diff` | Exactly what `agent.dta` changes: path, old value, new value. |
| `$G --baker csv` | Every tweak a row: path, value, default, range, allowed values, file and line. Quicker to scan than the JSON when you want every value at once. |
| `$G --baker promote --baker-patch FILE` | What promotion would write into `tuning.dta`, as a diff a person reviews and applies with `git apply -p1` from `sample/`. It writes nothing else: promotion is a person's decision. |
| `python3 tests/console.py PORT "(1 {cmd})"` | A command in a game a person is running with `-- --console-port PORT` (`0`: a free port, which the game prints). Use it only when asked to. |

`sim`, `check` and the other commands see `tuning.dta` and `agent.dta`, never `local`, so
results do not depend on whose machine runs them. Nothing in a run reads the clock or draws a
random number: the same data and scenario give the same result on every seed.

## The loop

1. **Read** `$G --baker reference --baker-json` (the tweaks are the values you can tune),
   `sample/data/tuning.dta` (each value's comment says what it does), and the scenarios and
   objectives in `sample/sim/`.
2. **Baseline:** `$G --baker sim --baker-seeds 1-3 --baker-results runs/before` before you change anything,
   over every scenario. Note what passes.
3. **Change** `sample/data/agent.dta`. Keep each change small and say in a `;` comment why:
   `(player (jump (height 120)))  ; clears the first platform with room`.
4. **`$G --baker check --baker-json`**: exit 0, or fix it.
5. **`$G --baker sim --baker-seeds 1-3 --baker-results runs/after`**, then
   **`$G --baker compare --baker-before runs/before --baker-after runs/after`**: it names each objective that broke
   (passed on a scenario and seed, now fails) and exits 1. A fix that breaks another
   objective is not done; that is the most common way a change fails review. `sim`'s exit
   status is the verdict.
6. **Report** to the person: what you changed and why (`$G --baker diff`), which objectives
   moved (quote `compare`), what objectives cannot judge (how the jump feels), and how to
   see it: play the sample from the editor; the Baker dock's Layers tab toggles `agent`.
7. **Stop.** The person plays it, tunes it live, and keeps or drops your values. To promote,
   give them `$G --baker promote --baker-patch agent.patch`; never edit `tuning.dta` yourself.

## The sample

- **Feel:** run (`player.run.speed`, `accel`, `friction`), air control
  (`player.air_control`), and the jump (`player.jump.height`, `time_to_peak`,
  `fall_multiplier`, `coyote`, `buffer`), with `player.max_fall`. Gravity follows from the
  jump's height and time to peak: changing the time changes the gravity, not the height.
- **Scenarios:** `jump` (one held jump peaks at `player.jump.height`, within 2 px),
  `reach` (a jump from the floor clears the first platform, 100 px up, with 10 px to spare,
  and stays 20 px below the second, 180 px up: the climb takes two jumps), `duel`
  (infantry takes 3 or 4 hits to destroy a tank), `cards` (the cards read the data), and
  `economy` (a match in turns: the opening's tank on turn 4 or 5, an army of
  `economy.army_ready` units on turn 6 to 11). The economy: each turn a side earns its
  buildings' `income` less its units' `upkeep`, builds its opening (`ai.opening`), then buys
  one thing or saves, drawn by its seed with `ai.weights` (units, buildings with a `cost`,
  `save`). After the opening, seeds play differently: judge a change over `--baker-seeds
  1-5` or more, never one seed, and do not expect to work an outcome out by hand. Several
  objectives bound a value from both sides: a change large enough to fix one often breaks
  its twin, so aim inside the window and let `compare` say. If what a person asks cannot
  pass every objective, do not make it pass some: say which objective stands in the way,
  and propose the change to that objective separately.
- **Tables and lists:** `units` and `buildings` are keyed tables, which layers merge by
  name: `(units (tank (armor 3)))` retunes one field, and `(units (mech (hp 16) (move 3)
  (attack 9) (armor 2) (price 55) (upkeep 3)))` adds a record, which `check` holds to the
  `#shape` in `skirmish.dta` (every field the shape requires, each in its range); give a
  new kind a weight in `ai.weights` or the AI never buys it. `ai.opening` is a list: a layer
  restates it whole, `(ai (opening infantry artillery tank))`, or changes it in place,
  `(ai (opening #append heli))` or `#drop`. The game reads a table or a list whole, so a
  record a layer adds, or a list it reorders, is what the match plays.
- **Rules:** the skirmish's damage is a script function in `data/skirmish.dta`, defined in
  its `#autorun`. To change a rule rather than a number, define it again in `agent.dta`'s
  own `#autorun`, whole, with its `(@doc ...)`; `sim` judges it as it judges a value.
- **Cards:** each card is a resource, `sample/skirmish/cards/NAME.tres` (read them; never
  edit them). Its tuning is `cards.NAME.cost`, `damage`, `upgrade` (another card's name) and
  `scaling` (a curve, `(scaling (1 1) (3 2))`): `reference --baker-json` lists each with its
  file's value. Retune a card in `agent.dta` by name, as any value.
- **The skirmish's C# port** (`sample/skirmish/cs/`): the same data, scenarios and
  objectives, the same results. `-- --skirmish cs --baker ...` runs it (Godot's .NET build);
  use it only when asked to, and then in every command.
- **Experiments:** `floaty` and `piercing` (in `data/experiments/`) are a person's A/B, off
  by default. Do not tune them unless asked; a scenario runs with them off.

## Rules

- An objective is a claim a person signed off on. If one seems wrong, say so in your report
  and propose the change separately; never change an objective to make data pass.
- Every claim you make about the game is backed by a run: quote `sim` or `compare`.
- Passing objectives is not the goal; a game that feels right is. Say what the objectives
  cannot see.
