# 0006: A host can tell that a live edit happened

**Shipped** in Baker 0.2.0 (`Config::epoch()` moves on every live edit, and the host loop's
`changed` callback follows it); this integration uses it and has dropped its own console.

## The need

A host can tell, with one number to compare each frame or with a callback, that the
Config's live state changed: a set() (the fast path into a path already in scratch
included), a clear_scratch(), a write-back, a promote, a restore. Today the epoch moves on
every change but a fast-path set(), and the reload-complete callback fires for changes to
the stack, not for live edits; a live loop's `changed` callback, built on it, misses them.

## Why

Hosts show and cache live state:

- an editor panel lists the running game's unsaved values, and must refresh after each
  edit, including the second edit of the same value;
- a scripting layer caches values read through the Config until the data changes;
- a tool reports what a person tuned since it last looked.

A host that makes every edit itself can note it at the call. But edits arrive from places
the host does not see: the console runs `{config.set path value}` for a tool or an agent,
and a bound tweak's set() goes through the Config. With the host kit's console, the host
answers no lines itself, so it cannot even note "a line was answered". Bound tweaks follow
the edit already; everything else the host shows goes stale.

## Constraints

- Reading the number, or having nothing to call, costs nothing on a quiet frame and
  allocates nothing; the fast path of set() stays as fast as it is.
- It moves for live edits whether the journal records them or its history is off.
- In shipping builds, where live edits are refused, it may never move.

## Today

The integration notes a change itself: in each call it makes on the Config, and after each
line its own console answers. With the host kit's console it would miss edits made there.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
