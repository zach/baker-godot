## Runtime tests: the autoload, BakerBinding, layers, live edits, reload.
##
##   godot --headless --path sample res://tests/runtime_test.tscn
##
## Prints one line per check and quits with the number of failures.
extends Node

const LOCAL := "user://baker/local.dta"

var failures := 0


func check(ok: bool, what: String) -> void:
	print(("ok   " if ok else "FAIL ") + what)
	if not ok:
		failures += 1


func _ready() -> void:
	DirAccess.remove_absolute(LOCAL)
	Baker.reload()
	await run()
	DirAccess.remove_absolute(LOCAL)
	print("%d failure(s)" % failures)
	get_tree().quit(failures)


func run() -> void:
	check(Baker.is_open(), "the project file opens")
	var names := Baker.get_layers().map(func(l): return l["name"])
	check(names == ["default", "local", "agent", "floaty", "scratch"],
		"layers from the project file, bottom to top: %s" % [names])

	# A player with one property the data has and one it lacks.
	var player: Node = load("res://player.tscn").instantiate()
	player.set_meta("unbound", 1.0)
	var binding: BakerBinding = player.get_node("BakerBinding")
	var bindings := binding.bindings.duplicate()
	bindings["air_control"] = "player.air_control"
	bindings["max_fall"] = "player.no_such_value"
	binding.bindings = bindings
	add_child(player)
	check(player.jump_height == 96.0, "a bound property takes the data's value")
	check(player.max_fall == 800.0, "a path the data lacks leaves the scene's value")
	var src: Dictionary = binding.sources()["jump_height"]
	check(src["layer"] == "default" and src["file"] == "data/tuning.dta" and src["line"] == 10,
		"sources() names layer, file and line: %s" % [src])

	# An experiment layer, toggled.
	check(Baker.toggle_layer("floaty") == true, "toggle_layer turns floaty on")
	check(player.jump_time_to_peak == 0.5, "the property follows the experiment")
	check(Baker.source("player.jump.time_to_peak")["layer"] == "floaty", "source() names floaty")
	Baker.toggle_layer("floaty")
	check(player.jump_time_to_peak == 0.38, "and returns when it is off")

	# Reads from GDScript.
	check(Baker.get_value("player.run.speed", 0.0) == 220.0, "get_value reads as the fallback's type")
	check(Baker.get_value("player.run.speed", 0) is int, "an int fallback reads an int")
	check(Baker.get_value("player.nothing", 7.5) == 7.5, "a missing path gives the fallback")
	check(Baker.get_value("player.run", 5) == 5, "a path to arrays gives an int fallback")
	check(Baker.tweak("player.jump.coyote", 0.0) == 0.1, "tweak() reads")

	# A live edit: scratch, unsaved, journaled, explained.
	check(Baker.set_value("player.jump.height", 120.0) == OK, "set_value")
	check(player.jump_height == 120.0, "the bound property follows a live edit")
	check(Baker.unsaved() == PackedStringArray(["player/jump/height"]),
		"unsaved() lists it: %s" % [Baker.unsaved()])
	var journal := Baker.journal()
	check(journal.size() == 1 and journal[0]["before"] == 96 and journal[0]["after"] == 120.0
		and journal[0]["fate"] == "unsaved", "the journal has it: %s" % [journal])
	var chain := Baker.explain("player.jump.height").map(
		func(e): return "%s:%s" % [e["layer"], e["standing"]])
	check(chain == ["code:shadowed", "default:shadowed", "scratch:wins"],
		"explain() gives the chain: %s" % [chain])

	# Kept for oneself: written into the local layer, in user://.
	check(Baker.write_back_to("player.jump.height", "local") == OK, "write_back_to local")
	check(FileAccess.get_file_as_string(LOCAL).contains("(height 120"),
		"local.dta holds the value: %s" % FileAccess.get_file_as_string(LOCAL))
	check(Baker.source("player.jump.height")["layer"] == "local", "and local supplies it")
	check(Baker.journal()[0]["fate"] == "written", "the journal says written")
	Baker.set_value("player.run.speed", 230.0)
	check(Baker.write_back_to("player.run.speed", "default") != OK,
		"the game never writes into res://")
	check(FileAccess.get_file_as_string("res://data/tuning.dta").contains("(speed 220)"),
		"tuning.dta is unchanged")
	Baker.clear_scratch()

	# An edit on disk is seen within a few frames.
	var f := FileAccess.open(LOCAL, FileAccess.WRITE)
	f.store_string("(player (jump (height 110)))\n")
	f.close()
	for i in 10:
		await get_tree().process_frame
		if player.jump_height == 110.0:
			break
	check(player.jump_height == 110.0, "an edit on disk reloads within frames")
	var stats := Baker.loop_stats()
	check(stats["reloads"] >= 1, "loop_stats counts reloads: %s" % [stats])

	# An edit to an included file is seen too: the watcher follows what the stack read.
	f = FileAccess.open("user://baker/inc.dta", FileAccess.WRITE)
	f.store_string("(player (jump (height 105)))\n")
	f.close()
	f = FileAccess.open(LOCAL, FileAccess.WRITE)
	f.store_string("#include \"inc.dta\"\n")
	f.close()
	for i in 10:
		await get_tree().process_frame
		if player.jump_height == 105.0:
			break
	check(player.jump_height == 105.0, "local.dta includes inc.dta: %s" % player.jump_height)
	check(Baker.source("player.jump.height")["file"] == "user://baker/inc.dta",
		"source() names the include as written: %s" % [Baker.source("player.jump.height")])
	f = FileAccess.open("user://baker/inc.dta", FileAccess.WRITE)
	f.store_string("(player (jump (height 104)))\n")
	f.close()
	for i in 10:
		await get_tree().process_frame
		if player.jump_height == 104.0:
			break
	check(player.jump_height == 104.0, "an edit to the included file reloads within frames")
	DirAccess.remove_absolute("user://baker/inc.dta")

	# A bound node gone: its tweaks go with it.
	player.queue_free()
	await get_tree().process_frame
	check(Baker.set_value("player.jump.height", 130.0) == OK, "edits after the node is freed")
	Baker.clear_scratch()
