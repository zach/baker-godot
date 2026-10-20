## A scenario: drives the player without a person and judges the run by its objectives.
##
##   godot --headless --path sample -- --scenario NAME [--out FILE]
##
## Prints one line of JSON (schema baker-godot.scenario/1: the scenario, whether it
## passed, each objective's value and verdict, and the layers it ran with), writes it to
## FILE too with --out, and quits with 0 if every objective passed, 1 if not. A scenario
## runs without the `local` layer, so its result does not depend on whose machine it is on.
extends Node

var player: CharacterBody2D
var _jumping := false
var _move := 0.0


func _ready() -> void:
	player = get_parent().get_node("Player")
	player.input_source = func() -> Dictionary:
		return {"move": _move, "jump": _jumping}
	run.call_deferred()


## Overridden by each scenario: drive the player, then finish() with the objectives.
func run() -> void:
	pass


func wait_physics(frames: int) -> void:
	for i in frames:
		await get_tree().physics_frame


## From standing, one jump held to its apex and back to the ground: how high it rose.
func jump_apex() -> float:
	await wait_physics(30)  # settled on the floor
	var start := player.position.y
	var apex := 0.0
	_jumping = true
	for frame in 600:
		await get_tree().physics_frame
		apex = max(apex, start - player.position.y)
		if frame > 10 and player.is_on_floor():
			break
	_jumping = false
	return apex


## objectives: [{name, desc, value, pass}].
func finish(objectives: Array) -> void:
	var passed := objectives.all(func(o): return o["pass"])
	var layers := Baker.get_layers().filter(func(l): return l["enabled"] and l["name"] != "scratch")
	var line := JSON.stringify({
		"schema": "baker-godot.scenario/1",
		"scenario": get_script().resource_path.get_file().get_basename(),
		"passed": passed,
		"objectives": objectives,
		"layers": layers.map(func(l): return l["name"]),
		"shipping": BakerRuntime.is_shipping(),
	})
	print(line)
	var args := OS.get_cmdline_user_args()
	var at := args.find("--out")
	if at >= 0 and at + 1 < args.size():
		var f := FileAccess.open(args[at + 1], FileAccess.WRITE)
		if f:
			f.store_line(line)
	get_tree().quit(0 if passed else 1)
