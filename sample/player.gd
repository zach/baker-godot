## The player: a platformer body whose feel is tuned in data/tuning.dta.
##
## Each @export below is bound to a path by the BakerBinding child, so it holds the data's
## value, follows every reload and layer toggle, and reads as a plain property. A path the
## data lacks leaves the value set here standing.
extends CharacterBody2D

@export var run_speed := 200.0
@export var run_accel := 1200.0
@export var run_friction := 1600.0
@export var air_control := 0.5
@export var jump_height := 80.0
@export var jump_time_to_peak := 0.4
@export var fall_multiplier := 1.5
@export var coyote_time := 0.08
@export var jump_buffer := 0.1
@export var max_fall := 800.0

## Where input comes from: the keyboard, or a scenario that drives the player.
## It returns {"move": -1..1, "jump": bool (held)}.
var input_source := Callable(self, "_keyboard")

var _coyote_left := 0.0
var _buffer_left := 0.0
var _jump_was_held := false


func gravity() -> float:
	return 2.0 * jump_height / (jump_time_to_peak * jump_time_to_peak)


func jump_speed() -> float:
	return 2.0 * jump_height / jump_time_to_peak


func _keyboard() -> Dictionary:
	return {
		"move": Input.get_axis("ui_left", "ui_right"),
		"jump": Input.is_action_pressed("ui_accept") or Input.is_action_pressed("ui_up"),
	}


func _physics_process(delta: float) -> void:
	var input: Dictionary = input_source.call()
	var move: float = input["move"]
	var held: bool = input["jump"]
	var pressed := held and not _jump_was_held
	_jump_was_held = held

	var control := 1.0 if is_on_floor() else air_control
	if move != 0.0:
		velocity.x = move_toward(velocity.x, move * run_speed, run_accel * control * delta)
	else:
		velocity.x = move_toward(velocity.x, 0.0, run_friction * control * delta)

	_coyote_left = coyote_time if is_on_floor() else max(_coyote_left - delta, 0.0)
	_buffer_left = jump_buffer if pressed else max(_buffer_left - delta, 0.0)
	if _buffer_left > 0.0 and _coyote_left > 0.0:
		velocity.y = -jump_speed()
		_buffer_left = 0.0
		_coyote_left = 0.0

	# Half the gravity before the move and half after, so a jump peaks at jump_height
	# exactly, at any frame rate.
	var g := gravity()
	if velocity.y > 0.0 or not held:
		g *= fall_multiplier
	velocity.y = min(velocity.y + g * delta * 0.5, max_fall)
	move_and_slide()
	velocity.y = min(velocity.y + g * delta * 0.5, max_fall)
