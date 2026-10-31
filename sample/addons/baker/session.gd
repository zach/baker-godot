@tool
## The editor's model: its own Baker over the project, the running game's last session,
## and what Keep and Discard do. The dock and the inspector are views of it.
extends RefCounted

signal changed

const Scan := preload("res://addons/baker/scan.gd")

## The editor's Baker: the same stack the game opens, allowed to write into the project.
var baker: BakerRuntime
## The game's last session: {layers, unsaved, bindings}. Kept after the game stops.
var session: Dictionary = {}
var running := false
var last_error := ""

var _debugger
var _synced := false  # the running game has the editor's layer toggles
var _scanned = null  # Scan.scan()'s, until the project's files change


func _init(debugger) -> void:
	_debugger = debugger
	baker = BakerRuntime.new()
	baker.project_writes = true
	if FileAccess.file_exists("res://baker-project.dta"):
		baker.open("res://baker-project.dta")
	debugger.session_received.connect(_on_session)
	debugger.running_changed.connect(_on_running)
	if Engine.is_editor_hint():
		EditorInterface.get_resource_filesystem().filesystem_changed.connect(
			func(): _scanned = null)


func close() -> void:
	baker.free()


func _on_session(s: Dictionary) -> void:
	session = s
	if running and not _synced:
		_synced = true
		_send_layers()
	changed.emit()


func _on_running(now: bool) -> void:
	running = now
	_synced = false
	changed.emit()


## The game just started: it takes the layers as the Layers tab has them (a preset, say)
## where they differ from the project file's.
func _send_layers() -> void:
	baker.reload()
	var mine := {}
	for l: Dictionary in baker.get_layers():
		mine[l["name"]] = l["enabled"]
	for l: Dictionary in session.get("layers", []):
		var name: String = l["name"]
		if name != "scratch" and mine.has(name) and mine[name] != l["enabled"]:
			_debugger.send("toggle", [name, mine[name]])


## Saved sets of layer toggles to play with: {name: {layer: enabled}}, in the project
## setting baker/presets, so a team shares them.
func presets() -> Dictionary:
	return ProjectSettings.get_setting("baker/presets", {})


## Saves the Layers tab's toggles as a preset.
func save_preset(name: String) -> void:
	baker.reload()
	var states := {}
	for l: Dictionary in baker.get_layers():
		if l["name"] != "scratch":
			states[l["name"]] = l["enabled"]
	var all := presets()
	all[name] = states
	ProjectSettings.set_setting("baker/presets", all)
	ProjectSettings.save()
	changed.emit()


## Sets the layers as a preset has them, in the editor and in a running game; a game started
## after takes them too.
func apply_preset(name: String) -> void:
	var states: Dictionary = presets().get(name, {})
	for layer: String in states:
		toggle(layer, states[layer])


## Every value in the data, and every bound property of every scene, loaded or not:
## [{path, value, source: {layer, file, line} or {}, scene: file or ""}], the data's first.
func data_rows() -> Array:
	baker.reload()
	var rows := {}
	for e: Dictionary in baker.data_paths():
		rows[e["path"]] = {"path": e["path"], "value": e["value"],
			"source": baker.source(e["path"]), "scene": ""}
	if _scanned == null:
		_scanned = Scan.scan()
	for e: Dictionary in _scanned:
		if not rows.has(e["path"]):
			rows[e["path"]] = {"path": e["path"], "value": e["value"], "source": {},
				"scene": e["scene"]}
	return rows.values()


## The game's unsaved values: [{path, value, before, layer, file, line}].
func unsaved() -> Array:
	return session.get("unsaved", [])


## The layers as the game has them, or as the project file has them when no game has run.
func layers() -> Array:
	if session.has("layers"):
		return session["layers"]
	baker.reload()
	return baker.get_layers()


## Turns a layer on or off: in the running game, and in the editor's own stack, which the
## inspector shows.
func toggle(layer: String, enabled: bool) -> void:
	baker.set_layer_enabled(layer, enabled)
	if running:
		_debugger.send("toggle", [layer, enabled])
	elif session.has("layers"):
		for l: Dictionary in session["layers"]:
			if l["name"] == layer:
				l["enabled"] = enabled
	changed.emit()


## Writes an unsaved value back to its line: into the file of the layer it came from, with
## the layers as the game had them, replacing only the value's tokens. A path no layer has
## yet is added to the bottom layer. The editor writes, and its file system is told.
func keep(path: String) -> Error:
	var entry := _entry(path)
	if entry.is_empty():
		return ERR_DOES_NOT_EXIST
	baker.reload()
	var states := {}
	for l: Dictionary in baker.get_layers():
		states[l["name"]] = l["enabled"]
	for l: Dictionary in session.get("layers", []):
		if l["name"] != "scratch" and states.has(l["name"]):
			baker.set_layer_enabled(l["name"], l["enabled"])
	var err := baker.set_value(path, entry["value"])
	if err == OK:
		if entry.get("layer", "") != "":
			err = baker.write_back(path)
		else:
			err = baker.write_back_to(path, baker.get_layers()[0]["name"])
	var written: Array = baker.get_last_written()
	last_error = "" if err == OK else baker.get_last_error()
	baker.clear_scratch(path)
	for layer: String in states:
		if layer != "scratch":
			baker.set_layer_enabled(layer, states[layer])
	if err != OK:
		return err
	var fs := EditorInterface.get_resource_filesystem()
	for w: Dictionary in written:
		if String(w["godot_path"]).begins_with("res://"):
			fs.update_file(w["godot_path"])
	_forget(path)
	if running:
		_debugger.send("kept", [path])
	changed.emit()
	return OK


## Drops an unsaved value: the game goes back to the files' value.
func discard(path: String) -> void:
	_forget(path)
	if running:
		_debugger.send("discard", [path])
	changed.emit()


func keep_all() -> Error:
	var result := OK
	for entry: Dictionary in unsaved().duplicate():
		var err := keep(entry["path"])
		if err != OK:
			result = err
	return result


## Every contributor to a path's value in the editor's stack, one line each:
## "default  96  data/tuning.dta:10  wins".
func chain_text(path: String, reload := true) -> String:
	if reload:
		baker.reload()
	var lines := PackedStringArray()
	for e: Dictionary in baker.explain(path):
		var where := "" if e["file"] == "" else "  %s:%d" % [e["file"], e["line"]]
		lines.append("%s  %s%s  %s" % [e["layer"], var_to_str(e["value"]), where, e["standing"]])
	return "\n".join(lines) if not lines.is_empty() else "%s: not in the data" % path


func _entry(path: String) -> Dictionary:
	for e: Dictionary in unsaved():
		if e["path"] == path:
			return e
	return {}


func _forget(path: String) -> void:
	session["unsaved"] = unsaved().filter(func(e): return e["path"] != path)
