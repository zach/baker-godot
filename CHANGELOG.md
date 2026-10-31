# Changelog

Releases of Baker for Godot, newest first. Versions follow [semantic
versioning](https://semver.org); before 1.0.0, a minor version may change the API.

## Unreleased

On Baker 0.7.0 and Godot 4.7.

- **Godot 4.7.** The integration builds with godot-cpp 10.0.0 at API 4.7 and runs on Godot
  4.7.2; the extension's `compatibility_minimum` is 4.7. CI, the export template and the
  docs follow. Phase 3's plan is `dev/phase-3-plan.md`.
- **The C# API, `Baker.Godot`,** a NuGet package: typed reads (`Get<T>`, `GetTable`,
  `GetList`), live edits, layers (pushed, toggled, as text, from a directory), rules
  (`CallFunction<T>`, `Eval`, `BindFunction`), records, lockstep, the `Changed` event, and
  `Tweak<T>`, a value current after every change. The sample is a C# project too
  (`BakerSample.csproj`) and runs in Godot's .NET build; `tests/cs_test.tscn` tests the
  package. CI runs every suite in the .NET build and packs the package; the Release
  workflow attaches `Baker.Godot.VERSION.nupkg` to the draft.
- **Values worker threads read:** `PublishedValues`, declared paths and tables published at
  the frame's start after a change as an immutable `Snapshot`, read on any thread with no
  engine call (about 9 ns a read, against about 8 µs through the autoload). The C# test
  shows four threads never see half of a change; `tests/cs_bench.tscn` measures the costs.
- **The skirmish in C#** (`sample/skirmish/cs/`, `-- --skirmish cs`): the match, with each
  side's turn on a worker thread reading a published snapshot, the catalog, the rules and
  the cards' field; `tests/port.sh` shows `sim` over seeds 1-5 giving the GDScript
  skirmish's results run by run, and `compare` moving nothing. The evaluation's skirmish
  tasks rerun on the port (`eval/harness.py --skirmish cs`) succeed 18 of 18, with the
  same fixes as on the GDScript skirmish.

## 0.2.0 - 2026-10-31

Phase 2: what a game shaped like a real one needs, proven on a second sample, a turn-based
skirmish with tables, rules, records, plugins and two peers. Built on Baker 0.7.0 and
godot-cpp `godot-4.5-stable`, for Godot 4.5.1, on Linux, Windows and macOS (Apple silicon).
macOS was last verified in CI at phase 2's step 7: its CI job is paused, its runners
unavailable since; steps 8 and the release changed GDScript, data, tests and docs, not C++.

- **Baker 0.7.0.** Metadata after a value is spelled with `@`, `(level_bonus 2 (@desc
  "..."))`, and a file declares its shapes with `#shape`: the sample's data is migrated with
  the release's `baker_migrate_metadata` and `baker_migrate_declarations`. A field may now be
  named `kind`, `range` or `desc`, and a table read leaves out only `(@...)`. A value kept in
  a layer's file that lacks its path is written as a row, `(/player.jump.height 120.0)`.

- **Baker 0.6.0.** The kit's tool-command flags begin `--baker-`: `--baker sim
  --baker-scenario jump --baker-seeds 1-3 --baker-results DIR`, `--baker-json`, `compare
  --baker-before A --baker-after B`, `promote --baker-patch FILE`, and `--baker-output FILE`
  in place of `--out`. Results are `baker.sim/1.3`, checks `baker.check/1.4`.
  `fingerprint()` is Baker's digest of the data; a read of a record sees the live edits
  beneath it through Baker; a number is written as the same text on macOS as elsewhere.

- **`check` holds calls to the game's functions to their parameters.** A function bound
  with `Baker.bind_function` is declared with its GDScript signature: its parameters'
  types (bool, int, float, String, StringName; others take any value), the ones with
  defaults optional. `check` catches `{player.hold_jump 1 2 3}` or `{player.move left}`
  in a scenario before it runs, and a call is held to the same signature. A lambda's
  parameters are counted, each any value.
- **Enum properties carry their choices.** A bound property's `@export_enum` is its
  tweak's allowed values (a String's strings, an int's named choices), which
  `reference --json` gives and `check` holds the data to.
- **Scenario directories, in exports too.** The project file may name a directory of
  scenarios or objectives, listed through Godot's file access, so an exported game finds
  them in its package. The sample keeps one scenario a file in `sim/scenarios/`.
- **`--console-port 0`** serves the console on a free port, which the game prints.
- **Bindings at scale.** A path is one tweak, shared by every node that binds it, and
  destroyed with the last of them: sixty units of two kinds bind two tweaks. A live edit of
  one node's bound property is an edit of its path, so of every node that binds it. A node
  whose path the data lacks keeps its own scene value, and gets it back when the data
  drops the path.
- **Paths keyed by the node:** `"units.{kind}.max_hp"` reads the node's `kind` as it binds.
- **`BakerBinding.property_changed(property)`** follows each assignment from the data.
- **Tables, read whole.** `get_value(path, {})` reads a keyed table as a Dictionary of
  records (String keys), and `get_value(path, [])` a list of one-tag records as an Array of
  them; metadata after a value is left out. `set_value(path, Dictionary)` sets each field
  it names as its own live edit, and a table read includes the live edits beneath it.
- **Runtime layers:** `push_layer(name, path, {optional, enabled, below, above})`,
  `push_layer_text(name, text, options)` (from the text alone; `#include` refused) and
  `remove_layer(name)`, for content found after startup.
- **`bind_resource(resource, bindings)`** binds a Resource's properties on the shared
  tweaks, a shader's parameters among them; a property that holds nothing yet binds as
  the type it declares.
- **The dock's Data tab and presets.** Every value in the data and every bound property of
  every scene, grouped by path, with its source and chain; presets of layer toggles
  (`baker/presets`) that a game started afterwards takes. `addons/baker/scan.gd` reads the
  bindings of scenes no one has loaded from their files, and the tool commands bind them,
  so `reference` and `check` know every tunable; `data_paths()` lists the data. `--baker
  csv` gives the tweaks as a spreadsheet.
- **Turn scenarios.** The skirmish's match (`skirmish/match.gd`) plays a turn a step: the
  `economy` scenario is written in turns, ends on the game's own outcome (`until`, named
  `army_ready`), and judges outcomes over seeds; `agent.sh` shows a dearer tank breaking it
  in `compare`.
- **Builds, plugins and lobbies.** The build's feature tags as defines (Godot's own, and
  those `baker/feature_defines` lists), for `#ifdef` and a project layer's `(when ...)`;
  `set_defines`, `get_defines`. `push_layer_dir` and a project's `(dir ...)` layers over
  Godot's file access, in exports too; `{plugin}` in a binding's path, the folder of the
  node's scene. `layer_text` and `set_layer_text`, for a lobby's layer; the lockstep peers
  send it. The sample's `cannon` plugin, a server layer, and a `match` scenario that the
  export test runs as a server build.
- **Records that are resources.** `bind_records(prefix, dir, {key, group})` binds every
  resource of a directory by its `@export_group("Tuning")` properties, one record a file,
  in exports too; `record`, `records`, `unbind_records`. A property referencing a record is
  its key, checked against the table; a `Curve` binds as Baker's curve, and sets and writes
  back as its points. A bound Resource changed from outside is a live edit, as a node is;
  `explain()` names the resource beneath the layers. The sample's cards
  (`skirmish/cards/*.tres`), and a `cards` scenario run in the export test.
- **Rules in Baker's script.** `call_function(name, args)` calls a script function with
  Godot values, and `eval(text)` evaluates script text; both return Godot values, in a tool
  command's or `sim`'s Context while one runs. A rule the data defines in a layer's
  `#autorun` is changed by a layer above defining it again: the sample's damage rule, its
  `piercing` experiment, and a `duel` scenario that judges it.
- **Lockstep.** `fingerprint()`, the data as the game reads it in sixteen hex digits,
  the same on every platform for the same data; `lock()`, which holds every change to the
  data (toggles, live edits, layers pushed or removed, files saved, the console) until
  `apply_held()` at a turn's boundary, or `unlock()`; `is_locked()`, `held_count()`.
  `sample/skirmish/peer.gd`: two peers refuse a match on different data.
- **A reload lands at the start of a frame,** before its first physics step or its process
  step, rather than during the autoload's process step: no node in a frame sees part of one.
- **`tests/e2e.sh` exits with the editor's status;** it had always exited 0.
- **The skirmish's tables:** units, buildings and the AI's opening in
  `sample/data/skirmish.dta`, held to their shapes by `check`, and read whole by
  `sample/skirmish/catalog.gd`, the start of the phase 2 sample.

## 0.1.0 - 2026-10-21

The first release: Baker's layered tuning data in a Godot 4.5 game, its editor, its tools
and its exports. Built on Baker 0.4.0 and godot-cpp `godot-4.5-stable`, for Godot 4.5.1, on
Linux and Windows.

- **The runtime.** The `Baker` autoload opens `baker-project.dta`: the layers (`default`,
  `local`, `agent`, experiments), read through Godot's file access. In debug builds it runs
  Baker's live loop: a layer file saved on disk shows in the running game within a frame.
  `get_value`, `tweak`, `source`, `explain`, `set_value`, `write_back`, `layer_effect`,
  `journal` and the `changed` signal, from GDScript.
- **`BakerBinding`.** Binds properties of a node to paths in the data, as tweaks of the
  property's own type (bool, int, float, String, StringName, Vector2, Vector3, Color). The
  property follows every reload, layer toggle and live edit, and costs nothing per frame. A
  property's `@export_range` is its tweak's range and step.
- **The editor.** A dock (layers, tweaks, and the running game's unsaved values, with Keep
  and Discard), the layer chain under each bound property in the inspector, and values
  tuned in the running game written back from the editor to their line, comment intact.
- **Tools and agents.** Baker's tool commands from the game: `check`, `reference`, `eval`,
  `diff`, `promote --patch`, `sim` and `compare`, headless (`-- --baker COMMAND`), and
  Baker's loopback console in debug builds. `check` holds the data to the bound
  properties' types and ranges. An agent skill (`.claude/skills/sample-agent`) teaches the
  loop over them; real agents fixed every planted problem in its evaluation (`eval/`).
- **Scenarios.** Scenarios and objectives are Baker data (`sample/sim/`), run by Baker's
  `sim` from the game's own physics ticks, a fresh scene each run, with `baker.sim/1.1`
  results and `compare`. `Baker.bind_function` makes GDScript functions Baker script
  functions, for scenarios, objectives, `eval` and the console.
- **Exports.** A release export is a Baker shipping build: no reloads, live edits, writes
  or console. The export step leaves out the layers marked `dev_only` (one person's tuning,
  an agent's proposal), and the exported game runs `sim` and `compare`.
- **The sample.** A platformer whose feel is tuned in data, with two scenarios, the tests
  that run it all headless (`tests/`), and CI on Linux and Windows.
