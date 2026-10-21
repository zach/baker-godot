# 0015: The live loop's console on a free port

**Shipped** in Baker 0.5.0 (`LiveOptions::console_any_port`); `--console-port 0` uses it, and
`tests/agent.sh` reads the port the game prints.

## The need

A host can ask the live loop for its console on any free port, as `Console::listen(0)`
already allows, and learn the port it got from `console_port()`. Today `LiveOptions`'s
`console_port` of 0 means no console, so the only way to get one is to name a port before
the game starts.

## Why

A tool or an agent that starts the game to talk to it over the console needs a port no
other process holds. Several instances of one game on one machine (an editor running two,
agents working in parallel, a CI machine running jobs side by side) collide on any port
fixed in advance, and a tool that picks one first (binds a socket, closes it, passes the
number) races with every other process for it. The console already knows how to take a
free port; the live loop has no way to ask for it. With it, the game prints or writes the
port it got, and the tool reads that.

## Constraints

- A way to say "any free port" distinct from "no console", such as an option of its own,
  so existing hosts keep their meaning of 0.
- Loopback only, as today; nothing about the console's address changes.
- Under shipping builds, no console, as today.

## Today

The integration's command line takes a console port, and its tests pick a free one first
and pass it, which can race. Asking for port 0 opens no console; before the integration
moved onto the live loop, it opened one on a free port.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
