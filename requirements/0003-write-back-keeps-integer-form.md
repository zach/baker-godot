# 0003: Write-back keeps a number's integer form when the new value is whole

**Shipped** in baker-dev 4a84a63 (D183); this integration uses it and has dropped the
workaround below.

## The need

When write-back replaces a number that the file wrote as an integer, and the new value is a
float with no fractional part, the file keeps an integer: `(height 96)` tuned to 120.0
becomes `(height 120)`, not `(height 120.0)`. A fractional value is written as a float as
today: `(height 96)` tuned to 120.5 becomes `(height 120.5)`.

## Why

Engines and editors hold most tunable numbers as floats: an inspector's number field, a
script's float property, a slider. A designer who tunes a value the data wrote as `96`
expects the diff to read `96` → `120`. Today it reads `96` → `120.0`, which changes how the
value is written for no reason the designer gave, and makes every whole-number tweak from
such a host look like a type change in review. Every host whose live edits come from float
fields meets this, so it belongs where the token is replaced, which already knows what the
old token was.

## Constraints

- Applies to write-back of a live edit (`write_back`, `write_back_to`, `promote` from
  scratch) over an existing integer token only. A new entry, or a float token, is written
  as today.
- A whole value outside the integer range, or a NaN or infinity, is written as a float.
- What `get` returns afterwards is unchanged for a float tweak or lookup: the value reads
  the same as a float either way.

## Today

The integration's Keep converts a whole float to an integer before the live edit when the
value it replaces was an integer, so Baker writes `(height 120)`. It knows the old value
from the edit journal, where each entry records what the path held before.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
