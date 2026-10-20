# 0004: Project options a host can build before the call and keep

## The need

A host can build the options it opens a project with (the layers to leave out, and the
rest) in one place, keep them in a variable or a member, choose them by a condition, and
pass them later, without the options pointing at storage that is already gone. Today the
layers to leave out are an `std::initializer_list`, whose array lives only until the end of
the full expression that wrote it: options built in one statement and passed in the next
read freed memory.

## Why

Hosts decide what to open from their own settings and command lines: leave out `local`
for a scenario run, a check, or CI; leave out an experiment for a benchmark. The natural
code builds the options first and calls once:

```cpp
const baker::ProjectOptions options =
    skip_local ? baker::ProjectOptions{.skip = {"local"}} : baker::ProjectOptions{};
baker::open_project(cfg, project, options);
```

It compiles without a warning on GCC, Clang and MSVC and reads freed memory. A debug build
often reads the stale array correctly, so the fault shows first in an optimized build: a
run that should leave out `local` keeps it, and its results depend on whose machine it ran
on, which the option exists to prevent. Any host that keeps its options in a struct, or
builds them in a helper, meets the same.

## Constraints

- No allocation for the common case of a few names, as today.
- The names stay `std::string_view`s, as today: a host's own strings or literals.
- Building the options inline in the call, as today's documentation shows, keeps working.
- Options a host keeps, or builds in a helper and returns, are valid for as long as the
  host's own strings are: for example, the names in a fixed-capacity array inside the
  options, or a span over storage the caller owns, documented as such.

## Today

The integration builds the options inside the call (`open_project(cfg, project,
{.skip = {"local"}})`) on one branch, and calls without options on the other, with a
comment saying why. An exported, optimized build found the fault: its scenario runs kept
the `local` layer while the debug build's left it out.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
