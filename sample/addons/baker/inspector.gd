@tool
## Under each bound property in the inspector: every layer that has a say in its value,
## bottom to top, with its file and line, and which one wins.
extends EditorInspectorPlugin

const Session := preload("res://addons/baker/session.gd")

var model: Session


func _init(m: Session) -> void:
	model = m


func _can_handle(object: Object) -> bool:
	return object is Node and _binding(object) != null


func _parse_property(object: Object, _type: Variant.Type, name: String, _hint: PropertyHint,
		_hint_string: String, _usage: int, _wide: bool) -> bool:
	var binding := _binding(object)
	if binding == null or not binding.bindings.has(name):
		return false
	var label := Label.new()
	label.text = "%s\n%s" % [binding.bindings[name], model.chain_text(binding.bindings[name])]
	label.add_theme_font_size_override("font_size", 11)
	label.modulate = Color(1, 1, 1, 0.7)
	add_custom_control(label)
	return false


func _binding(node: Node) -> BakerBinding:
	for child in node.get_children():
		if child is BakerBinding:
			return child
	return null
