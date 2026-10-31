## A lockstep match's first exchange: the host sends its lobby's settings, a layer as text,
## which the joining peer takes as its own; then each locks its data and sends the
## fingerprint, and peers whose data differs refuse the match, before a turn is played on it.
## Two processes, on one machine:
##
##   godot --headless --path sample res://skirmish/peer.tscn -- --host PORT [--lobby TEXT]
##   godot --headless --path sample res://skirmish/peer.tscn -- --join PORT
##
## PORT 0: a free one. The host prints "listening on PORT" once its data is locked. Each
## prints "agreed PRINT" and exits 0, or "refused: mine PRINT, theirs PRINT" and exits 1; 2
## if no peer came.
extends Node

var server: TCPServer
var peer: StreamPeerTCP
var hosting := false
var mine := ""  # set once this peer's data is locked
var lobby_sent := false
var buffer := PackedByteArray()  # what the peer sent, not yet read


func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	var host := args.find("--host")
	var join := args.find("--join")
	var lobby := args.find("--lobby")
	if host >= 0 and host + 1 < args.size():
		hosting = true
		if lobby >= 0 and lobby + 1 < args.size():
			Baker.push_layer_text("lobby", args[lobby + 1], {})
		mine = Baker.lock()  # the data the match is played on: nothing changes it until unlock()
		server = TCPServer.new()
		if server.listen(int(args[host + 1]), "127.0.0.1") != OK:
			return end("cannot listen", 2)
		print("listening on %d" % server.get_local_port())
	elif join >= 0 and join + 1 < args.size():
		peer = StreamPeerTCP.new()
		if peer.connect_to_host("127.0.0.1", int(args[join + 1])) != OK:
			return end("cannot connect", 2)
	else:
		return end("usage: -- --host PORT [--lobby TEXT] | --join PORT", 2)
	get_tree().create_timer(20.0).timeout.connect(end.bind("no peer", 2))


func _process(_delta: float) -> void:
	if server and not peer and server.is_connection_available():
		peer = server.take_connection()
	if not peer:
		return
	peer.poll()
	if peer.get_status() != StreamPeerTCP.STATUS_CONNECTED:
		return
	if hosting and not lobby_sent:
		# The lobby's layer as text (empty: none), then the fingerprint of the data with it.
		var has_lobby := Baker.get_layers().any(func(l): return l["name"] == "lobby")
		peer.put_utf8_string(Baker.layer_text("lobby") if has_lobby else "")
		peer.put_utf8_string(mine)
		lobby_sent = true
	var message: Variant = _next_message()
	if message == null:
		return
	if not hosting and mine == "":
		# The host's lobby, taken as this peer's own; then this peer's data is locked.
		if message != "":
			Baker.push_layer_text("lobby", message, {})
		mine = Baker.lock()
		peer.put_utf8_string(mine)
		return
	if message == mine:
		end("agreed %s" % mine, 0)
	else:
		end("refused: mine %s, theirs %s" % [mine, message], 1)


## The next whole string the peer sent, or null: put_utf8_string sends its length in four
## bytes, then its bytes.
func _next_message() -> Variant:
	var available := peer.get_available_bytes()
	if available > 0:
		var got := peer.get_data(available)
		if got[0] == OK:
			buffer.append_array(got[1])
	if buffer.size() < 4:
		return null
	var length := buffer.decode_u32(0)
	if buffer.size() < 4 + length:
		return null
	var text := buffer.slice(4, 4 + length).get_string_from_utf8()
	buffer = buffer.slice(4 + length)
	return text


func end(what: String, status: int) -> void:
	print(what)
	Baker.unlock()
	set_process(false)
	get_tree().quit(status)
