# 0002: Paths with a scheme, such as `user://`, kept as written

**Shipped** in Baker 0.1.0 (baker-dev eaba0d7, D179: a path with a scheme is absolute and kept as written); this integration uses it and has dropped the
workaround below.

## The need

A path that begins with a scheme (`name://`) is kept exactly as written everywhere Baker
handles paths: in a project file's layer paths, in what a Config passes its reader and
writer, and in `Source::file`. Such a path counts as absolute, so a project file's directory
is not joined onto it, and normalizing it does not fold its `//`.

## Why

Engines name files through virtual roots: one for the game's packaged content, one for
per-user data that version control never sees, others for mounted packages or a network
store. A personal `local` layer belongs in the per-user root, and the project file is the
one place every tool reads the stack from, so it should be able to say so:

```
(local "user://baker/local.dta" optional)
```

## Constraints

- Only a scheme of letters, digits, `+`, `-` and `.` before `://` counts, so a Windows path
  such as `C:/data/a.dta` is unaffected.
- A tool that cannot open such a path treats it as a missing file (an optional layer is
  empty), as it would any file it cannot read.

## Today

Baker normalizes `user://baker/local.dta` to `user:/baker/local.dta`, and joins the project
file's directory onto it when the project file is not at the root. The integration's reader
and writer accept the folded form, and cut any directory joined before `user:/`, so the
project file must name the layer in this one form. Sources show the folded form.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
