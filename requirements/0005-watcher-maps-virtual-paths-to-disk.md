# 0005: The watcher maps a host's virtual path to its file on the disk

**Shipped** in Baker 0.2.0 (the watcher's disk-path hook, `Watcher::set_disk_path_hook` and
`LiveOptions::disk_path`); this integration uses it through the host loop and has dropped
its own watcher.

## The need

A host can tell the watcher where a path it names virtually lives on the disk, so the
watcher watches that file through the operating system's notifications, as it watches any
file it can reach. Today the stamp hook can only answer with a stamp: a path the hook
takes is checked by its write time and size, at most every 100 ms, and never gets the
system's notifications.

## Why

Engines name their files through virtual roots: a project root, a per-user root. During
development each root is an ordinary directory, and a host can say which; only a packaged
game's files live in an archive. A host whose stack names its layers relative to its
project root ("data/tuning.dta", read through its own reader) must route every path
through the stamp hook today, because the path means nothing relative to the process's
working directory. So every file of its stack is polled, edits are seen up to 100 ms late,
and the system's notifications go unused for files that are on the disk all along. With a
mapping, the host keeps its virtual names (in sources, reloads and loop statistics) and
the watcher gets real files.

## Constraints

- The mapping is asked when files are watched (watch()), not each frame: a quiet frame
  still costs one non-blocking call and allocates nothing.
- The hook may decline a path (it has no file on the disk: a packaged archive's), and the
  stamp hook or the disk as given is used, as today.
- What the watcher reports (changed(), the reload's origin) still names the path as the
  host gave it, not the disk path.
- The same mapping serves a live loop that opens the stack through the host's reader.

## Today

The integration does not use the watcher yet. Its own watcher maps each of the stack's
files to its disk path with the engine's call for that, and checks each file's write time
and size once a frame.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
