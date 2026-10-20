## Scenario `reach`: a jump from the floor clears the first platform, and not the second.
##
## The first platform's top is 100 px above the floor, and a jump should clear it with
## 10 px to spare. The second's is 180 px up: the climb is meant to take two jumps, so a
## jump from the floor stays at least 20 px below it.
extends "res://scenarios/scenario.gd"

const PLATFORM := 100.0
const SPARE := 10.0
const SECOND := 180.0
const BELOW := 20.0


func run() -> void:
	var apex := await jump_apex()
	finish([
		{
			"name": "reach.first_platform",
			"value": snappedf(apex, 0.01),
			"pass": apex >= PLATFORM + SPARE,
		},
		{
			"name": "reach.not_second_platform",
			"value": snappedf(apex, 0.01),
			"pass": apex <= SECOND - BELOW,
		},
	])
