# Evaluation: real agents with the sample's skill

Run on 2026-10-20 against this repository at 383756f (Baker 0.1.1, Godot 4.5.1, Linux),
with headless Claude Code (2.1.304), model `claude-sonnet-5-5`.

**Result: 9 of 9 runs succeeded,** each in 3 to 5 tool calls (3.3 on average) and about
30 seconds, for $0.96 in all.

**Rerun on Baker 0.2.0** (81c9547, the runtime on Baker's host loop), same protocol: 9 of 9
again, in 2 to 5 tool calls (3.1 on average), for $1.04. That run's rows are not in
`results.csv`, which keeps the first.

**Rerun on Baker 0.2.1** (992464b: Baker's own tool commands, and `sim` under
`--fixed-fps 60`), same protocol: 9 of 9, in 3 to 4 tool calls (3.1 on average), 17 s a
run on average instead of about 31, for $0.95. Its rows are not in `results.csv` either.

**Rerun on Baker 0.4.0** (410cca1: the scenarios and objectives as Baker data, run by
Baker's `sim` and `compare`, with `--results` and `--before`/`--after`), same protocol: 9 of
9, in 3 to 5 tool calls (3.7 on average), 17 s a run, for $1.12. Not in `results.csv`.

**Rerun on Baker 0.5.0** (fb97833), same protocol: first 8 of 9. The run that failed had the
right data (check, sim and the task's condition passed) but left `sample/ref.json`, a
`reference --json --out ref.json` it parsed, so the project was not as planted. The skill
said to read results from a file and not where to put it; it now says to keep every file
an agent writes under `runs/`. Rerun with that: 9 of 9, 3 tool calls each, 19 s a run, for
$0.75. Neither run's rows are in `results.csv`.

## Phase 2: the skirmish, objectives in tension

Run on 2026-10-31 at a929029 (Baker 0.7.0, Godot 4.5.1, Linux), with headless Claude Code
2.1.316, model `claude-sonnet-5-5`, the same protocol: three runs a task, three at a time.
Results in `results-phase2.csv`.

**Result: 21 of 21 runs succeeded,** for $2.50 in all: the three platformer tasks again,
and four on the skirmish, where two objectives bound each value from both sides (a tank by
turn 5 but not before turn 4; infantry needs 3 or 4 hits to destroy a tank), so the
obvious fix, made large, fails its twin.

| task | planted | success | tool calls (mean) | wall (mean) | what the agents wrote |
|---|---|---|---|---|---|
| `slow-army` | HQ income 12: the tank on turn 8 | 3/3 | 7.3 | 32 s | HQ income 20, three times |
| `fragile-tank` | tank armor 0: two hits | 3/3 | 3.0 | 21 s | tank armor 2, three times |
| `rule-armor` | nothing: armor should stop half, in every match | 3/3 | 3.3 | 33 s | the damage rule defined again in `#autorun`, and tank hp 12, 13, 12 |
| `two-complaints` | both plants | 3/3 | 5.0 | 29 s | income 20 or 22, and armor 2 or hp 16 |
| `reach-short`, `skips-climb`, `floaty-fall` | as before | 9/9 | 3.3 to 4.7 | 19 to 21 s | as before |

**What the agents did.** Every run read the objectives before it changed anything, and
aimed inside both bounds rather than at one: the reports name the window ("turn 4 or 5",
"3 or 4 hits") and work the arithmetic out from the data (the opening costs 100 from a
start of 10, so the tank comes on turn ceil(90 / income)). In `rule-armor` every run copied
the experiment's rule whole into `agent.dta`'s own `#autorun`, with its `@doc`, saw from
`sim` that the duel then fell to two hits, and retuned the tank's hit points, the change
the task hid; one noted that armor 1 now stops nothing. No run edited a data file, an
objective, a scenario or a card.

**Where they struggled: nowhere these tasks reach.** The slowest, `slow-army`'s third run,
took 11 tool calls, trying income values by `sim` before it worked the formula out. Every
fix the agents chose was the shipped value or next to it, found by arithmetic: the windows
are wide enough, and the model of the economy small enough, to be solved on paper. These
tasks show that the skill carries over to rules, turns and tables, and that bounded
objectives stop an overshoot; they do not yet find a limit.

**What a harder evaluation needs** (not built): an economy whose outcome the agent cannot
work out by hand (seeds that change the AI's choices, several resources), objectives that
cannot all hold at once, so the agent must say which to give up and why, and tasks on
content at scale (hundreds of records, as in the assessed games) where finding the right
values is the work.

## Protocol

- **Tasks** (`harness.py`): each a person's complaint over a fresh copy of the repository,
  with a fault planted in `sample/data/tuning.dta` (the first three, below) or
  `sample/data/skirmish.dta` (phase 2's, above):

  | task | planted | complaint | the fix must |
  |---|---|---|---|
  | `reach-short` | nothing: the shipped jump of 96 px | "the jump comes up just short" of the first platform | clear 110 px and stay under 160 |
  | `skips-climb` | a jump of 190 px | "one jump carries me straight onto the second" platform | the same window, from above |
  | `floaty-fall` | a jump of 120 px, which passes | "the fall feels floaty" | raise the falling gravity by more than 5%, and still pass |

- **The agent:** the skill (`.claude/skills/sample-agent/SKILL.md`) in its system prompt
  and in the copy, the complaint, at most 60 turns, the copy as its working directory,
  `godot` on its PATH, and Bash, Read, Write, Edit, Glob and Grep.
- **Runs:** three per task, three at a time.
- **Success** is judged afterwards by the game, not the agent: every file but
  `data/agent.dta` (and run output) is byte for byte as planted; `--baker check` exits 0;
  `--baker sim --baker-seeds 1-3` exits 0; and the task's own condition holds.

## Results

`results.csv` has every run: success and its parts, the agent's `agent.dta`, turns, tool
calls, tokens, cost, wall time.

| task | success | tool calls (mean) | wall (mean) | what the agents wrote |
|---|---|---|---|---|
| `reach-short` | 3/3 | 3.3 | 34 s | height 120, 120, 124 |
| `skips-climb` | 3/3 | 3.0 | 30 s | height 135, three times |
| `floaty-fall` | 3/3 | 3.7 | 32 s | fall_multiplier 2.3, 2.2, 2.4 |

**Every agent followed the skill's loop:** it loaded the skill, read the data and the
objectives, ran the baseline (`sim --seeds 1-3 --out runs/before`), wrote `agent.dta` with a
comment giving the reason, then ran `check`, `sim --out runs/after`, `compare` and `diff`,
chained in one shell call. Each report said what it changed and why, which objectives moved,
and that it had not played the game. No run touched `tuning.dta`, a scenario or the game.

One stumble: a `cd sample` in one shell call carried into the next, so `--path sample`
pointed nowhere; the agent saw it and ran again from the root.

## Limits

- **The tasks are easy.** Each is one value against a window the objectives state, and the
  sample has ten tunables and two scenarios. This shows the skill and the commands work
  for an agent end to end; it does not measure how agents do on a game of real size, as
  baker-mcp's evaluation does on the stacks.
- One client, one model, three runs a task, Linux only.
- What objectives cannot see (how the fall feels) is unjudged; the agents said so.

## Reproducing

Build the extension (README), then:

```
python3 eval/harness.py --godot PATH/TO/godot --runs 3 --jobs 3 --model claude-sonnet-5-5
```
