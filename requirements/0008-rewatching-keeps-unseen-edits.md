# 0008: Watching a new list of files keeps an edit not yet seen

**Shipped** in Baker 0.2.1 (a file that stays in the watcher's list keeps what was last seen
of it); this integration has dropped its reload on a change of the list.

## The need

When the watcher is given a new list of files (the stack read a new file, a missing one
appeared, an #include was added), a file it already watched keeps what it last saw of
that file, so an edit made since the last poll is still reported at the next one. Today
watch() takes a fresh stamp of every file, so an edit between the last poll and the
re-watch counts as already seen, and the live loop never reloads it.

## Why

The list changes exactly when a person or a tool is busy with the files: a write-back
creates a layer's file that was missing, then the person edits it; a person adds an
#include and saves the included file a moment later; a tool writes several files in one
go. In each case the edit and the change of the list fall between two frames, and the
edit is lost until the next save. The person sees a stale value with nothing to tell
them why. It shows up first on a fresh machine, where a per-user layer's file and its
directory do not exist yet when the game starts.

## Constraints

- A file that stays in the list keeps its last seen stamp, or whatever tells the watcher
  it has changed since; one that is new to the list is stamped as now.
- A file that moves from being checked by its stamp to being watched through the system
  (its directory appeared) still reports an edit made before that move.
- Watching stays as cheap as it is: the work happens in watch(), not in poll().

## Today

The integration reloads once whenever the stack's list of files changes, at the frame
after the re-watch. The reload reads every layer's files, and does so only when the list
changes.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
