## A lockstep match's first exchange: each peer locks its data and sends the fingerprint,
## and peers whose data differs refuse the match, before a turn is played on it. Two
## processes, on one machine:
##
##   godot --headless --path sample res://skirmish/peer.tscn -- --host PORT   (0: a free one)
##   godot --headless --path sample res://skirmish/peer.tscn -- --join PORT
##
## The host prints "listening on PORT" once its data is locked. Each prints "agreed PRINT"
## and exits 0, or "refused: mine PRINT, theirs PRINT" and exits 1; 2 if no peer came.
extends Node

const LENGTH := 4 + 16  # put_utf8_string: the length, then a fingerprint's sixteen digits

var server: TCPServer
var peer: StreamPeerTCP
var mine := ""
var sent := false


func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	var host := args.find("--host")
	var join := args.find("--join")
	mine = Baker.lock()  # the data the match is played on: nothing changes it until unlock()
	if host >= 0 and host + 1 < args.size():
		server = TCPServer.new()
		if server.listen(int(args[host + 1]), "127.0.0.1") != OK:
			return end("cannot listen", 2)
		print("listening on %d" % server.get_local_port())
	elif join >= 0 and join + 1 < args.size():
		peer = StreamPeerTCP.new()
		if peer.connect_to_host("127.0.0.1", int(args[join + 1])) != OK:
			return end("cannot connect", 2)
	else:
		return end("usage: -- --host PORT | --join PORT", 2)
	get_tree().create_timer(20.0).timeout.connect(end.bind("no peer", 2))


func _process(_delta: float) -> void:
	if server and not peer and server.is_connection_available():
		peer = server.take_connection()
	if not peer:
		return
	peer.poll()
	if peer.get_status() != StreamPeerTCP.STATUS_CONNECTED:
		return
	if not sent:
		peer.put_utf8_string(mine)
		sent = true
	if peer.get_available_bytes() >= LENGTH:
		var theirs := peer.get_utf8_string()
		if theirs == mine:
			end("agreed %s" % mine, 0)
		else:
			end("refused: mine %s, theirs %s" % [mine, theirs], 1)


func end(what: String, status: int) -> void:
	print(what)
	Baker.unlock()
	set_process(false)
	get_tree().quit(status)
