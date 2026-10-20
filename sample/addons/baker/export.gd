@tool
## Every export leaves out the files of the layers the project file marks dev_only: one
## person's tuning, an agent's proposal awaiting review. A shipping build would not read
## them anyway; this keeps them out of the package too.
extends EditorExportPlugin

const PROJECT := "res://baker-project.dta"

var _skip := PackedStringArray()


func _get_name() -> String:
	return "Baker"


func _export_begin(_features: PackedStringArray, _is_debug: bool, _path: String,
		_flags: int) -> void:
	_skip = BakerRuntime.dev_only_files(PROJECT) if FileAccess.file_exists(PROJECT) \
		else PackedStringArray()


func _export_file(path: String, _type: String, _features: PackedStringArray) -> void:
	if path in _skip:
		skip()
