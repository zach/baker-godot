# 0009: Layers the project file marks for development only

## The need

A project file can mark a layer as one for development only: a person's own tuning, an
agent's proposal waiting for review, a scratch experiment. A shipping build leaves such a
layer out even when its file is present, and a host's packaging step can ask the project
which files it must not ship, rather than keeping its own list in step with the project
file.

## Why

Layers for development sit beside the shipped data, and a packaging step that takes every
data file takes them too. An agent's proposal left on the machine that builds a release
then ships, unreviewed, above the default layer, and wins. This integration's own export
did that until it listed the file to exclude by hand. The same hazard holds for any host
that packages a data directory whole: a game's build script, an engine's export step, a
content pipeline. A mark in the project file is one place that says it, read the same way
by the game, its tools and its packaging. A layer that stays optional for development
(`optional` today) is not the same thing: an optional layer whose file is present loads,
in every build.

## Constraints

- Under shipping builds, a marked layer is never pushed, whatever its file says; in
  development builds it loads as today.
- A host can list the marked layers' files, as the project names them, before it packages
  anything, without opening a Config.
- A layer can be both marked and optional, as `local` and `agent` are.
- Tools that run without the marked layers (a check, a simulation) can leave them all out
  in one option, rather than naming each.

## Today

The integration's export presets exclude the agent layer's file by name, and a test plants
one before exporting and checks the exported game does not read it. A new
development-only layer needs a new entry in every preset.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
