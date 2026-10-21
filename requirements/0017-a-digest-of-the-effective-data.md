# 0017: A digest of the data a Config holds

## The need

A host can ask a Config for a digest of its effective data: a short, stable value that is
the same on every machine, platform and build holding the same data, and different when
any value differs. It is cheap enough to compute at a session boundary (a match's start, a
save, a reconnect) and short enough to send between peers or store in a save.

Two digests serve different questions, and a host may want either:

- **of the merged tree:** the values the game reads, whatever layers produced them; two
  stacks that merge to the same data have the same digest;
- **of the stack:** each enabled layer's name and content, in order; it changes when a
  layer is toggled or edited even if the merged values happen to agree.

A live edit in scratch is part of the effective data, so it changes the digest of the
merged tree.

## Why

Several kinds of host run the same rules on several machines from the same data and must
agree on it:

- **lockstep multiplayer,** where peers send only their inputs and each computes the
  outcome. A peer with one different number (one person's own layer, an experiment left
  on, a value tuned live, a content pack the other lacks) drifts from the others with no
  error, and the match is decided by the difference;
- **replays,** which reproduce a match from its inputs and are wrong on any other data;
- **dedicated servers and clients** that both simulate;
- **saves,** which a game may want to refuse, or warn about, when loaded under different
  data.

Each host can hash the files it reads itself, but that digest differs between hosts that
read the same data through different file access (an archive, a platform's file system,
line endings, include order). It also cannot see a live edit, and it cannot say whether two
stacks merge to the same values. Baker holds the merged tree and knows its own
representation, so a digest it computes is the same everywhere the data is.

## Constraints

- **Stable:** the same data gives the same digest on every platform, byte order and build,
  and across Baker versions within a major version. It does not depend on memory addresses,
  hash-table order, symbols' interning order, or the time.
- **Defined by value:** numbers by their value and type, symbols and strings by their text,
  arrays in order. The source locations and comments of the data are not part of it.
- **Short:** fixed length, such as 64 or 128 bits, writable as text.
- **Cheap:** linear in the size of the tree, done when asked, never on a hot path, and
  allocating nothing it keeps.
- **Not a security boundary:** it detects difference, not tampering, so it may be a
  fast non-cryptographic hash, as long as it is documented as such.

## Today

The integration has no lockstep game yet. Its plan for one hashes the files of the enabled
layers, in order, through the engine's file access, with the stack's names; that misses
live edits and cannot match a peer that reads the same data another way.

## Not included

This note contains no code, identifiers, structure or documentation from Godot, and
nothing confidential.
