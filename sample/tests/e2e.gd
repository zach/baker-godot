@tool
## The demonstration, end to end, in a headless editor (tests/e2e.sh runs it on a copy of
## the sample):
##   1. play;
##   2. tune the jump height as the remote inspector does (its scene:set_object_property);
##   3. toggle the floaty experiment while playing;
##   4. stop;
##   5. the session lists the change, and Keep writes a one-line diff to tuning.dta.
## Prints one line per check and quits the editor with the number of failures.
extends Node

const TUNING := "res://data/tuning.dta"
const PATH := "player/jump/height"

var model
var debugger
var dock
var failures := 0
## BAKER_SHOTS=<dir>: screenshots of the editor at each step, for a person to look at.
var shots := OS.get_environment("BAKER_SHOTS")


func check(ok: bool, what: String) -> void:
	print(("ok   " if ok else "FAIL ") + what)
	if not ok:
		failures += 1


func _ready() -> void:
	run.call_deferred()


func run() -> void:
	await wait(func(): return false, 2.0)
	var original := FileAccess.get_file_as_string(TUNING)
	if shots != "":
		EditorInterface.open_scene_from_path("res://main.tscn")
		await wait(func(): return false, 1.0)
		var scene := EditorInterface.get_edited_scene_root()
		EditorInterface.edit_node(scene.get_node("Player"))
		await shot("1-inspector-layers", 0)
		await shot("1-tweaks", 1)

	EditorInterface.play_main_scene()
	check(await wait(func(): return model.running and _binding("jump_height") != {}, 30.0),
		"1. the game runs and sends its bindings")
	if failures:
		return finish()

	var b := _binding("jump_height")
	debugger.send_raw("scene:set_object_property", [b["object_id"], "jump_height", 120.0])
	check(await wait(func(): return _unsaved_value(PATH) == 120.0, 10.0),
		"2. a remote-inspector edit reaches the session: %s" % [model.unsaved()])
	var entry: Dictionary = model.unsaved()[0] if not model.unsaved().is_empty() else {}
	check(entry.get("before") == 96 and entry.get("file") == "data/tuning.dta",
		"   it was 96, from data/tuning.dta: %s" % [entry])

	await shot("2-playing-tuned", 2)
	model.toggle("floaty", true)
	check(await wait(func(): return _layer_on("floaty"), 10.0), "3. floaty toggled on in the game")
	model.toggle("floaty", false)
	check(await wait(func(): return not _layer_on("floaty"), 10.0), "   and off again")

	EditorInterface.stop_playing_scene()
	check(await wait(func(): return not model.running, 10.0), "4. the game stops")
	check(model.unsaved().size() == 1, "   the session still lists the change")
	await shot("3-stopped", 2)

	check(model.keep(PATH) == OK, "5. Keep: %s" % model.last_error)
	var kept := FileAccess.get_file_as_string(TUNING)
	var a := original.split("\n")
	var k := kept.split("\n")
	var changed := []
	for i in min(a.size(), k.size()):
		if a[i] != k[i]:
			changed.append([a[i], k[i]])
	check(a.size() == k.size() and changed.size() == 1, "   one line changed: %s" % [changed])
	check(changed.size() == 1 and changed[0][1] == changed[0][0].replace("(height 96)", "(height 120)"),
		"   the value's tokens only, its comment intact")
	check(model.unsaved().is_empty(), "   and the session no longer lists it")
	await shot("4-kept", 0)
	finish()


## Saves the editor's window as <name>.png, with the dock's tab `tab` showing.
func shot(name: String, tab := -1) -> void:
	if shots == "":
		return
	if tab >= 0:
		dock.get_child(0).current_tab = tab
	await wait(func(): return false, 0.5)
	var image := get_viewport().get_texture().get_image()
	image.save_png(shots.path_join(name + ".png"))


func finish() -> void:
	print("%d failure(s)" % failures)
	get_tree().quit(failures)


func wait(condition: Callable, seconds: float) -> bool:
	var end := Time.get_ticks_msec() + int(seconds * 1000)
	while Time.get_ticks_msec() < end:
		if condition.call():
			return true
		await get_tree().process_frame
	return condition.call()


func _binding(property: String) -> Dictionary:
	for b: Dictionary in model.session.get("bindings", []):
		if b["property"] == property:
			return b
	return {}


func _unsaved_value(path: String) -> Variant:
	for e: Dictionary in model.unsaved():
		if e["path"] == path:
			return e["value"]
	return null


func _layer_on(layer: String) -> bool:
	for l: Dictionary in model.session.get("layers", []):
		if l["name"] == layer:
			return l["enabled"]
	return false
