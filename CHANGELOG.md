# Changelog

Releases of Baker for Godot, newest first. Versions follow [semantic
versioning](https://semver.org); before 1.0.0, a minor version may change the API.

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
