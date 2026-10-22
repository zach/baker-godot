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

	# Tables, read whole: a keyed table is a Dictionary of records, and a list of records
	# an Array.
	var units_table: Dictionary = Baker.get_value("units", {})
	var kinds := units_table.keys()
	kinds.sort()
	check(kinds == ["artillery", "heli", "infantry", "tank"],
		"get_value(\"units\", {}) is the unit table, by kind: %s" % [kinds])
	check(units_table["tank"] == {"hp": 10, "move": 6, "attack": 10, "armor": 2, "price": 50},
		"a record is a Dictionary of its fields: %s" % [units_table["tank"]])
	check(units_table["artillery"]["reach"] == [2, 3] and units_table["heli"]["flies"] == 1,
		"a field of several values is an Array, a flag a number")
	check(Baker.get_value("buildings.hq", {}) == {"income": 20, "capture": 1000},
		"one record by its path")
	check(SkirmishCatalog.opening() == [&"infantry", &"infantry", &"tank"],
		"a list of records, all of one tag, is an Array of them: %s" % [SkirmishCatalog.opening()])
	check(SkirmishCatalog.affordable(50) == ["infantry", "tank"],
		"the production menu reads the table whole: %s" % [SkirmishCatalog.affordable(50)])
	check(Baker.get_value("units.tank.hp", {}) == {}, "a value that is not a table gives the fallback")
	check(Baker.set_value("units.tank", {"price": 45}) == OK
		and Baker.get_value("units.tank", {}) == {"hp": 10, "move": 6, "attack": 10, "armor": 2, "price": 45},
		"a Dictionary set live sets the fields it names, and the record keeps the rest: %s"
			% [Baker.get_value("units.tank", {})])
	check(Baker.unsaved() == PackedStringArray(["units/tank/price"]),
		"each field a live edit of its own: %s" % [Baker.unsaved()])
	check(Baker.set_value("probe.rec", {"a": [10, [&"range", 0, 20]], "b": {"c": 1}}) == OK
		and Baker.get_value("probe.rec", {}) == {"a": 10, "b": {"c": 1}},
		"metadata after a value is left out: %s" % [Baker.get_value("probe.rec", {})])
	Baker.clear_scratch()

	# Layers the game finds after it starts: a content pack merges into the unit table, and
	# removing it leaves no trace.
	var pack := FileAccess.open("user://baker/pack.dta", FileAccess.WRITE)
	pack.store_string("(units (scout (hp 6) (move 8) (attack 4) (armor 0) (price 30)) (tank (price 40)))\n")
	pack.close()
	check(Baker.push_layer("pack", "user://baker/pack.dta", {"below": "local"}) == OK,
		"push_layer: a content pack below local")
	check(Baker.get_layers().map(func(l): return l["name"])
		== ["default", "pack", "local", "agent", "floaty", "scratch"], "in its place in the stack")
	check(SkirmishCatalog.units().has("scout") and SkirmishCatalog.units()["tank"]["price"] == 40,
		"its records merge into the table: a new unit, a retuned price")
	pack = FileAccess.open("user://baker/pack.dta", FileAccess.WRITE)
	pack.store_string("(units (scout (hp 6) (move 8) (attack 4) (armor 0) (price 30)) (tank (price 35)))\n")
	pack.close()
	await until(func(): return SkirmishCatalog.units()["tank"]["price"] == 35)
	check(SkirmishCatalog.units()["tank"]["price"] == 35, "an edit of the pack's file reloads")
	check(Baker.remove_layer("pack") == OK and not SkirmishCatalog.units().has("scout")
		and SkirmishCatalog.units()["tank"]["price"] == 50, "remove_layer: the table as it was")
	DirAccess.remove_absolute("user://baker/pack.dta")
	check(Baker.push_layer_text("mod", "(units (tank (hp 12)))") == OK
		and SkirmishCatalog.units()["tank"]["hp"] == 12
		and Baker.source("units.tank.hp")["layer"] == "mod",
		"push_layer_text: a layer from received text, named by its source")
	check(Baker.push_layer_text("sneaky", "#include \"skirmish.dta\"\n") != OK
		and not Baker.get_layers().any(func(l): return l["name"] == "sneaky"),
		"received text that includes a file is refused")
	check(Baker.remove_layer("mod") == OK and SkirmishCatalog.units()["tank"]["hp"] == 10,
		"and removed")

	# A Resource's properties, bound as a node's: one path can feed a script's value and a
	# shader's parameter.
	var tweaks_at_start := BakerBinding.shared_tweak_count()
	var stat_res: Resource = preload("res://tests/stats_probe.gd").new()
	var material := ShaderMaterial.new()
	material.shader = Shader.new()
	material.shader.code = "shader_type canvas_item; uniform float glow = 1.0;"
	var stats_binding: BakerBinding = Baker.bind_resource(stat_res,
		{"max_hp": "units.{kind}.hp", "glow": "fx.glow"})
	Baker.bind_resource(material, {"shader_parameter/glow": "fx.glow"})
	check(stats_binding != null and stat_res.max_hp == 10.0,
		"bind_resource: a Resource's property reads its record, by its own key: %s" % [stat_res.max_hp])
	check(Baker.set_value("fx.glow", 2.5) == OK and stat_res.glow == 2.5
		and material.get_shader_parameter("glow") == 2.5,
		"one path feeds the Resource and the shader parameter: %s, %s"
			% [stat_res.glow, material.get_shader_parameter("glow")])
	Baker.clear_scratch()
	Baker.unbind_resource(stat_res)
	Baker.unbind_resource(material)
	await get_tree().process_frame
	check(BakerBinding.shared_tweak_count() == tweaks_at_start, "unbind_resource: their tweaks go")

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

	# A reload lands at the frame's start: a node processed before every other sees it in the
	# same frame as one processed after.
	var early: Node = load("res://tests/frame_probe.gd").new()
	var late: Node = load("res://tests/frame_probe.gd").new()
	early.process_priority = -1000
	late.process_priority = 1000
	for probe in [early, late]:
		probe.player = player
		add_child(probe)
	for height in [113.0, 114.0, 115.0]:
		f = FileAccess.open(LOCAL, FileAccess.WRITE)
		f.store_string("(player (jump (height %d)))\n" % height)
		f.close()
		await until(func(): return player.jump_height == height)
	await get_tree().process_frame
	var torn := []
	for frame in early.seen:
		if late.seen.has(frame) and early.seen[frame] != late.seen[frame]:
			torn.append([frame, early.seen[frame], late.seen[frame]])
	check(player.jump_height == 115.0 and torn.is_empty(),
		"no frame sees part of a reload: the first node and the last read alike %s" % [torn])
	early.free()
	late.free()

	# A match in lockstep: the data's fingerprint, and a lock that holds every change for the
	# turn's boundary.
	var print0 := Baker.fingerprint()
	check(print0.length() == 16 and print0 == Baker.fingerprint(),
		"fingerprint(): sixteen hex digits, the same for the same data: %s" % print0)
	Baker.set_value("player.jump.height", 111.0)
	check(Baker.fingerprint() != print0, "a live edit moves it")
	Baker.clear_scratch()
	check(Baker.fingerprint() == print0, "and it returns with the data")
	check(Baker.lock() == print0 and Baker.is_locked(), "lock() returns it")
	var peak: float = Baker.get_value("player.jump.time_to_peak", 0.0)
	check(Baker.toggle_layer("floaty"), "a toggle during a match says the state it will have")
	check(Baker.set_value("player.jump.height", 108.0) == OK, "a live edit is taken")
	f = FileAccess.open(LOCAL, FileAccess.WRITE)
	f.store_string("(player (run (speed 230)))\n")
	f.close()
	for i in 10:
		await get_tree().process_frame
	check(Baker.get_value("player.jump.time_to_peak", 0.0) == peak and player.jump_height == 115.0
		and Baker.get_value("player.run.speed", 0) == 220 and Baker.fingerprint() == print0,
		"and with a file saved, all wait for the turn's boundary: the data is as locked")
	check(Baker.held_count() == 2, "held_count(): the toggle and the edit")
	var print1 := Baker.apply_held()
	check(Baker.get_value("player.jump.time_to_peak", 0.0) == 0.5 and player.jump_height == 108.0
		and Baker.get_value("player.run.speed", 0) == 230,
		"apply_held(): all of it at once, the saved file too")
	check(print1 != print0 and print1 == Baker.fingerprint() and Baker.is_locked(),
		"it returns the new fingerprint, and the data stays locked")
	Baker.toggle_layer("floaty")
	Baker.unlock()
	check(not Baker.is_locked() and Baker.held_count() == 0
		and not Baker.get_layers()[3]["enabled"], "unlock() applies what waited")
	Baker.clear_scratch()
	f = FileAccess.open(LOCAL, FileAccess.WRITE)
	f.store_string("")
	f.close()
	Baker.reload()

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
