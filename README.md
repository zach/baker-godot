# Baker for Godot

An integration of [Baker](https://github.com/zach/baker) with [Godot](https://godotengine.org) 4.5:
tuning data in layers (`default`, `local`, `agent`, experiments), each value knowing where
it came from, and values tuned while playing written back to their line in the data.

Status: steps 1 to 3 of 4: the runtime, the editor loop, and agents and exports (see
"Plan" below).

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
  read. In debug builds it reloads a layer file when it changes on disk, within a frame.
- **`BakerBinding`**, a node that binds properties of its parent to paths in the data:

  ```
  Player (CharacterBody2D, @export var jump_height := 80.0)
  └─ BakerBinding   bindings = {"jump_height": "player.jump.height"}
  ```

  The property holds the data's value and follows every reload, layer toggle and live
  edit. GDScript reads it as a plain property, with no call into Baker per frame. A path
  the data lacks leaves the scene's value standing.
- **The editor plugin** (`addons/baker/plugin.cfg`):
  - a dock with **Layers** (toggle each, in the running game too; hover to see what a
    layer changes), **Tweaks** (the edited scene's bound properties, their values and the
    file and line each comes from), and **Session** (what the running game tuned, kept
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

  The `changed` signal fires after every change of the data.

## Agents

An agent works on the game's data through the same surface a person's tools use, in
debug builds, with nothing to parse that Baker did not write as JSON. It is the surface the
MCP bridge of baker-mcp is planned against; the bridge itself is not built yet.

- **The game as its own tool.** Run headless with the scene loaded, so the bound
  properties are tweaks:

  ```sh
  godot --headless --path sample -- --baker check --json         # baker.check/1
  godot --headless --path sample -- --baker reference --json     # vocabulary, tweaks, shapes
  godot --headless --path sample -- --baker eval --json "{config.explain player.jump.height}"
  godot --headless --path sample -- --baker diff                 # what agent.dta changes
  godot --headless --path sample -- --baker promote --patch agent.patch
  ```

  `--out FILE` writes the result to a file too. `promote` writes nothing but the patch: a
  person applies it (`git apply -p1`) or keeps the values from the editor.
- **Scenarios** drive the player and judge the run by written objectives:
  `-- --scenario reach` (does a jump clear the first platform?) and `-- --scenario jump`.
  Each prints one line of JSON (`baker-godot.scenario/1`) and exits 1 if an objective
  failed.
- **The agent layer.** An agent proposes in `data/agent.dta`, which version control
  ignores; `check`, `diff` and the scenarios then judge the proposal.
- **The running game's console**, on 127.0.0.1 only: `-- --console-port 7000`. A line
  `(1 {config.toggle agent})` is answered with one line of `baker.reply/1.1` JSON. Baker's
  Config tools are bound (`config.explain`, `config.set`, `config.toggle`,
  `config.journal`, ...); the ones that write files only with `--console-write`.
  `tests/console.py` is a client.

Tool commands and scenarios leave out the `local` layer, so their results do not depend on
whose machine runs them. `tests/agent.sh` runs an agent's loop end to end: `reach` fails,
a proposal in `agent.dta` passes `check` and makes `reach` pass, A/B over the console, and
a promote patch that applies as a one-line diff.

## Exports

A release export is a Baker shipping build: the data is packed (`export_presets.cfg`
includes `*.dta`) and read as text, without caches, which are tied to Baker's version and
pointer size. It does not reload, take live edits, write, or open a console.
`tests/export.sh` exports the sample and runs a scenario in the exported game.
`tools/fetch_template.py` installs one platform's export template without downloading the
whole bundle.

## The sample

`sample/` is a small platformer with ten tunables in `data/tuning.dta`, an experiment layer
`floaty` (F1 while playing), and a headless scenario:

```sh
godot --headless --path sample -- --scenario jump       # checks the jump's apex
godot --headless --path sample res://tests/runtime_test.tscn
tests/e2e.sh path/to/godot                              # the demonstration, headless
tests/agent.sh path/to/godot                            # an agent's loop
tests/export.sh path/to/godot                           # a release export, run
```

With `BAKER_SHOTS=<dir>`, the end-to-end test also saves screenshots of the editor at each
step (run it under a display, such as `xvfb-run`).

## Building

CMake fetches godot-cpp (4.5) and Baker 0.1.1, and puts the library in
`sample/addons/baker/bin/`:

```sh
cmake -B build -DGODOTCPP_TARGET=template_debug && cmake --build build
cmake -B build-release -DGODOTCPP_TARGET=template_release && cmake --build build-release
```

A release build is a Baker shipping build: no reloads, live edits or write-back. To build
against a local Baker checkout, add `-DFETCHCONTENT_SOURCE_DIR_BAKER=/path/to/baker`.

## Plan

1. **Runtime** (done): the autoload, Godot's file access, layers from the project file,
   `BakerBinding`, reload, and the sample.
2. **Editor loop** (done): a dock (layers, tweaks, the running game's journal with Keep and
   Discard), an inspector plugin showing a property's layer chain, the journal sent over
   the debugger channel, and write-back from the editor process.
3. **Agents and shipping** (done): the console and Baker's Config tools in debug builds,
   the game's tool commands and scenarios headless, and exports that load text.
4. **Requirements back to Baker**, as notes in `requirements/`.

## Requirements for Baker

When this integration needs something Baker lacks, it is written down in `requirements/`,
in Baker's own terms, from `requirements/0000-template.md`. Only the text of a requirement
goes to Baker, never code from here.

## License

This repository is licensed under the MIT license (`LICENSE`), as Godot and godot-cpp are.
Baker itself is MIT-licensed and is used here as a dependency. Nothing in this repository
is part of Baker. It contains no Godot engine source; godot-cpp is fetched at build time.

Godot is a product of its owners, who have not endorsed this project.
