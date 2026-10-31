## The skirmish's economy, played a turn at a time: a turn is a step of Baker's scenarios
## (sim/scenarios/economy.dta), one each physics tick of a match that has started. Each turn,
## each side earns its buildings' income, then buys: its opening first (ai.opening), then,
## by its seed, any unit it can afford. The data says what everything costs and earns.
##
##   {skirmish.start SEED}        a fresh match, both sides with their start points
##   {skirmish.turn}              the turns played
##   {skirmish.built_at KIND}     the turn blue first built KIND, or 99
##   {skirmish.army SIDE}         how many units SIDE has
##   {skirmish.outcome}           army_ready once blue has the army the data asks for, else 0
extends Node

const NEVER := 99

var running := false
var turn := 0
var sides := {}  # side -> {points, units: [kind], opening: index, rng}
var built_at := {}  # blue's first turn building each kind


func _ready() -> void:
	Baker.bind_function("skirmish.start", start)
	Baker.bind_function("skirmish.turn", func() -> int: return turn)
	Baker.bind_function("skirmish.built_at", func(kind: StringName) -> int:
		return built_at.get(String(kind), NEVER))
	Baker.bind_function("skirmish.army", func(side: StringName) -> int:
		return sides[String(side)]["units"].size() if sides.has(String(side)) else 0)
	Baker.bind_function("skirmish.outcome", outcome)


func start(seed: int = 1) -> void:
	turn = 0
	built_at = {}
	sides = {}
	for side in ["blue", "red"]:
		var rng := RandomNumberGenerator.new()
		rng.seed = seed * 2 + (1 if side == "red" else 0)
		sides[side] = {"points": int(Baker.get_value("economy.start_points", 0)), "units": [],
			"opening": 0, "rng": rng}
	running = true


func _physics_process(_delta: float) -> void:
	if running:
		play_turn()


func play_turn() -> void:
	turn += 1
	var units := SkirmishCatalog.units()
	var opening := SkirmishCatalog.opening()
	var income := 0
	for building in Baker.get_value("economy.buildings", [&"hq"]):
		income += int(SkirmishCatalog.buildings()[String(building)]["income"])
	for side in sides:
		var s: Dictionary = sides[side]
		s["points"] += income
		while s["opening"] < opening.size():
			var kind := String(opening[s["opening"]])
			if s["points"] < units[kind]["price"]:
				break
			_build(side, kind, units)
			s["opening"] += 1
		if s["opening"] >= opening.size():
			var affordable := SkirmishCatalog.affordable(s["points"])
			if not affordable.is_empty():
				var rng: RandomNumberGenerator = s["rng"]
				_build(side, affordable[rng.randi_range(0, affordable.size() - 1)], units)


func _build(side: String, kind: String, units: Dictionary) -> void:
	sides[side]["points"] -= units[kind]["price"]
	sides[side]["units"].append(kind)
	if side == "blue" and not built_at.has(kind):
		built_at[kind] = turn


func outcome() -> Variant:
	var army: int = Baker.get_value("economy.army_ready", 5)
	return &"army_ready" if sides.has("blue") and sides["blue"]["units"].size() >= army else 0
