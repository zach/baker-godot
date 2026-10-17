# Baker for Godot

An integration of [Baker](https://github.com/zach/baker) with [Godot](https://godotengine.org) 4.5:
tuning data in layers (`default`, `local`, `agent`, experiments), each value knowing where
it came from, and values tuned while playing written back to their line in the data.

Status: step 1 of 4, the runtime. The editor dock, write-back from the editor, and the agent
tools come next (see "Plan" below).

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
- **A GDScript API** on `Baker` for everything else:

  | Call | |
  |---|---|
  | `get_value(path, fallback)` | the value, read as the fallback's type |
  | `tweak(path, fallback)` | the same, cached until the data next changes |
  | `source(path)`, `explain(path)` | where the value comes from; every layer that has a say |
  | `get_layers()`, `toggle_layer(name)`, `set_layer_enabled(name, on)` | the stack, and A/B |
  | `set_value(path, value)`, `unsaved()`, `journal()`, `clear_scratch(path)` | live edits |
  | `write_back_to(path, "local")` | keep a value in your own layer, in `user://` |
  | `reload()`, `loop_stats()` | reload now; how long edits take to be seen |

  The `changed` signal fires after every change of the data.

## The sample

`sample/` is a small platformer with ten tunables in `data/tuning.dta`, an experiment layer
`floaty` (F1 while playing), and a headless scenario:

```sh
godot --headless --path sample -- --scenario=jump       # checks the jump's apex
godot --headless --path sample res://tests/runtime_test.tscn
```

## Building

CMake fetches godot-cpp (4.5) and Baker at a pinned baker-dev commit, and puts the library
in `sample/addons/baker/bin/`:

```sh
cmake -B build -DGODOTCPP_TARGET=template_debug && cmake --build build
cmake -B build-release -DGODOTCPP_TARGET=template_release && cmake --build build-release
```

A release build is a Baker shipping build: no reloads, live edits or write-back. To build
against a local Baker checkout, add `-DFETCHCONTENT_SOURCE_DIR_BAKER=/path/to/baker-dev`.

Baker is pinned to a baker-dev commit rather than a release, because the published 0.1.0
predates the project file, `explain`, and tweaks with paths made at runtime.

## Plan

1. **Runtime** (this): the autoload, Godot's file access, layers from the project file,
   `BakerBinding`, reload, and the sample.
2. **Editor loop**: a dock (layers, tweaks, the running game's journal with Keep and
   Discard), an inspector plugin showing a property's layer chain, the journal sent over
   the debugger channel, and write-back from the editor process.
3. **Agents and shipping**: Baker's console and config tools in debug builds, a headless
   runner, and exports that load text.
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
