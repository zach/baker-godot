# 0022: A layer over a list written as same-tagged records

## The need

When a layer merges into an array whose children repeat a tag, `check` warns at the layer's
row, and `diff` and `explain` report what the merge did. Baker's language says that
repeated tags do not merge one by one, and that a list a layer may retune is written as
plain values; what is missing is the tools saying so when data breaks the rule. Today a
layer's restatement of such a list merges into the first child by its tag, the rest of the
layer's children fold into the same child, and nothing reports it: `check` passes, `diff`
shows the one key with its old and new value (the same value, when the layer restates the
first child), and the game reads a list the author did not write.

Concretely:

- A warning from `check`, located at the layer's row and naming the path and the repeated
  tag: the array below holds several children with that tag, so the layer's children merge
  into the first of them; write the list as plain values, or extend it with a verb. The
  same warning would serve a bottom layer that declares such an array under a path a
  `#shape` or a tweak reads as a list.
- `diff` and `layer_effect` reporting the merge as what it is: the key, the value it had,
  the value it has, and that the children the layer wrote beyond the first were folded.
- Nothing changes in merging itself: data in the field relies on it.

## Why

A list of same-tagged records reads naturally and is what a designer writes first:
`(opening (build infantry) (build infantry) (build tank))` for a build order, `(waves (wave
...) (wave ...))` for a level's waves, `(steps (step ...) ...)` for a tutorial. The game
reads it whole, as a list, and plays it as written; everything works until a layer above
restates it. This integration's sample carried its build order in that form through two
rounds of an agent evaluation, where every task retuned values and none a list; the first
task that asked an agent to reorder the build order found that the agent's layer, correct
to a reader, changed the opening to one entry, and `diff` reported the one key as unchanged.
The agent's loop (change, check, sim, compare) could not see it: check was quiet, the sim's
objectives failed for reasons the agent could not trace to the layer, and `diff` said
nothing had changed.

Any host whose layers come from people and agents who did not write the bottom data (a
balance pass over another team's tables, a mod over a shipped game, an agent's proposal) will
meet this: the bottom data decides whether a list can be layered, and the person writing
the layer learns it only from the game's behaviour.

## Constraints

- A warning, not an error: data in the field holds such arrays, and a layer that sets one
  field of the first child may be intended.
- Located at the layer's row, with the path and the tag, so a tool or an agent can show
  the line and say what to write instead.
- No cost on a read or in shipping builds: the check is for `check` and the tools, as the
  rest of `check` is.
- Deterministic, and quiet for data that follows the language's rule (a plain list, a
  keyed table whose keys do not repeat, a list of records nobody layers over).

## Today

The sample's build order is a plain list now, which a layer restates whole or extends with
`#append` and `#drop`, and the integration's README and its agent skill say to write a list
a layer may retune that way. Data a user of the integration writes in the other form gets
no warning from the integration either: it has no view of the merge beyond Baker's.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
