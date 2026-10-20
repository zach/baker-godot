# 0010: check holds the data to the tweaks the game declares

## The need

`check` compares the data with every tweak registered when it runs, and reports, located
by file and line:

- **an error** for a value that does not convert to its tweak's type (a string where a
  float tweak reads), which today the tweak only warns about as it binds, and the check
  passes;
- **an error** for a value outside its tweak's range (`TweakMeta::range`, or the data's own
  `(range ...)`), or not among its `one_of`, as a shape's range and list already are;
- **a warning** for a tweak whose path the data lacks, so the tweak keeps its default, and
  for a value beside the tweaks of its group that no tweak reads, which is how a misspelled
  key looks (`(jump (hieght 120))` next to the tweaks `jump.height` and `jump.buffer`).

The same findings in `baker.check` JSON, in lists of their own or in the shape lists, so a
gate reads them as it reads the rest.

## Why

A tweak is how a game declares a tunable: its path, its type, and often its range. A shape
says the same of data, and `check` holds data to shapes; it does not hold data to tweaks,
so a game must declare each tunable twice, or its check passes over a proposal that cannot
work. That matters most where `check` is the gate: an agent's edit, a pre-commit hook, CI.
In this integration, an agent's layer with `(height "tall")`, `(height 1000)` against a
declared range of 16 to 320, or `(hieght 120)` each passes `check` with exit 0, and the
game then plays with the scene's default, or a value its designer ruled out, with nothing
on the record. Any game with tweaks in code (a range given in `TweakMeta`) has the same
gap, and the tweak already holds everything the check needs.

## Constraints

- Only tweaks registered when the check runs: a tool run inside the game's binary, with
  its scene or its systems loaded, as the tool commands are.
- A tweak whose path is missing stays a warning, never a failure: a default in code is a
  choice.
- A key no tweak reads is a warning only where a tweak reads a sibling of it (the same
  group), so data that tweaks never read (structured data, scenarios) stays quiet.
- Nothing new on the tweak's hot path: the work is in `check`.

## Today

The integration gives each bound property's tweak its export range and step as metadata,
so `reference --json` shows them and the agent skill tells agents to keep within them.
Nothing enforces them: the three edits above pass `check`.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
