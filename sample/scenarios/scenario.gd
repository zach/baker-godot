## A scenario: drives the player without a person and judges the run by its objectives.
## The runner (runner.gd) adds one to a fresh instance of the main scene; it calls finish()
## once with its objectives.
extends Node

## The run, as one entry of stacks.sim/1.1's "runs".
signal finished(run: Dictionary)

var player: CharacterBody2D
var seed := 1  # set by the runner; the game draws no random numbers, so runs repeat
var _jumping := false
var _move := 0.0
var _steps := 0


func _ready() -> void:
	player = get_parent().get_node("Player")
	player.input_source = func() -> Dictionary:
		return {"move": _move, "jump": _jumping}
	run.call_deferred()


func _physics_process(_delta: float) -> void:
	_steps += 1


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


## objectives: [{name, value, pass}].
func finish(objectives: Array) -> void:
	var results := objectives.map(func(o): return {
		"name": o["name"], "value": o["value"], "pass": o["pass"], "over": "seed", "error": ""})
	finished.emit({
		"scenario": get_script().resource_path.get_file().get_basename(),
		"seed": seed,
		"steps": _steps,
		"passed": objectives.all(func(o): return o["pass"]),
		"errors": [],
		"objectives": results,
	})
