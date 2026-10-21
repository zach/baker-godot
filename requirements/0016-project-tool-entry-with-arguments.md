# 0016: The project's tool entry with arguments

## The need

A project file's `(tool ...)` entry can name the command that answers the kit's commands
with arguments of its own, not only a binary, for example the engine's binary, the
project to open, and a separator before the game's own arguments. A tool reading the
project then runs `<tool> <arguments> check --json` without knowing how the game starts.

## Why

`(tool "build/bin/my_game")` lets a tool find the binary that answers `check`, `sim` and
the rest, so an editor's test view, CI or an agent can run a project's scenarios without
knowing anything about the game. A game that runs inside an engine is started as the
engine's binary with arguments: which project to open, that it runs headless, sometimes a
fixed frame rate, and a marker after which the game's own arguments begin. Such a game
cannot be described by a binary alone, so a tool that reads the project cannot run it, and
each such game documents its command line by hand instead. Scripted games, games run from
an interpreter, and games built per platform into different paths are in the same place.

## Constraints

- A path to a binary alone keeps working as today.
- Arguments are a list, not one string, so nothing depends on a shell's quoting.
- A path among them may be relative to the project file, as the binary's is.

## Today

The integration's project file has no `(tool ...)` entry. Its README and its agent skill
say how to start the game for the commands, and every tool that runs them repeats that
command line.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
