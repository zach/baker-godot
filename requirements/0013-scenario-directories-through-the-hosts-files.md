# 0013: Scenario and objective directories read through the host

## The need

A project's `(scenarios ...)` and `(objectives ...)` entries name directories the kit can
read wherever the stack's files live: through the host's reader, as the layers and single
scenario files already are. Either the host can list a directory (a callback that gives
the `.dta` files in it, in name order), or an entry can name several files, so a project
that keeps its scenarios in many files names them without a directory.

## Why

A packaged game keeps its data in an archive the engine opens, not in directories on the
disk. The kit reads a single scenarios file through the host's reader, so it works in a
package, but it lists a directory with the disk's own calls, so a directory in a package
reads nothing: the game finds no scenarios, and a `sim` run in the shipped game, which is
how a release is checked against its objectives, runs nothing. During development the same
directory reads, so the gap shows only in the build that ships. Any host whose files live
in an archive, a virtual file system or a remote store has it, and any project large
enough to keep one scenario per file runs into it.

## Constraints

- The order stays the one the kit gives today: files in name order, so results do not
  depend on how the host lists them.
- Without a listing callback, directories read from the disk as today.
- A file the listing names is read through the host's reader, as a single file is.

## Today

The integration names one scenarios file and one objectives file, which work in the
package and outside it. A project with many scenarios would keep them in one file.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
