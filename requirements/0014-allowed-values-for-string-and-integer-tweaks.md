# 0014: Allowed values for string and integer tweaks

## The need

A tweak whose value is a string or an integer can carry the values it may take, as a
symbol tweak carries its `one_of` today: a list of strings, or of integers with a name for
each, given in code (`TweakMeta`) or in the data after the value. `check` holds the data to
the list, as it holds a symbol tweak to its `one_of`, and the tweaks JSON writes it, so a
panel offers a choice and an agent knows what it may propose.

## Why

Editors and engines offer a choice among names for a property in two forms: a string from
a list, or an integer whose values have names (an enumeration). A host that binds such a
property to a tweak knows the list, but a string or integer tweak cannot carry it, and only
a symbol tweak's list is checked. In this integration, a property declared as one of
"light", "heavy" or "kick" would bind as a string tweak: nothing would tell a tool its
choices, and a misspelled `(attack "hevy")` would pass `check`. Binding it as a symbol
instead would change what the data must contain from what the engine's own files hold, so
the integration leaves such lists out. Enumerations in integer
form (a difficulty, a team, a state) are as common, and the same is true of them.

## Constraints

- The data's list after the value wins over the code's, as for `one_of` and `range`.
- For integers, each allowed value may have a name, which the tweaks JSON writes beside
  the value, so a panel shows the name and the data holds the number.
- A value outside the list is a `check` error, located, as for a symbol tweak; the tweak
  itself still takes the value as it takes any, as today.

## Today

The integration carries only a property's numeric range to its tweak. A property with a
list of choices binds as a plain string or integer tweak, with no list for tools, agents or
`check`.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
