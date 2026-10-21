# 0011: A scenario run the host advances from its own loop

## The need

A host can run the kit's scenarios by advancing each run itself, one fixed step per frame
of its own main loop, rather than handing the kit a step callback that the kit calls in a
loop inside one command call. The kit keeps everything else it does for `sim` today: it
reads the scenarios and objectives, orders the runs by scenario and seed, runs each start
command and each `(at ...)` command when its time comes, measures and judges the
objectives, and writes `baker.sim/1` and the results `compare` reads, with the same exit
status.

A shape that would serve:

- the host starts a sim from its command line, as `run_command` would, and gets back a run
  in progress;
- before each run, the kit asks the host to reset with the run's seed, and the host says
  when it is ready, which may be some frames later (a fresh level loaded, say);
- once a frame, the host tells the run that one step of `dt` seconds has passed in its
  game; the kit counts the step, runs what is due, and at the scenario's end judges the run
  and moves to the next;
- when the last run ends, the host gets the exit status and the output, as from
  `run_command`.

## Why

Engines that own their main loop advance their physics, and much else, only inside that
loop: one frame at a time, each frame returning to the engine. Such an engine offers no
call to run one step of its game on demand and come back. A game in it cannot implement
`CommandHooks::step` faithfully. Stepping its systems from inside one call skips the
engine's physics, and the results are wrong and not repeatable. In this integration's
sample, a jump the engine's own frames carry to an apex of 96.06, as its data says, gave
144.58 and 147.65 in two runs stepped that way. This holds for the commercial and open
engines alike, and for any game whose simulation is driven by a frame loop it does not
call.

The kit's scenarios are worth having in such a game: data a tool or an agent can write
without the game's scripting language, checked with the data, located results, medians over
seeds, and one `compare` for every Baker game. Without a run the host can advance, each
such game keeps its own runner and its own result format instead.

## Constraints

- **Determinism holds as it does now.** Steps are counted, never timed: a run depends only
  on the data, the scenario, the seed and the steps the host reports. The host reports one
  step per fixed step of its game, at the rate the command line names (`--step-hz`), and
  can learn that rate from the run so it sets its own fixed rate to match.
- **The synchronous step stays,** for hosts that can step on demand. Both drive the same
  runs and give the same output.
- **A quiet frame allocates nothing:** a step with nothing due is a count and a compare.
- **Reset may take frames.** Between runs the host may need to rebuild its world, which in
  such an engine happens across frames; the run waits for the host to say it is ready, and
  counts no steps meanwhile.
- **Commands run between frames,** on the thread that owns the Context, as the console's
  do.
- **The host can stop a run** (its window closed, a timeout) and still get the output so
  far, with the run marked as not finished.

## Today

The integration keeps its own runner in the game's scripting language. Its scenarios are
scripts that drive the player frame by frame, wait on events in the game, and report their
objectives. It writes its own JSON for results and has its own `compare`. It cannot use the
kit's scenarios, their checking, `baker.sim/1` or the kit's `compare`.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
