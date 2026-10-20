## Runs scenarios without a person and reports them as the stacks' sim command does:
##
##   godot --headless --path sample -- --scenario jump --scenario reach [--out DIR]
##
## Each scenario runs in a fresh instance of the main scene. The results print as one JSON
## object, schema stacks.sim/1.1 (status, runs, medians, summary), with one addition: "host",
## the layers the runs used and whether the build ships. --out DIR writes it to
## DIR/results.json. Quits with 0 if every run passed, 1 if not. The game draws no random
## numbers, so each scenario runs once, as seed 1. Scenarios run without the `local` layer,
## so their results do not depend on whose machine they ran on.
extends Node

var names: PackedStringArray
var out_dir := ""
var runs: Array = []


## The scenarios and --out DIR the command line asks for; none: not a scenario run.
static func from_arguments() -> Node:
	var args := OS.get_cmdline_user_args()
	var runner: Node = load("res://scenarios/runner.gd").new()
	for i in args.size():
		if args[i].begins_with("--scenario="):
			runner.names.append(args[i].get_slice("=", 1))
		elif args[i] == "--scenario" and i + 1 < args.size():
			runner.names.append(args[i + 1])
		elif args[i] == "--out" and i + 1 < args.size():
			runner.out_dir = args[i + 1]
	if runner.names.is_empty():
		runner.free()
		return null
	runner.name = "ScenarioRunner"
	return runner


## Called by each fresh main scene: runs the next scenario in it.
func attach(main: Node) -> void:
	var name := names[runs.size()]
	var path := "res://scenarios/%s.gd" % name
	if not ResourceLoader.exists(path) or name in ["scenario", "runner"]:
		_record({"scenario": name, "seed": 1, "steps": 0, "passed": false,
			"errors": ["no scenario %s in res://scenarios/" % name], "objectives": []})
		return
	var scenario: Node = load(path).new()
	scenario.finished.connect(_record, CONNECT_ONE_SHOT)
	main.add_child(scenario)


func _record(run: Dictionary) -> void:
	runs.append(run)
	if runs.size() < names.size():
		get_tree().reload_current_scene.call_deferred()
	else:
		_finish()


func _finish() -> void:
	var passed := runs.all(func(r): return r["passed"])
	var summary := []
	var tallies := {}
	for r: Dictionary in runs:
		for o: Dictionary in r["objectives"]:
			var t: Dictionary = tallies.get_or_add(o["name"], {"passed": 0, "failed": 0, "values": []})
			t["passed" if o["pass"] else "failed"] += 1
			t["values"].append(o["value"])
	var objective_names := tallies.keys()
	objective_names.sort()
	for name: String in objective_names:
		var t: Dictionary = tallies[name]
		var values: Array = t["values"]
		values.sort()
		summary.append({"objective": name, "tally": {
			"passed": t["passed"], "failed": t["failed"],
			"min": values.front(), "median": values[values.size() / 2], "max": values.back()}})
	var sorted_runs := runs.duplicate()
	sorted_runs.sort_custom(func(a, b): return a["scenario"] < b["scenario"])
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
	print(text)
	if out_dir != "":
		DirAccess.make_dir_recursive_absolute(out_dir)
		var f := FileAccess.open(out_dir.path_join("results.json"), FileAccess.WRITE)
		if f == null:
			printerr("cannot write %s" % out_dir.path_join("results.json"))
			get_tree().quit(2)
			return
		f.store_string(text + "\n")
		f.close()
	get_tree().quit(0 if passed else 1)
