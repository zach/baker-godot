@tool
## The Baker dock: Layers (toggle them, see what each changes), Tweaks (the edited scene's
## bound properties and where their values come from), and Session (what the game tuned,
## to Keep or Discard).
extends VBoxContainer

const Session := preload("res://addons/baker/session.gd")

var model: Session
var _layers: VBoxContainer
var _tweaks: Tree
var _status: Label
var _unsaved: Tree
var _keep_all: Button

enum { BUTTON_KEEP, BUTTON_DISCARD }


func _init(m: Session) -> void:
	model = m
	var tabs := TabContainer.new()
	tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	tabs.tab_changed.connect(func(_tab): refresh())
	add_child(tabs)

	_layers = VBoxContainer.new()
	_layers.name = "Layers"
	tabs.add_child(_layers)

	_tweaks = Tree.new()
	_tweaks.name = "Tweaks"
	_tweaks.columns = 3
	_tweaks.hide_root = true
	_tweaks.column_titles_visible = true
	_tweaks.set_column_title(0, "Property")
	_tweaks.set_column_title(1, "Value")
	_tweaks.set_column_title(2, "From")
	_tweaks.set_column_expand(1, false)
	_tweaks.set_column_custom_minimum_width(1, 64)
	tabs.add_child(_tweaks)

	var session := VBoxContainer.new()
	session.name = "Session"
	tabs.add_child(session)
	_status = Label.new()
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	session.add_child(_status)
	_unsaved = Tree.new()
	_unsaved.columns = 2
	_unsaved.hide_root = true
	_unsaved.column_titles_visible = true
	_unsaved.set_column_title(0, "Path")
	_unsaved.set_column_title(1, "Tuned")
	_unsaved.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_unsaved.set_column_expand(1, false)
	_unsaved.set_column_custom_minimum_width(1, 140)
	_unsaved.button_clicked.connect(_on_button)
	session.add_child(_unsaved)
	var row := HBoxContainer.new()
	session.add_child(row)
	_keep_all = Button.new()
	_keep_all.text = "Keep all"
	_keep_all.pressed.connect(func(): _report(model.keep_all()))
	row.add_child(_keep_all)

	model.changed.connect(refresh)
	refresh()


func refresh() -> void:
	_refresh_layers()
	_refresh_session()
	refresh_tweaks()


func _refresh_layers() -> void:
	for c in _layers.get_children():
		c.queue_free()
	for l: Dictionary in model.layers():
		if l["name"] == "scratch":
			continue
		var box := CheckBox.new()
		box.text = l["name"]
		box.button_pressed = l["enabled"]
		var effect: Array = model.baker.layer_effect(l["name"])
		box.tooltip_text = "\n".join(effect.map(func(e): return "%s = %s" % [e["path"], _show(e["value"])]))
		box.text += "  (%d value%s)" % [effect.size(), "" if effect.size() == 1 else "s"]
		var layer: String = l["name"]
		box.toggled.connect(func(on): model.toggle(layer, on))
		_layers.add_child(box)


func _refresh_session() -> void:
	var unsaved := model.unsaved()
	if model.running:
		_status.text = "Playing: %d value(s) tuned and not kept." % unsaved.size()
	elif model.session.is_empty():
		_status.text = "Play the game and tune bound properties in the remote inspector."
	else:
		_status.text = "Stopped: %d value(s) tuned and not kept. Keep writes each to the line it came from." % unsaved.size()
	_unsaved.clear()
	var root := _unsaved.create_item()
	for e: Dictionary in unsaved:
		var item := _unsaved.create_item(root)
		var where := "%s:%d" % [e["file"], e["line"]] if e.get("file", "") != "" else ""
		var to := "Keep writes it to %s" % where if where != "" else "No layer has it: Keep adds it"
		item.set_text(0, e["path"])
		item.set_tooltip_text(0, "%s\n%s" % [e["path"], to])
		item.set_text(1, "%s → %s" % [_show(e.get("before")), _show(e["value"])])
		item.set_tooltip_text(1, to)
		item.set_metadata(0, e["path"])
		item.add_button(1, get_theme_icon("Save", "EditorIcons"), BUTTON_KEEP, false, "Keep: " + where)
		item.add_button(1, get_theme_icon("Remove", "EditorIcons"), BUTTON_DISCARD, false, "Discard")
	_keep_all.disabled = unsaved.is_empty()


func refresh_tweaks() -> void:
	_tweaks.clear()
	var root := _tweaks.create_item()
	var scene := EditorInterface.get_edited_scene_root() if Engine.is_editor_hint() else null
	if scene == null:
		return
	for binding: BakerBinding in scene.find_children("*", "BakerBinding", true, false):
		var target := binding.get_parent()
		for property: String in binding.bindings:
			var path: String = binding.bindings[property]
			var item := _tweaks.create_item(root)
			item.set_text(0, property)
			item.set_tooltip_text(0, "%s.%s\n%s" % [target.name, property, path])
			item.set_text(1, _show(model.baker.get_value(path, target.get(property))))
			var s: Dictionary = model.baker.source(path)
			item.set_text(2, "%s:%d" % [String(s["file"]).get_file(), s["line"]] if not s.is_empty() else "the scene")
			item.set_tooltip_text(2, "%s, %s:%d" % [s["layer"], s["file"], s["line"]] if not s.is_empty() else "Not in the data: the scene's value stands")


## A value as a person reads it: 120 rather than 120.0.
func _show(v: Variant) -> String:
	if v is float and v == floorf(v) and absf(v) < 1e15:
		return str(int(v))
	return var_to_str(v)


func _on_button(item: TreeItem, _column: int, id: int, _mouse: int) -> void:
	var path: String = item.get_metadata(0)
	if id == BUTTON_KEEP:
		_report(model.keep(path))
	else:
		model.discard(path)


func _report(err: Error) -> void:
	if err != OK:
		push_error("Baker: Keep failed: %s" % model.last_error)
