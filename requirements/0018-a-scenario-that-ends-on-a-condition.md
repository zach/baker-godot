# 0018: A scenario that ends on a condition, in the host's own steps

**Shipped** in Baker 0.6.0 (`(until {command})` naming how a run ended, `(steps N)`,
`(at step N ...)`); phase 2's step 6 uses it for turns.

## The need

A scenario can say when its run ends by the game's own state, not only by its length:
`(until {board.side_lost})`, which the kit evaluates after each step, ending the run when
it gives a nonzero value. The scenario's length stays, as a ceiling: a run that reaches it
first is judged as today, or marked as having hit the ceiling, so an objective can tell
"won by turn 12" from "still going at turn 40".

With it, a host whose game advances in discrete steps (a turn, a tick of a board game, an
event) can name what one step is, so a scenario reads in the game's terms, such as
`(turns 40)` or `(at turn 5 ...)`, rather than in seconds at a step rate chosen to make one
second one turn.

## Why

A turn-based or event-stepped game has no fixed number of steps a scenario lasts: a
match ends when a side loses, an economy test when a goal is met, a puzzle when it is
solved. Today a scenario gives `(seconds N)` and runs exactly N times the step rate,
so such a game must guess a length long enough for every tuning, run past the end and
judge what is left. That costs time on every run, and the result cannot say how long the
outcome took, which is often what a balance question asks ("how many turns does the
attacker need"). Such a game also has no seconds: its scenarios are written in turns, and
mapping a turn to a second by setting the step rate to one is a convention every such host
must document and every author must know.

## Constraints

- **Determinism holds:** the condition is evaluated after each step the host reports, and
  a run that ends on it ends at the same step on every machine for the same data, scenario
  and seed.
- **The ceiling is required:** a run always ends, so a condition that never holds cannot
  hang a simulation.
- **The results say how a run ended:** at its condition or at its ceiling, and after how
  many steps, so an objective or `compare` can use it; `$steps` (or the like) is available
  to an objective's measure.
- **A step's name is presentation:** naming a step "turn" changes how scenarios are
  written and results are read, not how the kit counts; the step rate stays for hosts that
  step in time.
- **A quiet step allocates nothing,** as today; evaluating the condition is one command a
  step.

## Today

The integration's sample steps in physics ticks and its scenarios have fixed lengths in
seconds, which suits a platformer. Its plan for a turn-based sample reports one step per
turn under a step rate of one, writes lengths in seconds that mean turns, and runs each
scenario to a length long enough for its slowest tuning.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
