## Runtime tests: the autoload, BakerBinding, layers, live edits, reload.
##
##   godot --headless --path sample res://tests/runtime_test.tscn
##
## Prints one line per check and quits with the number of failures.
extends Node

const LOCAL := "user://baker/local.dta"

var failures := 0
var completed := false  # run() reached its end: a script error stops it short


func check(ok: bool, what: String) -> void:
	print(("ok   " if ok else "FAIL ") + what)
	if not ok:
		failures += 1


func _ready() -> void:
	DirAccess.remove_absolute(LOCAL)
	Baker.reload()
	await run()
	check(completed, "the tests ran to their end, with no script error")
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

	# An edit on disk is seen within a few frames; within 100 ms for a file whose directory
	# did not exist when the watch began (a first run), which the watcher checks by its stamp.
	var f := FileAccess.open(LOCAL, FileAccess.WRITE)
	f.store_string("(player (jump (height 110)))\n")
	f.close()
	await until(func(): return player.jump_height == 110.0)
	check(player.jump_height == 110.0, "an edit on disk reloads")
	var stats := Baker.loop_stats()
	check(stats["reloads"] >= 1, "loop_stats counts reloads: %s" % [stats])

	# An edit to an included file is seen too: the watcher follows what the stack read.
	f = FileAccess.open("user://baker/inc.dta", FileAccess.WRITE)
	f.store_string("(player (jump (height 105)))\n")
	f.close()
	f = FileAccess.open(LOCAL, FileAccess.WRITE)
	f.store_string("#include \"inc.dta\"\n")
	f.close()
	await until(func(): return player.jump_height == 105.0)
	check(player.jump_height == 105.0, "local.dta includes inc.dta: %s" % player.jump_height)
	check(Baker.source("player.jump.height")["file"] == "user://baker/inc.dta",
		"source() names the include as written: %s" % [Baker.source("player.jump.height")])
	f = FileAccess.open("user://baker/inc.dta", FileAccess.WRITE)
	f.store_string("(player (jump (height 104)))\n")
	f.close()
	await until(func(): return player.jump_height == 104.0)
	check(player.jump_height == 104.0, "an edit to the included file reloads")
	f = FileAccess.open(LOCAL, FileAccess.WRITE)
	f.store_string("")
	f.close()
	Baker.reload()
	DirAccess.remove_absolute("user://baker/inc.dta")

	# A property set from outside, as the remote inspector sets it, becomes a live edit.
	player.run_speed = 250.0
	check(binding.capture() == 1, "capture() makes a live edit of a property changed outside")
	check(Baker.get_value("player.run.speed", 0.0) == 250.0 and "player/run/speed" in Baker.unsaved(),
		"it is in scratch, unsaved: %s" % [Baker.unsaved()])
	check(binding.capture() == 0, "and is captured once")
	var session := Baker.session()
	var tuned: Array = session["unsaved"].filter(func(e): return e["path"] == "player/run/speed")
	check(tuned.size() == 1 and tuned[0]["value"] == 250.0 and tuned[0]["before"] == 220
		and tuned[0]["file"] == "data/tuning.dta" and tuned[0]["line"] == 5,
		"session() gives its value, what it was, and where it came from: %s" % [tuned])
	var bound: Array = session["bindings"].filter(func(b): return b["property"] == "jump_height")
	check(bound.size() == 1 and bound[0]["object_id"] == player.get_instance_id()
		and bound[0]["path"] == "player.jump.height",
		"session() lists the bindings with their objects: %s" % [bound])
	check(session["layers"].size() == 5, "session() gives the layers")
	Baker.clear_scratch("player/run/speed")
	check(player.run_speed == 220.0, "discarded, the property returns to the data's value")
	check(binding.capture() == 0, "which is not taken for an edit")

	# Bindings at scale: forty scouts and twenty tanks, one binding each through the path
	# units.{kind}.max_hp, make two tweaks, and an edit of a kind reaches every unit of it.
	var tweaks_before := BakerBinding.shared_tweak_count()
	var units: Array[Node] = []
	for i in 60:
		var u: Node = preload("res://tests/unit_probe.gd").new()
		u.kind = &"scout" if i < 40 else &"tank"
		u.max_hp = 10.0 + i  # each its own, while the data has no units
		add_child(u)
		units.append(u)
	var scouts := units.slice(0, 40)
	var tanks := units.slice(40)
	check(BakerBinding.shared_tweak_count() == tweaks_before + 2,
		"sixty units of two kinds bind two tweaks: %d" % [BakerBinding.shared_tweak_count() - tweaks_before])
	check(units.all(func(u): return u.max_hp == 10.0 + units.find(u)),
		"the data has no units: each keeps its own value")
	check(Baker.set_value("units.scout.max_hp", 50.0) == OK
		and scouts.all(func(u): return u.max_hp == 50.0 and u.hp == 50.0)
		and tanks.all(func(u): return u.max_hp == 10.0 + units.find(u)),
		"an edit of units.scout.max_hp reaches every scout, and no tank")
	check(scouts.all(func(u): return u.changes == 1),
		"property_changed tells each scout once, which re-derives its hit points")
	scouts[3].max_hp = 55.0
	check(scouts[3].get_node("BakerBinding").capture() == 1
		and scouts.all(func(u): return u.max_hp == 55.0),
		"one scout changed from outside is an edit of the kind: every scout follows")
	Baker.clear_scratch("units/scout/max_hp")
	check(units.all(func(u): return u.max_hp == 10.0 + units.find(u)),
		"the data without the path again: each unit has its own value back")
	var stray: Node = preload("res://tests/unit_probe.gd").new()
	stray.kind = &""
	add_child(stray)
	check(stray.get_node("BakerBinding").sources().is_empty(),
		"a path whose key the node lacks is not bound")
	stray.free()
	for u in units:
		u.free()
	check(BakerBinding.shared_tweak_count() == tweaks_before,
		"the units gone, their tweaks go: %d left" % [BakerBinding.shared_tweak_count()])

	# A bound node gone: its tweaks go with it.
	player.queue_free()
	await get_tree().process_frame
	check(Baker.set_value("player.jump.height", 130.0) == OK, "edits after the node is freed")
	Baker.clear_scratch()
	completed = true


## Waits a frame at a time, up to a second, until `condition` holds.
func until(condition: Callable) -> void:
	var end := Time.get_ticks_msec() + 1000
	while not condition.call() and Time.get_ticks_msec() < end:
		await get_tree().process_frame
