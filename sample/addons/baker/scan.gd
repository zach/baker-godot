@tool
## The bindings in every packed scene of the project, read from the scenes' files without
## instantiating them: each BakerBinding's bindings, with its node's values and export hints
## from the scene and its script. The dock lists them; a tool command binds them
## (bind_unloaded), so `reference` and `check` know the tunables of scenes no one has open.
extends RefCounted

const Scanned := preload("res://addons/baker/scanned.gd")
const SKIP := ["res://addons", "res://.godot"]


## [{scene, node, property, path, value, info}]: every bound property of every scene.
static func scan(root := "res://") -> Array:
	var out := []
	for file: String in _scenes(root):
		_scan_scene(file, out)
	return out


## Binds each scanned node's properties to a stand-in holding its values and hints, so the
## paths are tweaks as if its scene were loaded. The stand-ins live while their bindings do.
static func bind_unloaded(runtime: Node) -> int:
	var by_node := {}
	for e: Dictionary in scan():
		var key: String = "%s:%s" % [e["scene"], e["node"]]
		if not by_node.has(key):
			by_node[key] = []
		by_node[key].append(e)
	for key: String in by_node:
		var holder := Scanned.new()
		var bindings := {}
		for e: Dictionary in by_node[key]:
			holder.add(e["property"], e["value"], e["info"])
			bindings[e["property"]] = e["path"]
		runtime.bind_resource(holder, bindings)
	return by_node.size()


static func _scenes(dir: String) -> Array:
	var out := []
	if SKIP.has(dir.trim_suffix("/")):
		return out
	for file: String in DirAccess.get_files_at(dir):
		file = file.trim_suffix(".remap")  # an export's converted scene
		if file.ends_with(".tscn") or file.ends_with(".scn"):
			out.append(dir.path_join(file))
	for sub: String in DirAccess.get_directories_at(dir):
		out.append_array(_scenes(dir.path_join(sub)))
	return out


static func _scan_scene(file: String, out: Array) -> void:
	var packed := ResourceLoader.load(file) as PackedScene
	if packed == null:
		return
	var state := packed.get_state()
	for i in state.get_node_count():
		if state.get_node_type(i) != "BakerBinding":
			continue
		var bindings: Dictionary = _properties(state, i).get("bindings", {})
		var parent_path := state.get_node_path(i, true)
		var parent := -1
		for k in state.get_node_count():
			if state.get_node_path(k) == parent_path:
				parent = k
		if parent < 0:
			continue
		var props := _properties(state, parent)
		var script: Script = props.get("script")
		if script == null:
			continue  # an instance of another scene, scanned in its own file
		var infos := {}
		for info: Dictionary in script.get_script_property_list():
			infos[info["name"]] = info
		for property: String in bindings:
			if not infos.has(property):
				continue
			var path := _resolve(String(bindings[property]), props, script, file)
			if path.is_empty():
				continue
			out.append({"scene": file, "node": String(parent_path), "property": property,
				"path": path, "info": infos[property],
				"value": props.get(property, script.get_property_default_value(property))})


static func _properties(state: SceneState, node: int) -> Dictionary:
	var out := {}
	for j in state.get_node_property_count(node):
		out[state.get_node_property_name(node, j)] = state.get_node_property_value(node, j)
	return out


## The path with each {key} the node's own value, as a BakerBinding fills it; {plugin}, if the
## node has none, the folder of its scene. Empty if a key has no value.
static func _resolve(path: String, props: Dictionary, script: Script, file: String) -> String:
	var out := path
	while out.contains("{"):
		var open := out.find("{")
		var close := out.find("}", open)
		if close < 0:
			return ""
		var key := out.substr(open + 1, close - open - 1)
		var value: Variant = props.get(key, script.get_property_default_value(key))
		if value == null and key == "plugin":
			value = file.get_base_dir().get_file()
		if value == null or str(value).is_empty():
			return ""
		out = out.substr(0, open) + str(value) + out.substr(close + 1)
	return out
