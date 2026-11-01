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

## Phase 2, the harder round

Run on 2026-10-31 at 9ea064d, same model and client, the skirmish's six tasks, three runs
each, judged over seeds 1-5. Results in `results-phase2b.csv`, with each run's report.

What changed from the first round: the economy charges each unit's upkeep every turn, and
after its opening each side buys one thing or saves, drawn by its seed from `ai.weights`, so
seeds play differently (blue's army on turn 9 to 11) and an outcome is not worked out on
paper; a fourth objective bounds the army from below (not before turn 6). Two tasks are
new: `card-scale`, over 40 card resources, eight of which cost 3 or more and deal less than
4 a point; and `tank-on-3`, a request that `economy.tank_not_before_turn_4` forbids, judged
by every objective still passing and the report naming that objective.

**Result: 18 of 18 runs succeeded,** for $2.27.

| task | success | tool calls (mean) | wall (mean) | what the agents did |
|---|---|---|---|---|
| `slow-army` | 3/3 | 6.0 | 30 s | HQ income 22, three times: above the shipped 20, judged over five seeds |
| `fragile-tank` | 3/3 | 3.3 | 21 s | tank armor 2, three times |
| `rule-armor` | 3/3 | 3.3 | 31 s | the rule redefined, and the tank's armor doubled or its hit points raised |
| `two-complaints` | 3/3 | 7.0 | 36 s | armor 2, and income 20 or 24 |
| `card-scale` | 3/3 | 6.3 | 31 s | all eight weak cards found by name, each set to exactly 4 a point, none other touched |
| `tank-on-3` | 3/3 | 3.3 | 21 s | nothing changed; each report named the objective and why |

**What the agents did.** With the economy no longer solvable on paper, the `slow-army` and
`two-complaints` agents leaned on `sim` and `compare` over the five seeds, and two chose
incomes the first round never did (22, 24), inside the window on every seed. In
`card-scale` every run read the card files or `reference`, found the same eight cards,
excluded fireball because the data prices it at 2, and wrote one override a card. In
`tank-on-3` every run declined: one tried a shorter opening in its layer, saw the tank come
on turn 2 and break the bound, removed the trial, and reported both the attempt and the
objective; all three said a person would have to change the objective, separately.

**Where they struggled: still nowhere these tasks reach.** The skill's loop (read the
objectives, baseline, change, check, sim over seeds, compare) carries the tasks, including
the one that must end in a refusal. What this sample cannot test: a game whose
consequences of a change are not visible to the scenarios at all (feel, readability), and
data at the scale of a shipped game, where an agent's reading of the files, not the
commands, would be the cost.

## Phase 3: the skirmish's C# port

Run on 2026-10-31 at 0258945 (Baker 0.7.0, Godot 4.7.2's .NET build, Linux), same model
and client, the skirmish's six tasks judged with the C# port (`harness.py --skirmish cs`:
`--skirmish cs` first after `--` in every command, the agent told so). Results in
`results-phase3-cs.csv`.

**Result: 18 of 18 runs succeeded,** for $2.54, 3.3 to 7.3 tool calls a task, as on the
GDScript skirmish. Every transcript shows the agent putting `--skirmish cs` in each command,
and the fixes are the ones the GDScript round chose (HQ income 22 to 24, tank armor 2 or
more hit points, the rule redefined whole, the eight weak cards by name, a refusal that
names `economy.tank_not_before_turn_4`). The port changes nothing the agent sees: the same
commands, the same results, so the skill carries over to a C# game unchanged.

## Phase 3, the third round: tables, lists and references

Run on 2026-11-01 at the commit this file is in (Baker 0.7.0, Godot 4.7.2, Linux), same
model and client, three new skirmish tasks, three runs each on the C# port
(`results-phase3b-cs.csv`) and three on the GDScript skirmish (`results-phase3b-gd.csv`),
judged over seeds 1-5 and, for two of them, by a scenario of the judge's own, written into
the copy after the planted files were compared and run over the data the agent left.

What changed: the tasks change a table's and a list's shape, not only a value in them.
The match reads its unit table and its opening whole (the C# port from a published
snapshot), so a record a layer adds, or a list it reorders, is what the match plays.
`new-unit` asks for a fifth unit the AI buys "now and then" with the economy's pacing kept;
`open-artillery` asks for an artillery second in the opening with the tank and the army
landing when they do now; `free-upgrades` asks for each 0-cost card to upgrade into a
1-cost card, nothing else touched. Making them possible changed the sample: the opening
was a list of `(build ...)` records, and a layer restating one merges into the first by its
tag, silently (`diff` reported nothing changed); it is a plain list now, which a layer
restates or extends with `#append`, and the skill teaches tables and lists.

**Result: 12 of 18 runs succeeded,** for $1.97 on the C# port and $1.66 on the GDScript
skirmish: the first task this evaluation has that agents fail.

| task | C# | GDScript | tool calls (mean) | what the agents wrote |
|---|---|---|---|---|
| `new-unit` | 0/3 | 0/3 | 11.3, 10.7 | a mech of 14 or 16 hit points, move 4, priced 52 to 55, at weight 1 |
| `open-artillery` | 3/3 | 3/3 | 10, 8.3 | `(opening infantry artillery tank)`, and the start's points raised (40 to 53) or the artillery priced down (25 to 40), the tank back on turn 5 |
| `free-upgrades` | 3/3 | 3/3 | 7, 5.3 | six overrides, each free card into a 1-cost card, often by theme (gust into gale) |

**What the agents did.** Every run read the unit shape or the reference before adding the
mech, and every mech passed `check` with every required field; every run gave it a weight,
having read that the AI never buys a kind without one. In `open-artillery` every run saw the
tank slip to turn 7 at the artillery's price of 60, then found a lever that put it back
(points at the start, or the artillery's price) and held the army's turns over five seeds.
In `free-upgrades` every run named the 1-cost cards from the card files or the csv and set
only `upgrade`, and `check` held each name to the cards that exist. No run touched a data
file, a card, a scenario or an objective; the C# runs put `--skirmish cs` in every command.

**Where they struggled: the purchase against the pacing.** In `new-unit` every run found
the tension the task holds: at weight 2 or more a mech of the tank's price breaks
`economy.army_by_turn_11` on seed 3, so all six chose weight 1, priced the mech at 52 to 55
("near a tank"), and reported the pacing unchanged, which it was. Five reported the AI
buying it, from probe matches of their own run to 30 turns; the judge plays the matches the
scenarios play, which end when blue's army is ready (turn 9 to 11), and in 30 of those a
mech at 55 and weight 1 is bought in none. A fix exists inside every objective: at price
45 and weight 3 the AI buys one in 7 of 30 such matches, with the economy's five seeds
passing; no run tried a price below the tank's. The complaint's "priced near a tank" held
them there, and none judged "now and then" by the matches the objectives judge.

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

The skirmish's tasks need Godot's .NET build as the path (the sample is a C# project);
`--skirmish cs` runs them on the C# port, and `--tasks new-unit,open-artillery,free-upgrades`
the third round alone.
