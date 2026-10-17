@tool
## The editor side of Baker: the dock, the inspector's layer chains, and the running
## game's session over the debugger channel.
extends EditorPlugin

const Session := preload("res://addons/baker/session.gd")
const Debugger := preload("res://addons/baker/debugger.gd")
const Dock := preload("res://addons/baker/dock.gd")
const Inspector := preload("res://addons/baker/inspector.gd")

var model: Session
var debugger: Debugger
var dock: Dock
var inspector: Inspector


func _enter_tree() -> void:
	debugger = Debugger.new()
	add_debugger_plugin(debugger)
	model = Session.new(debugger)
	dock = Dock.new(model)
	dock.name = "Baker"
	add_control_to_dock(DOCK_SLOT_RIGHT_BL, dock)
	inspector = Inspector.new(model)
	add_inspector_plugin(inspector)
	scene_changed.connect(func(_root): dock.refresh_tweaks())
	if OS.get_environment("BAKER_E2E") == "1":
		var e2e: Node = load("res://tests/e2e.gd").new()
		e2e.set("model", model)
		e2e.set("debugger", debugger)
		e2e.set("dock", dock)
		add_child(e2e)


func _exit_tree() -> void:
	remove_inspector_plugin(inspector)
	remove_control_from_docks(dock)
	dock.queue_free()
	remove_debugger_plugin(debugger)
	model.close()


func _enable_plugin() -> void:
	add_autoload_singleton("Baker", "res://addons/baker/baker.gd")


func _disable_plugin() -> void:
	remove_autoload_singleton("Baker")
