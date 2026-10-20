## The sample's simulation commands, as the stacks' game binaries have them:
##
##   godot --headless --path sample -- --baker sim [--scenario NAME]... [--seeds A-B]
##                                                 [--out DIR] [--json]
##   godot --headless --path sample -- --baker compare BEFORE_DIR AFTER_DIR
##
## sim runs each scenario (every one in res://scenarios/ unless named) once per seed, each
## run in a fresh instance of the main scene, and judges each run by the scenario's
## objectives. It prints a line per run, or with --json the results as JSON (schema
## stacks.sim/1.1, with one addition: "host", the layers the runs used and whether the build
## ships); --out DIR writes that JSON to DIR/results.json. Exit status: 0 if every run
## passed, 1 if an objective failed, 2 if something could not run. Nothing in a run reads
## the clock or draws a random number: the same data, scenario and seed give the same
## result. Runs leave out the `local` layer, so they do not depend on whose machine they
## ran on.
##
## compare reads two sim --out directories and says, per objective, whether it broke (passed
## on a scenario and seed, now fails), was fixed, or stayed the same, as JSON (schema
## stacks.compare/1.1). Exit status: 1 if one broke, 2 if a directory has no results.
extends Node

var command := ""
var names := PackedStringArray()
var seed_first := 1
var seed_last := 1
var out_dir := ""
var json := false
var dirs := PackedStringArray()  # compare's
var error := ""
var queue: Array = []  # [scenario, seed] still to run
var runs: Array = []


## The command line's sim or compare after --baker; null when it asks for neither.
static func from_arguments() -> Node:
	var args := OS.get_cmdline_user_args()
	var at := args.find("--baker")
	if at < 0 or at + 1 >= args.size():
		return null
	var runner: Node = load("res://scenarios/runner.gd").new()
	runner.name = "BakerRunner"
	runner.command = args[at + 1]
	if runner.command not in ["sim", "compare"]:
		if not BakerRuntime.is_shipping():
			runner.free()  # a tool command: Baker answers it
			return null
		runner.error = "no tool commands in a shipping build"
		return runner
	var i := at + 2
	while i < args.size():
		var a := args[i]
		var value := args[i + 1] if i + 1 < args.size() else ""
		if a == "--scenario" and value != "":
			runner.names.append(value)
			i += 1
		elif a == "--seeds" and value != "":
			var parts := value.split("-")
			runner.seed_first = parts[0].to_int()
			runner.seed_last = parts[-1].to_int()
			if not parts[0].is_valid_int() or not parts[-1].is_valid_int() \
					or runner.seed_last < runner.seed_first:
				runner.error = "--seeds A-B needs whole numbers, A <= B"
			i += 1
		elif a == "--out" and value != "":
			runner.out_dir = value
			i += 1
		elif a == "--json":
			runner.json = true
		elif not a.begins_with("--"):
			runner.dirs.append(a)
		else:
			runner.error = "unknown option %s" % a
		i += 1
	if runner.command == "compare" and runner.dirs.size() != 2:
		runner.error = "compare needs two result directories: compare BEFORE AFTER"
	if runner.command == "sim" and runner.error == "":
		if runner.names.is_empty():
			runner.names = _all_scenarios()
		for name in runner.names:
			for seed in range(runner.seed_first, runner.seed_last + 1):
				runner.queue.append([name, seed])
	return runner


static func _all_scenarios() -> PackedStringArray:
	var found := PackedStringArray()
	for file in DirAccess.get_files_at("res://scenarios"):
		var name := file.trim_suffix(".remap").get_basename()
		if file.get_extension() in ["gd", "remap"] and name not in ["scenario", "runner"] \
				and not file.ends_with(".uid") and name not in found:
			found.append(name)
	found.sort()
	return found


## Called by each fresh main scene: runs the next scenario in it, or answers compare.
func attach(main: Node) -> void:
	if error != "":
		return _quit(2, error)
	if command == "compare":
		return _compare()
	var next: Array = queue[runs.size()]
	var path := "res://scenarios/%s.gd" % next[0]
	if not ResourceLoader.exists(path) or next[0] in ["scenario", "runner"]:
		_record({"scenario": next[0], "seed": next[1], "steps": 0, "passed": false,
			"errors": ["no scenario %s in res://scenarios/" % next[0]], "objectives": []})
		return
	var scenario: Node = load(path).new()
	scenario.seed = next[1]
	scenario.finished.connect(_record, CONNECT_ONE_SHOT)
	main.add_child(scenario)


func _record(run: Dictionary) -> void:
	runs.append(run)
	if not json:
		var line := "%s seed %d: %s" % [run["scenario"], run["seed"],
			"passed" if run["passed"] else "FAILED"]
		for o: Dictionary in run["objectives"]:
			line += "  %s %s%s" % [o["name"], o["value"], "" if o["pass"] else " (fail)"]
		print(line)
		for e: String in run["errors"]:
			print("  error: %s" % e)
	if runs.size() < queue.size():
		get_tree().reload_current_scene.call_deferred()
	else:
		_finish()


func _finish() -> void:
	var passed := runs.all(func(r): return r["passed"])
	var sorted_runs := runs.duplicate()
	sorted_runs.sort_custom(func(a, b):
		return a["scenario"] < b["scenario"] if a["scenario"] != b["scenario"] else a["seed"] < b["seed"])
	var tallies := _tallies(runs)
	var summary := []
	for name: String in _sorted_keys(tallies):
		summary.append({"objective": name, "tally": _tally_json(tallies[name])})
	var layers := Baker.get_layers().filter(func(l): return l["enabled"] and l["name"] != "scratch")
	var text := JSON.stringify({
		"schema": "stacks.sim/1.1",
		"status": "passed" if passed else "failed",
		"runs": sorted_runs,
		"medians": [],
		"summary": summary,
		"host": {
			"layers": layers.map(func(l): return l["name"]),
			"shipping": BakerRuntime.is_shipping(),
		},
	}, "  ")
	if json:
		print(text)
	else:
		print("%d run(s): %s" % [runs.size(), "passed" if passed else "FAILED"])
	if out_dir != "":
		DirAccess.make_dir_recursive_absolute(out_dir)
		var f := FileAccess.open(out_dir.path_join("results.json"), FileAccess.WRITE)
		if f == null:
			return _quit(2, "cannot write %s" % out_dir.path_join("results.json"))
		f.store_string(text + "\n")
		f.close()
	get_tree().quit(0 if passed else 1)


func _compare() -> void:
	var sides := []
	for dir in dirs:
		var path := dir.path_join("results.json")
		var data = JSON.parse_string(FileAccess.get_file_as_string(path)) \
			if FileAccess.file_exists(path) else null
		if not (data is Dictionary and data.get("schema", "").begins_with("stacks.sim/")):
			return _quit(2, "no sim results in %s" % dir)
		sides.append(data["runs"])
	var before := _tallies(sides[0])
	var after := _tallies(sides[1])
	var names := _sorted_keys(before)
	for n in _sorted_keys(after):
		if n not in names:
			names.append(n)
	names.sort()
	var objectives := []
	var broke := 0
	var fixed := 0
	for name: String in names:
		var entry := {"name": name, "moved": "same",
			"before": _tally_json(before[name]) if before.has(name) else null,
			"after": _tally_json(after[name]) if after.has(name) else null,
			"median_change": null, "changed": []}
		if not before.has(name):
			entry["moved"] = "new"
		elif not after.has(name):
			entry["moved"] = "gone"
		else:
			var was: Dictionary = before[name]["by_run"]
			var now: Dictionary = after[name]["by_run"]
			var any_broke := false
			var any_fixed := false
			for key in _sorted_keys(now):
				if was.has(key) and was[key] != now[key]:
					if now[key]:
						any_fixed = true
					else:
						any_broke = true
					entry["changed"].append({"scenario": key.get_slice("#", 0),
						"seed": key.get_slice("#", 1).to_int(), "before": was[key], "after": now[key]})
			entry["moved"] = "broke" if any_broke else "fixed" if any_fixed else "same"
			entry["median_change"] = _median(after[name]["values"]) - _median(before[name]["values"])
		broke += 1 if entry["moved"] == "broke" else 0
		fixed += 1 if entry["moved"] == "fixed" else 0
		objectives.append(entry)
	print(JSON.stringify({"schema": "stacks.compare/1.1", "broke": broke, "fixed": fixed,
		"objectives": objectives}, "  "))
	get_tree().quit(1 if broke > 0 else 0)


## Per objective: passes and failures, its values, and its verdict by scenario#seed.
static func _tallies(run_list: Array) -> Dictionary:
	var by := {}
	for r: Dictionary in run_list:
		for o: Dictionary in r["objectives"]:
			var t: Dictionary = by.get_or_add(o["name"],
				{"passed": 0, "failed": 0, "values": [], "by_run": {}})
			t["passed" if o["pass"] else "failed"] += 1
			t["values"].append(o["value"])
			t["by_run"]["%s#%d" % [r["scenario"], r["seed"]]] = o["pass"]
	return by


static func _tally_json(t: Dictionary) -> Dictionary:
	var values: Array = t["values"].duplicate()
	values.sort()
	return {"passed": t["passed"], "failed": t["failed"], "min": values.front(),
		"median": _median(values), "max": values.back()}


static func _median(values: Array) -> float:
	var v := values.duplicate()
	v.sort()
	return v[v.size() / 2]


static func _sorted_keys(d: Dictionary) -> Array:
	var keys := d.keys()
	keys.sort()
	return keys


func _quit(status: int, message: String) -> void:
	printerr("baker: %s" % message)
	get_tree().quit(status)
