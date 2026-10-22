# Phase 2: a turn-based game with rules

The first release proved the integration on a platformer with ten numbers, one scene and
three objectives. The Tanks of Freedom II assessment (`dev/tanks-of-freedom-assessment.md`,
on the branch `assess/tanks-of-freedom`) shows what a game shaped like a real one needs:
many instances of few types, data kept as tables of records, rules written as formulas,
and peers that must agree on the data. Phase 2 builds those, proven on a second sample.

## The second sample

Written here from scratch, small but shaped like that game: a grid, two sides, three or
four unit types in a keyed table with several instances of each, buildings that earn
income, deterministic combat, a simple AI, and two peers run locally as two headless
processes. Its scope is tunables and rules, never scenes. Nothing is copied from the
assessed game, or from any other.

## Steps

| Step | Delivers | Proven by |
|---|---|---|
| 1. Bindings at scale (G1, G2, G10) | One tweak per path, shared by every instance that binds it; paths that take a key from the node (`units.{kind}.hp`); a `property_changed` signal per binding | Many instances of few types register one tweak per path; a live edit reaches every instance of its type; an instance whose path the data lacks keeps its own value |
| 2. Tables (G3) | `get_value("units", {})` gives a Dictionary of Dictionaries; shapes, written in the data, check the tables | The production menu and the AI read the table whole; `check` catches a malformed unit record |
| 3. Runtime layers (G6), and Resources | `push_layer`, `push_layer_text`, `remove_layer` from GDScript; `bind_resource(resource, bindings)`, on the shared tweaks of step 1 (a script's Resource, a shader's parameters) | A content pack found at startup merges into the unit table; removing it restores the base; one path feeds a Resource's value and a shader parameter |
| 4. Lockstep (G5), and a reload at the frame | A fingerprint of the effective data (the integration's own hash until Baker's digest, note 0017) and a lock for a match, built on the Config's freeze if it serves; a reload's assignments staged and published together at the start of a frame | Two peers with different `local` layers refuse a match; a toggle during a match waits for the turn boundary; no frame sees half a reload |
| 5. Rules in script (G4) | `Baker.call(name, args)` and `Baker.eval(text)` returning Godot values; a rules pack (level bonus, damage) in Baker script | An experiment layer changes a rule, not only a number, and `sim` sees it |
| 6. Turn scenarios (G7) | The game reports a step per turn under `--step-hz 1`; objectives on outcomes | "The HQ affords a tank by turn 3", repeatable over seeds, and `compare` on a balance change |
| 7. A data view in the dock (G9) | Every path, grouped by prefix, with its value and layer chain, whichever scene is open | `tests/e2e.sh` |
| 8. Harder agent evaluation | Balance tasks with objectives in tension, on the new sample | `eval/` says where agents struggle |

The evaluation session's feedback on 0.1.0, from two shipped games, confirmed steps 1 to 7
and added Resources (step 3) and a reload published at the frame (step 4); macOS joined CI.

Deferred: reading standalone trees for maps (G8), until a game wants Baker's format for its
content; emitting shapes from a GDScript class, until hand-written shapes prove short.

## Decisions

- **A shared tweak's default is the first binder's value,** and an instance keeps its own
  scene value while the data lacks the path, as a single binding does today. Its type and
  metadata are the first binder's; a later binder of another type is refused with a warning.
- **A live edit of one instance's bound property is an edit of its path,** and so of every
  instance that binds it: right for a type's stats. State a game derives from a stat (current
  hit points from the maximum) re-derives on `property_changed`.
- **Step 6 starts on today's Baker** (a turn as one step at a rate of one); note 0018 (a run
  that ends on a condition, in the host's own steps) replaces the convention when it ships.

## After phase 2

- **Phase 3, C#.** A thin C# package with typed wrappers over the runtime, and a published
  value a threaded simulation reads without a call into the engine per read, with its cost
  written down; proven on a C# sample whose constants are read by worker threads. Larger
  Godot simulations are written in C#.
- **Godot's version line is open.** The integration is pinned to 4.5; games in use are on
  4.7. Moving the line, or building for several at once, is the owner's decision.

## With Baker

- 0017, a digest of the data a Config holds, for step 4.
- 0018, a scenario that ends on a condition, for step 6.
