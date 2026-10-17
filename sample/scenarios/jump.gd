## Scenario `jump`: from standing, one held jump; checks the apex against the data.
##
##   godot --headless --path sample -- --scenario=jump
##
## Prints one JSON line and quits with 0 if the apex is within 2 px of player.jump.height.
extends Node

var _frames := 0
var _start_y := 0.0
var _apex := 0.0
var _jumping := false


func _ready() -> void:
	var player: CharacterBody2D = get_parent().get_node("Player")
	player.input_source = func() -> Dictionary:
		return {"move": 0.0, "jump": _jumping}


func _physics_process(_delta: float) -> void:
	var player: CharacterBody2D = get_parent().get_node("Player")
	_frames += 1
	if _frames == 30:  # settled on the floor
		_start_y = player.position.y
		_jumping = true
	if _jumping:
		_apex = max(_apex, _start_y - player.position.y)
		if _frames > 40 and player.is_on_floor():
			_finish(player)


func _finish(player: CharacterBody2D) -> void:
	var expected: float = Baker.get_value("player.jump.height", player.jump_height)
	var ok: bool = abs(_apex - expected) <= 2.0
	print(JSON.stringify({
		"scenario": "jump",
		"apex": snappedf(_apex, 0.01),
		"expected": expected,
		"layers": Baker.get_layers().filter(func(l): return l["enabled"]).map(func(l): return l["name"]),
		"ok": ok,
	}))
	get_tree().quit(0 if ok else 1)
