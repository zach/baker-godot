# Baker for Godot

An integration of [Baker](https://github.com/zach/baker) with [Godot](https://godotengine.org) 4.7:
tuning data in layers (`default`, `local`, `agent`, experiments), each value knowing where
it came from, and values tuned while playing written back to their line in the data.

Version 0.3.0 (`CHANGELOG.md`), on Baker 0.7.0 and Godot 4.7.2, for Linux and Windows: the
runtime, the editor loop, agents and exports (phase 1); what a game shaped like a real one
needs (phase 2): bindings at scale, tables, runtime layers and records, lockstep, rules in
script, turn scenarios, the dock's data view, and a harder evaluation; and C# (phase 3):
the `Baker.Godot` NuGet package, values worker threads read, and the skirmish in C#.

## The demonstration

1. Play the sample from the editor.
2. Select the player in the remote scene tree and change Jump Height in the inspector.
3. Stop the game.
4. The Baker dock's Session tab lists the change. "Keep" writes it to the line it came
   from: a one-line diff to `data/tuning.dta`, its comment intact.
5. While playing, toggle the `floaty` experiment in the dock's Layers tab (or press F1).

`tests/e2e.sh` runs all five steps in a headless editor, sending the game the same message
the remote inspector sends, and checks the diff.

## What it gives a Godot game

- **The `Baker` autoload** (`addons/baker/baker.gd`, a `BakerRuntime`) opens
  `baker-project.dta` at the project root: the same layer stack Baker's editor and checker
  read. It runs Baker's host loop (`baker::host::Live`): in debug builds it reloads a layer
  file when it is saved, within a frame, watching the files through the operating system
  (res:// and user:// are mapped to their directories on the disk). A reload lands at the
  start of a frame, before its first physics step or else its process step, so every node
  in that frame, and any work a node starts, reads the whole reload or none of it.
- **`BakerBinding`**, a node that binds properties of its parent to paths in the data:

  ```
  Player (CharacterBody2D, @export var jump_height := 80.0)
  └─ BakerBinding   bindings = {"jump_height": "player.jump.height"}
  ```

  The property holds the data's value and follows every reload, layer toggle and live
  edit. GDScript reads it as a plain property, with no call into Baker per frame. A path
  the data lacks leaves the scene's value standing.

  Each path is one tweak, however many nodes bind it: a hundred units of one kind share
  it, and a live edit of one is an edit of the kind. A path can take its key from the node,
  so one binding in a base scene serves every scene that inherits it:

  ```
  bindings = {"max_hp": "units.{kind}.max_hp"}   # the node's `kind` names the record
  ```

  `property_changed(property)` follows each assignment from the data, for state the node
  derives from it (current hit points from the maximum).

  **A bound property is a tunable, not game state.** Under the editor's debugger, a bound
  property whose value changes from outside is taken for a person tuning it, and becomes a
  live edit. So game code must not write to a bound property at runtime: keep modifiers
  (a buff, a level bonus, damage taken) in properties of their own, beside the exported
  value they modify.
- **Layers the game finds after it starts.** `Baker.push_layer("dlc1", path, {"below":
  "local"})` adds a content pack, a downloaded map's settings or a mod, watched as the
  project's layers are; `push_layer_text(name, text)` adds one from received text, read from
  the text alone with any `#include` refused; `remove_layer(name)` leaves no trace.
- **Resources.** `Baker.bind_resource(stats, {"max_hp": "units.{kind}.hp"})` binds a
  Resource's properties as a `BakerBinding` binds a node's, with the same shared tweaks; a
  shader's parameters are a `ShaderMaterial`'s (`"shader_parameter/glow"`), so one path
  can feed a script's value and a shader. It returns the binding; `unbind_resource` ends it.
- **Builds, plugins and lobbies.**
  - The build's feature tags are defines: Godot's own (`debug`, `release`, `pc`, `linux`,
    ...) and the custom tags the project setting `baker/feature_defines` lists. A project
    layer `(server "data/server.dta" (when server))` is in a dedicated server's export and
    no other; `set_defines` changes them while the game runs.
  - A directory of layers, `(plugins (dir "plugins") optional)` in the project file or
    `Baker.push_layer_dir("mods", "user://mods")` from the game: each `.dta` file, and each
    folder with a `layer.dta`, is a layer, found again at every reload, in an export too. A
    plugin's scene binds `"minigames.{plugin}.gravity"`, `{plugin}` being its folder's name
    (or the node's own `plugin`), so its pack supplies it and write-back lands in its file.
  - A lobby's settings: `Baker.layer_text("lobby")` on the host, `push_layer_text` on a
    peer, and `set_layer_text` when they come again; `sample/skirmish/peer.gd` sends them
    before the fingerprints are compared.
- **Records that are resources.** For a game whose content is the engine's own resources,
  a file a card, Baker is the layers above them: `Baker.bind_records("cards",
  "res://skirmish/cards")` binds every resource in the directory, the properties under its
  script's `@export_group("Tuning")` (or the group `{"group": ...}` names) to
  `cards.<file>.<property>` (or the field `{"key": ...}` names). The data holds only what a
  layer changes; the file is the bottom of the chain, and `explain()` names it. A property
  that references a resource of a class bound as records is the record's name in the data,
  checked against the table's names; a `Curve` is its points, `(scaling (1 1) (3 2))`, and
  Godot samples it as Baker does. `record(prefix, key)` and `records(prefix)` give the
  resources; an exported game lists the directory from its package.
- **Rules in Baker's script.** A rule is a script function the data defines in a layer's
  `#autorun`, so a layer above changes it by defining it again: an experiment, an agent's
  proposal, a mod. The game calls it with `Baker.call_function("skirmish.damage", [6, 2,
  0])`, and `Baker.eval("{skirmish.damage 6 2 1}")` evaluates script text; both give Godot
  values. A rule reads tunables with `{config.get path}`. The sample's damage rule is in
  `data/skirmish.dta`, the `piercing` experiment defines it again (armor stops half), and the
  `duel` scenario judges it, so `sim` and `compare` see a changed rule as they see a changed
  number.
- **A match in lockstep.** `Baker.fingerprint()` is the data as the game reads it, as
  sixteen hex digits: peers compare theirs before a match. `Baker.lock()` returns it and
  holds every change until `apply_held()`, which a game calls at a turn's boundary and which
  returns the new fingerprint: a toggle from the game or the dock, a live edit, a layer
  pushed or removed, a file saved, and the console. `unlock()` applies what waited.
  `sample/skirmish/peer.gd` is a match's first exchange between two processes, and
  `tests/lockstep.sh` runs it: peers on the same data agree; a peer with an extra layer is
  refused.
- **Text a player reads stays in Godot's translation system.** Data names it by its
  translation key (`(name CARD_FIREBALL)`), and the game passes the key to `tr()`.
- **Tables, read whole.** With a Dictionary fallback, `Baker.get_value("units", {})` is a
  keyed table as a Dictionary of records, `{"tank": {"hp": 10, "price": 50, ...}, ...}`; with
  an Array fallback, a list of records with one tag is an Array of them
  (`(opening (build infantry) (build tank))` at `ai.opening` is `[&"infantry", &"tank"]`).
  Metadata after a value, `(@range 0 10)` or `(@desc "...")`, is left out; any other name,
  `kind` or `range` too, is a field. Shapes in the data hold the tables to their fields
  (`sample/data/skirmish.dta`). `set_value("units.tank", {"price": 45})` sets each field it
  names as a live edit of its own, and a table read sees them.
- **The editor plugin** (`addons/baker/plugin.cfg`):
  - a dock with **Layers** (toggle each, in the running game too; hover to see what a
    layer changes; save the toggles as a preset, shared in the project setting
    `baker/presets`, and "Play with" one: a game started afterwards takes the Layers tab's
    toggles), **Data** (every value in the data and every bound property of every scene,
    loaded or not, grouped by path, with the layer, file and line it comes from and its
    whole chain on hover), **Tweaks** (the edited scene's bound properties, their values and
    the file and line each comes from), and **Session** (what the running game tuned, kept
    after it stops, with Keep and Discard);
  - under each bound property in the inspector, every layer that has a say in its value,
    with its file and line, and which one wins.
- **Write-back from the editor.** Under the editor's debugger, a value set on a bound
  property from outside (the remote inspector) becomes a live edit. The game sends its
  unsaved values to the editor over the debugger channel, and Keep writes them from the
  editor process, through Baker's write-back, which replaces only the value's tokens. The
  editor's file system is told about the change. A game never writes into the project; it
  may write only into `user://`. Run on its own, without the debugger, a game makes no
  such comparisons.
- **A GDScript API** on `Baker` for everything else:

  | Call | |
  |---|---|
  | `get_value(path, fallback)` | the value, read as the fallback's type |
  | `tweak(path, fallback)` | the same, cached until the data next changes |
  | `source(path)`, `explain(path)` | where the value comes from; every layer that has a say |
  | `get_layers()`, `toggle_layer(name)`, `set_layer_enabled(name, on)` | the stack, and A/B |
  | `set_value(path, value)`, `unsaved()`, `journal()`, `clear_scratch(path)` | live edits |
  | `write_back_to(path, "local")` | keep a value in your own layer, in `user://` |
  | `session()` | what the editor is sent: unsaved values, layers, bindings |
  | `reload()`, `loop_stats()` | reload now; how long edits take to be seen |
  | `call_function(name, args)`, `eval(text)` | a rule, or any script, with Godot values |

  The `changed` signal fires after every change of the data.

## Agents

An agent works on the game's data the way a technical designer does, with a shell and the
game's own commands, and the agent skill (`.claude/skills/sample-agent/SKILL.md`) teaches
the loop: read, baseline, change `agent.dta`, check, simulate, compare, report, stop. The
commands and their output are the ones Baker's game stacks have, so the same skill's habits
carry over.

- **The game as its own tool.** Run headless with the scene loaded, so the bound
  properties are tweaks:

  ```sh
  godot --headless --path sample -- --baker check --baker-json         # baker.check/1.4
  godot --headless --path sample -- --baker reference --baker-json     # vocabulary, tweaks, shapes
  godot --headless --fixed-fps 60 --path sample -- --baker sim --baker-seeds 1-3 --baker-results runs/before
  godot --headless --path sample -- --baker compare --baker-before runs/before --baker-after runs/after
  godot --headless --path sample -- --baker eval --baker-json "{config.explain player.jump.height}"
  godot --headless --path sample -- --baker diff                 # what agent.dta changes
  godot --headless --path sample -- --baker promote --baker-patch agent.patch
  godot --headless --path sample -- --baker csv --baker-output tweaks.csv  # a spreadsheet
  ```

  The commands see the bound properties of every scene, not only those loaded:
  `addons/baker/scan.gd` reads each packed scene's bindings, values and export hints from
  its file without instantiating it. `csv` is every tweak a row (path, type, value,
  default, min, max, step, one_of, layer, file, line, desc), for a designer's spreadsheet.

  These are Baker's own tool commands (its host kit), reading through Godot's file access.
  `--baker-output FILE` writes a command's result to a file too (a `user://` path as well); `promote`
  never writes into the project: `--baker-patch` gives the change for a person to apply.
- **`sim`** is Baker's: it runs the scenarios in `sample/sim/scenarios/` (all unless
  `--baker-scenario NAME`) and judges each run by the objectives in `sample/sim/objectives.dta`,
  results as `baker.sim/1.3` (in `DIR/results.json` with `--baker-results DIR`), exit 1 if an
  objective failed. Scenarios and objectives are data: they call the player's functions
  (`sample/sim/driver.gd`, bound with `Baker.bind_function`), and `check` holds those calls
  to the functions the game binds: their names, how many arguments, and of which types. The game drives each run from its own frames
  (Baker's `SimRun`): before each run it reloads the scene, and each physics tick is one
  step. **`compare`** says which objectives broke or were fixed between two runs
  (`baker.compare/1`), and exits 1 if one broke. `reach` asks whether a jump clears the
  first platform and stays below the second; the shipped
  data's does not clear it. With `--fixed-fps 60`, Godot runs frames as fast as it can,
  each 1/60 s of game time: the same results, about 20 times sooner than at the clock's
  pace.
- **Turns.** A turn-based game counts its own steps: the skirmish's match
  (`sample/skirmish/match.gd`) plays a turn each physics tick once a scenario starts it, so
  `economy` is written in turns, `(steps 12)`, `(at step 0 {skirmish.start $seed})`, and
  ends when blue's army is ready, `(until {skirmish.outcome})`, which names how it ended
  (`$ended`, `army_ready`). Its objectives judge outcomes ("the opening's tank by turn 5"),
  and a dearer tank in a layer breaks one, which `compare` names.
- **The agent layer.** An agent proposes in `data/agent.dta`, which version control
  ignores. `promote` writes nothing but a patch: a person applies it (`git apply -p1`) or
  keeps the values from the editor.
- **The game's functions in Baker's script.** `Baker.bind_function("player.apex", apex)` makes
  a GDScript function `{player.apex}`: scenarios, objectives, `eval` and the console call it,
  and `check` knows it. A call with the wrong number of arguments is an error at the call.
  Binding a name again replaces it, so a reloaded scene binds its own. While a tool command
  or `sim` runs, the game's reads (`Baker.get_value`, `explain`, ...) see that command's data.
- **The running game's console** (Baker's), on 127.0.0.1 only: `-- --console-port 7000`, or
  `--console-port 0` for a free port, which the game prints (`Baker: console on ...`). A line
  `(1 {config.toggle agent})` is answered with one line of `baker.reply/1.1` JSON. Baker's
  Config tools are bound (`config.explain`, `config.set`, `config.toggle`,
  `config.journal`, ...); the ones that write files only with `--console-write`.
  `tests/console.py` is a client.

Every `--baker` command leaves out the `local` layer, so its results do not depend on whose
machine runs it. `tests/agent.sh` runs the skill's loop end to end: the baseline fails
`reach`; a jump of 200 fixes it but breaks the second objective, and `compare` refuses it; a
jump of 120 passes `check`, `sim` and `compare`; A/B over the console; and a promote patch
that applies as a one-line diff.

A game that exports a dedicated server already has a harness for this: run headless with
an AI in every seat, a match is a scenario and each turn a step, and `sim` and `compare`
judge balance changes on the server build with no display.

## Exports

A release export is a Baker shipping build: the data is packed (`export_presets.cfg`
includes `*.dta`) and read as text, without caches, which are tied to Baker's version and
pointer size. It does not reload, take live edits, write, or open a console. The plugin's
export step leaves out the files of the layers `baker-project.dta` marks `dev_only`, such
as `data/agent.dta`, an agent's unreviewed proposal, which a shipping build would not read
anyway.
`tests/export.sh` exports the sample and runs `--baker sim` in the exported game: `sim` and
`compare` work in shipping builds; the other commands and the console do not.
`tools/fetch_template.py` installs one platform's export template without downloading the
whole bundle.

## The sample

`sample/` is a small platformer with ten tunables in `data/tuning.dta`, an experiment layer
`floaty` (F1 while playing), and a headless scenario:

```sh
godot --headless --fixed-fps 60 --path sample -- --baker sim   # the scenarios, judged
godot --headless --path sample res://tests/runtime_test.tscn
tests/e2e.sh path/to/godot                              # the demonstration, headless
tests/agent.sh path/to/godot                            # an agent's loop
tests/lockstep.sh path/to/godot                         # two peers agree on the data
tests/port.sh path/to/godot-mono                        # the C# skirmish gives the GDScript's results
tests/export.sh path/to/godot                           # a release export, run
```

With `BAKER_SHOTS=<dir>`, the end-to-end test also saves screenshots of the editor at each
step (run it under a display, such as `xvfb-run`).

## Releasing

The Release workflow (Actions tab, "Run workflow", with the version `CHANGELOG.md` names)
builds the extension for Linux and Windows, packs `addons/baker` with it, and drafts the
GitHub release "Baker for Godot VERSION" with that version's changelog entry. Publishing
the draft creates its tag.

## Building

CMake fetches godot-cpp (10.0.0, for Godot 4.7) and Baker 0.7.0 (its core, its host kit and its tools), and puts the library in
`sample/addons/baker/bin/`:

```sh
cmake -B build -DGODOTCPP_TARGET=template_debug && cmake --build build
cmake -B build-release -DGODOTCPP_TARGET=template_release && cmake --build build-release
```

A release build is a Baker shipping build: no reloads, live edits or write-back. To build
against a local Baker checkout, add `-DFETCHCONTENT_SOURCE_DIR_BAKER=/path/to/baker`.

The sample is a C# project as well as a GDScript one, so it runs in Godot's .NET build
(4.7.2), with the .NET 8 SDK. Build its C# before opening it headless, and export it with
the .NET export templates (`tools/fetch_template.py --mono`):

```sh
dotnet build sample/BakerSample.csproj        # the sample's C#, and the package from source
dotnet pack csharp/Baker.Godot -c Release     # the NuGet package Baker.Godot
godot --headless --path sample res://tests/cs_test.tscn   # the C# tests
```

## C#

`csharp/Baker.Godot` is the C# API, published as the NuGet package `Baker.Godot`: typed
calls over the `Baker` autoload, which the addon provides (a game installs both). See
`csharp/Baker.Godot/README.md`.

```csharp
using BakerGodot;
float height = Baker.Get("player.jump.height", 96f);
var gravity = new Tweak<float>("world.gravity", 980f);   // current after every change
int damage = Baker.CallFunction<int>("skirmish.damage", 6, 2, 0);
```

**The skirmish in C#.** `sample/skirmish/cs/` is the skirmish's economy, catalog and rules
ported to C#, selected with `-- --skirmish cs`: the same data, scenarios and objectives,
and the two sides' turns run on worker threads in parallel, each reading a
`PublishedValues` snapshot taken once a turn, so a live edit or a layer reaches both sides
whole at the next turn. `tests/port.sh` runs `sim` over seeds 1-5 with each and shows every
run the same, outcome by outcome, and `compare` moving nothing.

**Values worker threads read.** A `Baker.Get` is a call into the engine, on the main
thread, about 8 µs. For a threaded simulation, declare the paths it reads on a
`PublishedValues` (main thread); after any change, the values are published at the next
frame's start, where a reload lands, as one immutable `Snapshot` swapped by one reference.
A worker takes `values.Current` once a tick and reads it: about 9 ns, no engine call, no
allocation, and never part of a change. Tables come whole, as .NET dictionaries.

```csharp
var values = new PublishedValues();
var price = values.Declare("units.tank.price", 50);
var units = values.DeclareTable("units");
// on a worker, once a tick:
var now = values.Current;
int p = now.Get(price);
long hp = (long)now.Get(units)["tank"]["hp"];
```

## Plan

1. **Runtime** (done): the autoload, Godot's file access, layers from the project file,
   `BakerBinding`, reload, and the sample.
2. **Editor loop** (done): a dock (layers, tweaks, the running game's journal with Keep and
   Discard), an inspector plugin showing a property's layer chain, the journal sent over
   the debugger channel, and write-back from the editor process.
3. **Agents and shipping** (done): the console and Baker's Config tools in debug builds,
   the game's tool commands, `sim` and `compare` headless, the agent skill, and exports that
   load text. (An MCP bridge, plan A, was built and evaluated in baker-mcp; the command
   line and skill needed fewer calls for the same success, so this repository drops it.)
4. **Requirements back to Baker** (done), as notes in `requirements/`: 0001 to 0020 shipped
   in Baker and are in use here; 0021 is built on Baker's main, for its next release.
5. **Phase 2** (done, `dev/phase-2-plan.md`): the skirmish sample, steps 1 to 8.
6. **Phase 3** (done, `dev/phase-3-plan.md`): Godot 4.7, the C# package on NuGet, values a
   threaded simulation reads, and the skirmish ported to C#, giving the GDScript's results.

## Requirements for Baker

When this integration needs something Baker lacks, it is written down in `requirements/`,
in Baker's own terms, from `requirements/0000-template.md`. Only the text of a requirement
goes to Baker, never code from here.

## License

This repository is licensed under the MIT license (`LICENSE`), as Godot and godot-cpp are.
Baker itself is MIT-licensed and is used here as a dependency. Nothing in this repository
is part of Baker. It contains no Godot engine source; godot-cpp is fetched at build time.

Godot is a product of its owners, who have not endorsed this project.
