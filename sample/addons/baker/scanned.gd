@tool
## A scanned node's stand-in: its bound properties' values and export hints, as properties of
## its own, so a BakerBinding binds them as it would the node's.
extends Resource

var _values := {}
var _infos := []


func add(property: String, value: Variant, info: Dictionary) -> void:
	_values[property] = value
	_infos.append(info)


func _get(property: StringName) -> Variant:
	return _values.get(String(property))


func _set(property: StringName, value: Variant) -> bool:
	if not _values.has(String(property)):
		return false
	_values[String(property)] = value
	return true


func _get_property_list() -> Array[Dictionary]:
	var out: Array[Dictionary] = []
	out.assign(_infos)
	return out
