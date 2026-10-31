## A unit for the runtime test of bindings at scale: its kind is the key of its path, and it
## copies its hit points at spawn, as a game does, re-deriving them when the data changes.
extends Node

@export var kind: StringName = &"scout"
@export_range(1, 100, 1) var max_hp := 10.0
var hp := 0.0
var changes := 0


func _ready() -> void:
	hp = max_hp
	var binding := BakerBinding.new()
	binding.name = "BakerBinding"
	binding.bindings = {"max_hp": "units.{kind}.max_hp"}
	binding.property_changed.connect(_on_changed)
	add_child(binding)


func _on_changed(property: StringName) -> void:
	changes += 1
	if property == &"max_hp":
		hp = max_hp
