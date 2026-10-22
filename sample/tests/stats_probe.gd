## A Resource of stats, for the runtime test of bind_resource: shared by every instance
## that holds it, as Godot games keep stats.
extends Resource

@export var kind: StringName = &"tank"
@export var max_hp := 1.0
@export var glow := 1.0
