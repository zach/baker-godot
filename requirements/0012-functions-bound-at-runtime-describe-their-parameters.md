# 0012: A function bound at runtime describes its parameters

## The need

A host can register a script function that has a body and a described signature at once:
the body a callable the host builds at runtime, the signature an `OverloadDescription` (its
parameters' types and kinds, its result) as `Context::declare` takes today for a function
without a body. `check` then counts a call's arguments and checks its literal arguments'
types against the description, and `reference` writes the parameters, exactly as for a
native function whose signature is deduced from its C++ type.

## Why

A host with a scripting language of its own exposes that language's functions to Baker's
script one at a time, at runtime, with no C++ signature to deduce: the only way today is a
function taking `Args`, which reads as "values..." and accepts any number of arguments of
any type. `check` then passes every call to it. In this integration, a scenario calling
`{player.move}` with no argument, and `{player.hold_jump 1 2 3}` with three to a function
that takes none, both pass `check`; the mistake shows only when the run plays, if at all.
The scripting language usually knows each function's parameters (their count, often their
types), so the host has the description; Baker has no way to take it with a body. Any
engine or game with an embedded scripting language that binds its functions into Baker's
script has the same gap, and it is where agents and designers write the calls.

## Constraints

- The description is checked as `declare` checks it today: a type the Context cannot check
  is an error, and nothing is registered.
- At a call, the arguments arrive as they would for the described parameters (evaluated,
  or raw for a raw one), so the body need not re-check what `check` already holds.
- A host may still register a function as `Args` alone, as today.
- Nothing new on the call path of native functions.

## Today

The integration binds each script function as one taking `Args`. Its reference lists them
as taking any values, and `check` cannot catch a wrong argument count or type in a
scenario, an objective or an agent's data.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
