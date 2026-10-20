# 0007: The kit's tool commands read and write through the host

## The need

A host can run the kit's tool commands (check, reference, diff, promote, eval) over the
same files its game reads, and send each command's output where it chooses:

- the commands open the stack through the host's reader, as the live loop can, so a stack
  whose layers and includes are named under a virtual root reads the same way in a
  command as in the game;
- a command's output (the JSON, the diff, the reference) goes to a sink or a file the host
  names, not only to the process's standard output.

## Why

Engines run a game's own binary as its tool: an editor, a CI checker or an agent with a
shell calls the game with a command and reads the answer. Two things stand in the way
inside an engine:

- The engine names files through its own roots (a project root, a per-user root) and its
  own file access; a packaged or remapped file is reachable only through it. The live loop
  takes the host's reader; the commands open their stack from the disk alone.
- The engine owns the process's standard output: it prints its banner and its own messages
  there before and while the game runs, so a tool that parses stdout as JSON gets lines
  that are not the command's. Writing the result to a named file, or to a sink the host
  routes, keeps it clean. A failure to write that file is the command's error (exit 2), as
  a patch it cannot write is today.

## Constraints

- The reader is the same callback the live loop and the Config take; nothing new for a
  host that already has one.
- Without a reader or an output target, the commands behave as today: the disk, and
  stdout.
- The exit statuses stay as they are (0, 1, 2).

## Today

The integration keeps its own small implementation of the commands, run on the game's
open Config through the host's reader, and adds an `--out FILE` flag of its own. It does
not use the kit's commands.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
