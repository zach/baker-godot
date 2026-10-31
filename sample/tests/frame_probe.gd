## What a bound property read, frame by frame, from a node's _process: two of these, one
## processed before every other node and one after, see the same value in every frame if a
## reload lands before the frame's nodes run.
extends Node

var player: Node
var seen := {}  # process frame -> jump_height


func _process(_delta: float) -> void:
	seen[Engine.get_process_frames()] = player.jump_height
