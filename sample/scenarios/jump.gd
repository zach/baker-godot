## Scenario `jump`: from standing, one held jump peaks at the data's jump height.
extends "res://scenarios/scenario.gd"


func run() -> void:
	var apex := await jump_apex()
	var expected: float = Baker.get_value("player.jump.height", player.jump_height)
	finish([{
		"name": "jump.apex",
		"value": snappedf(apex, 0.01),
		"pass": absf(apex - expected) <= 2.0,
	}])
