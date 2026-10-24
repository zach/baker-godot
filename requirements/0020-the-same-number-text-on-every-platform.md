# 0020: The same text for a number on every platform

**Shipped** in Baker 0.6.0; the runtime test's write-back of 120 checks the text on macOS.

## The need

Baker writes a number (a write-back, a promote, a patch, a value in JSON or the reference)
as the same text on every platform and standard library: the shortest text that reads back
as the same value, without an exponent where the number has none to need. Today a build
against Apple's standard library writes 120.0 as `1.2e+02`, where every other platform
writes `120.0`.

## Why

The point of write-back is a diff a person reads: one line, the old value and the new.
On a Mac, keeping a value tuned in the running game writes `(height 1.2e+02)` into the
data, where a teammate on another platform writes `(height 120.0)`; a promote patch made on
a Mac carries the same text, and so does anything that writes values out (a snapshot, an
export of the data). The value reads back the same, so nothing fails at runtime, but the
data's text changes form by whose machine touched it last, every review sees noise, and a
person reading the file finds an exponent in a hand-written table of small numbers. Every
studio with a Mac in it hits this, and most Godot, Unity and Unreal teams have one.

The cause is in the fallback formatter used where the standard library lacks a double
`to_chars` (Apple's, for older deployment targets): it prints with `%.*g` at the shortest
precision that reads back, and `%g` switches to an exponent whenever the precision is below
the number of digits before the point, so 120 at precision 2 is `1.2e+02`.

## Constraints

- **Byte for byte the same** as the `to_chars` path for every double: the shortest
  round-trip digits, fixed notation where the shortest form is fixed, an exponent only
  where the shortest form needs one, and the same whole-float form (`120.0`) the writer
  uses elsewhere.
- **Locale-independent,** as today.
- **A test that runs on Apple's library,** or that forces the fallback on every platform,
  so the two paths cannot drift again.

## Today

The integration's runtime test writes a tuned value back to a layer and reads the file:
it passes on Linux and Windows and fails on macOS, where the file holds `1.2e+02`. The
integration cannot work around it, since the text is Baker's.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
