# 0021: Tweaks whose value the data may leave out

## The need

A host can mark a tweak as one the data is not expected to supply: its default, from the
host's own content, is the value, and a layer supplies one only to change it. `check` does
not warn that the data lacks such a tweak's path. Everything else stays as for any tweak:
`check` still fails a value of the wrong type, out of the tweak's range or not among its
allowed values when a layer does supply one; it still warns of a key no tweak reads beside
the tweak's siblings (a misspelling); and the reference still lists the tweak, with its
default and the mark, so a tool or an agent knows the value is tunable and where it comes
from.

The mark belongs on the tweak (one flag in its metadata, say), so a host can set it for
some tweaks and not others: a record's fields that a content file supplies, but not a
gameplay constant the data must carry.

## Why

A game whose content lives in its engine's own asset files (cards, items, enemies, a file
each, with their values in the files) binds those values as tweaks so that layers can
retune them by name: an experiment, a balance pass, a mod, an agent's proposal. The data
then holds only what a layer changes, which is the point: a few overrides over a library of
records. Today `check` warns once for every bound value the data does not mention, which is
nearly all of them. This integration's sample has three cards and gets a dozen warnings; a
shipped deckbuilder with about 400 cards, items and events, each with several tuned fields,
would get thousands, and a real warning (a tweak a constant needs and the data dropped)
disappears among them. A check that is the gate for an agent's or a pre-commit hook's change
must be quiet when the data is as intended.

The same holds for any host that registers tweaks from content it owns rather than from
code: an editor's asset database, a mod's own defaults, a level's placed objects.

## Constraints

- Nothing on a tweak's read path, and nothing kept in shipping builds beyond what the tweak
  keeps today: the mark is for check and the tools.
- Unmarked tweaks keep today's warning: a default in code that the data should supply is
  still worth a warning.
- The reference's JSON names the mark, so a tool can tell "the data may leave this out"
  from "the data is missing this".
- A host can mark tweaks it creates at runtime, in bulk (several thousand at load), without
  a cost per tweak beyond setting the flag.

## Today

The integration binds every record's tuning fields as tweaks and the data overrides a few.
`check` passes, with a warning for each field of each record the data does not mention. The
integration does not filter them: it cannot tell them from a warning that matters, and
dropping all missing-path warnings would hide those too.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
