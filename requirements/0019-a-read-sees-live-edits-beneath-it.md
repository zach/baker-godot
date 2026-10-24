# 0019: A read of a path sees the live edits beneath it

**Shipped** in Baker 0.6.0 (`Config::apply_edits`, and a read of a record or table above
a live edit sees it); the integration's own overlay of the edits on a table read is gone.

## The need

A read through a Config of a path that holds a record or a table (`get` of `units.tank`,
of `units`) includes the live edits made in scratch at paths beneath it, as a read of the
edited path itself already does. Today, after `set("units.tank.price", 45)`, a read of
`units.tank.price` gives 45, but a read of `units.tank` gives the record with its old
price, until the next full merge (a reload, a layer toggled); `root()` is as stale.

## Why

Games read data at the level they use it: a field where a tweak is bound, a record where
code builds a thing from it (a unit from its type, a building from its kind), a table where
code iterates it (a production menu, an AI's valuation, a loot roll). A person tuning live
edits one field, and expects every reader to see it on the next frame, as tweaks do. A
reader of the record or the table sees the old value instead, so the game runs on two
versions of the same data at once: a unit's price edited live shows in the field's panel
and not in the menu that lists the record. The mismatch ends at a reload and returns at the
next edit. Every host that reads structured data whole has it; the more data-driven the
game, the more of it is read that way.

## Constraints

- **The fast path stays fast.** Setting a path already in scratch updates its tweaks with
  no merge and no allocation, as today; the cost of seeing the edit moves to the read of a
  path above it, and only while an edit beneath it is pending.
- **A read above an edit is consistent:** every edited field beneath the path, and nothing
  else changed.
- **`root()`,** or another way to have the merged tree with scratch applied, says what it
  includes, so a tool that walks the tree whole (a panel, a check) can choose.
- **Shipping builds,** which refuse live edits, are unaffected.

## Today

The integration's table reads lay the unsaved edits beneath the path over the record they
read: it lists the Config's unsaved paths, reads each that starts with the path, and sets it
into the Dictionary it returns. That is a read and a conversion per pending edit, done on
every table read while edits are pending, and only where the integration reads; a reader
that goes to the Config directly still sees the stale record.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
