## The player's functions in Baker's script, for scenarios, objectives and the console:
##
##   {player.hold_jump}     hold the jump button, from now on
##   {player.release_jump}  let it go
##   {player.move X}        hold left (-1) or right (1), or neither (0)
##   {player.apex}          how high the last jump rose above its takeoff, in pixels
##
## Calling one drives the player from then on, in place of the keyboard. Main adds a driver
## to each scene, so each binds its own player (Baker.bind_function replaces by name).
extends Node

var _jump := false
var _move := 0.0
var _takeoff := NAN  # the player's height when the jump was held; NAN: none yet
var _apex := 0.0

@onready var player: CharacterBody2D = get_parent().get_node("Player")


func _ready() -> void:
	Baker.bind_function("player.hold_jump", hold_jump)
	Baker.bind_function("player.release_jump", release_jump)
	Baker.bind_function("player.move", move)
	Baker.bind_function("player.apex", apex)


func _physics_process(_delta: float) -> void:
	if not is_nan(_takeoff):
		_apex = max(_apex, _takeoff - player.position.y)


func hold_jump() -> void:
	_drive()
	_jump = true
	_takeoff = player.position.y
	_apex = 0.0


func release_jump() -> void:
	_drive()
	_jump = false


func move(direction: float) -> void:
	_drive()
	_move = clampf(direction, -1.0, 1.0)


func apex() -> float:
	return snappedf(_apex, 0.01)


func _drive() -> void:
	player.input_source = func() -> Dictionary: return {"move": _move, "jump": _jump}
