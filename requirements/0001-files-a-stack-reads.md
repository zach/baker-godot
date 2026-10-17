# 0001: The files a Config's stack was read from, for a host's own watcher

## The need

A host can ask a Config for every file its layers were read from: each layer's own file
and every file reached from it by `#include`, `#merge` and their kin, with the layer each
belongs to. The list changes when a reload, push or toggle reads different files, and the
host can tell that it changed (an epoch, or a callback) without comparing lists.

## Why

Engines and editors already watch files: a file-system notification, an editor's own
change tracking, or a cheap check of each file's write time and size once a frame. A host
that knows which files matter calls `reload()` only when one of them changed, and gives
`ReloadRequest::origin_ns` the time the edit was saved, so edit-to-see is measured from the
save. Today a host knows only the files it named itself (the project file's layers). An
included file's edit is found only by calling `reload()` on a timer, which re-reads and
hashes every file each time, and whose origin is unknown.

## Constraints

- Reading the list allocates nothing on the frame path: a span over storage the Config
  owns, valid until the next change, is enough.
- Paths are as the Config's reader is given them, so the host maps them to its own file
  system exactly as it maps reads.
- Shipping builds may return an empty list.

## Today

The integration stats the project file's layer files once a frame, and calls `reload()`
once a second to find edits to included files. An edit to an included file is seen up to a
second late, with no origin time, so it is left out of edit-to-see.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
