## Scenario `reach`: a jump from the floor clears the first platform.
##
## The platform's top is 100 px above the floor; the objective asks for 10 px to spare.
extends "res://scenarios/scenario.gd"

const PLATFORM := 100.0
const SPARE := 10.0


func run() -> void:
	var apex := await jump_apex()
	finish([{
		"name": "reach.first_platform",
		"value": snappedf(apex, 0.01),
		"pass": apex >= PLATFORM + SPARE,
	}])
