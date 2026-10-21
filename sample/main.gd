## The sample level: a floor, some platforms, the player, and a line of help.
##
## F1 toggles the `floaty` experiment layer while playing. Run with `-- --baker sim` to run
## the scenarios in sim/scenarios.dta instead of playing: each run reloads this scene, which
## tells Baker when it is set up.
extends Node2D

const PLATFORMS := [
	Rect2(-2000, 400, 4000, 200),  # the floor
	Rect2(140, 300, 160, 20),
	Rect2(380, 220, 160, 20),
	Rect2(620, 140, 160, 20),
	Rect2(-260, 290, 120, 20),
]

@onready var player: CharacterBody2D = $Player
@onready var help: Label = $HUD/Help


func _ready() -> void:
	for rect: Rect2 in PLATFORMS:
		_add_platform(rect)
	Baker.changed.connect(_show_layers)
	_show_layers()
	add_child(preload("res://sim/driver.gd").new())
	if Baker.is_simulating():
		Baker.sim_ready.call_deferred()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F1:
		Baker.toggle_layer("floaty")


func _show_layers() -> void:
	var on := PackedStringArray()
	for layer: Dictionary in Baker.get_layers():
		if layer["enabled"] and layer["name"] != "scratch":
			on.append(layer["name"])
	help.text = "Arrows move, Space jumps, F1 toggles floaty.  Layers: %s" % ", ".join(on)


func _add_platform(rect: Rect2) -> void:
	var body := StaticBody2D.new()
	body.position = rect.get_center()
	var shape := CollisionShape2D.new()
	shape.shape = RectangleShape2D.new()
	shape.shape.size = rect.size
	body.add_child(shape)
	var look := Polygon2D.new()
	var h := rect.size / 2
	look.polygon = PackedVector2Array([-h, Vector2(h.x, -h.y), h, Vector2(-h.x, h.y)])
	look.color = Color(0.25, 0.3, 0.4)
	body.add_child(look)
	add_child(body)
