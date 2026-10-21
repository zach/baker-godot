@tool
## The debugger channel between the editor and the running game. The game sends
## baker:session whenever its session changes; the editor sends baker:toggle,
## baker:kept and baker:discard.
extends EditorDebuggerPlugin

signal session_received(session: Dictionary)
signal running_changed(running: bool)


func _has_capture(prefix: String) -> bool:
	return prefix == "baker"


func _capture(message: String, data: Array, _session_id: int) -> bool:
	if message.trim_prefix("baker:") == "session" and data.size() == 1:
		session_received.emit(data[0])
		return true
	return false


func _setup_session(session_id: int) -> void:
	var session := get_session(session_id)
	session.started.connect(func(): running_changed.emit(true))
	session.stopped.connect(func(): running_changed.emit(false))


func is_running() -> bool:
	for session in get_sessions():
		if session.is_active():
			return true
	return false


## Sends baker:<message> to every running game.
func send(message: String, data: Array) -> void:
	send_raw("baker:" + message, data)


## Sends a message as written, such as one of Godot's own scene: messages.
func send_raw(message: String, data: Array) -> void:
	for session in get_sessions():
		if session.is_active():
			session.send_message(message, data)
