## The skirmish's economy, played a turn at a time: a turn is a step of Baker's scenarios
## (sim/scenarios/economy.dta), one each physics tick of a match that has started. Each turn,
## each side earns its buildings' income less its units' upkeep, then buys: its opening first
## (ai.opening), then one of what it can afford, or nothing, drawn by its seed with the
## weights ai.weights gives (a unit kind, a building kind, or save). The data says what
## everything costs, earns and keeps costing, so an outcome depends on the seed.
##
##   {skirmish.start SEED}        a fresh match, both sides with their start points
##   {skirmish.turn}              the turns played
##   {skirmish.built_at KIND}     the turn blue first built KIND (a unit or a building), or 99
##   {skirmish.army SIDE}         how many units SIDE has
##   {skirmish.outcome}           army_ready once blue has the army the data asks for, else 0
extends Node

const NEVER := 99

var running := false
var turn := 0
var sides := {}  # side -> {points, units: [kind], buildings: [kind], opening: index, rng}
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
	var buildings: Array = Baker.get_value("economy.buildings", [&"hq"])
	for side in ["blue", "red"]:
		var rng := RandomNumberGenerator.new()
		rng.seed = seed * 2 + (1 if side == "red" else 0)
		sides[side] = {"points": int(Baker.get_value("economy.start_points", 0)), "units": [],
			"buildings": buildings.map(func(b): return String(b)), "opening": 0, "rng": rng}
	running = true


func _physics_process(_delta: float) -> void:
	if running:
		play_turn()


func play_turn() -> void:
	turn += 1
	var units := SkirmishCatalog.units()
	var buildings := SkirmishCatalog.buildings()
	var opening := SkirmishCatalog.opening()
	var weights: Dictionary = Baker.get_value("ai.weights", {})
	for side in sides:
		var s: Dictionary = sides[side]
		for b: String in s["buildings"]:
			s["points"] += int(buildings[b]["income"])
		for u: String in s["units"]:
			s["points"] -= int(units[u].get("upkeep", 0))
		while s["opening"] < opening.size():
			var kind := String(opening[s["opening"]])
			if s["points"] < units[kind]["price"]:
				break
			_buy(side, kind, int(units[kind]["price"]), "units")
			s["opening"] += 1
		if s["opening"] >= opening.size():
			_choose(side, units, buildings, weights)


## One of what the side can afford, or nothing, by the weights and the side's seed.
func _choose(side: String, units: Dictionary, buildings: Dictionary, weights: Dictionary) -> void:
	var s: Dictionary = sides[side]
	var options := []  # [kind, cost, list, weight]
	for kind: String in weights:
		var weight := float(weights[kind])
		if weight <= 0:
			continue
		if kind == "save":
			options.append([kind, 0, "", weight])
		elif units.has(kind) and units[kind]["price"] <= s["points"]:
			options.append([kind, int(units[kind]["price"]), "units", weight])
		elif buildings.has(kind) and buildings[kind].has("cost") and buildings[kind]["cost"] <= s["points"]:
			options.append([kind, int(buildings[kind]["cost"]), "buildings", weight])
	var total := 0.0
	for o in options:
		total += o[3]
	if total <= 0:
		return
	var rng: RandomNumberGenerator = s["rng"]
	var pick := rng.randf() * total
	for o in options:
		pick -= o[3]
		if pick < 0:
			if o[2] != "":
				_buy(side, o[0], o[1], o[2])
			return


func _buy(side: String, kind: String, cost: int, list: String) -> void:
	sides[side]["points"] -= cost
	sides[side][list].append(kind)
	if side == "blue" and not built_at.has(kind):
		built_at[kind] = turn


func outcome() -> Variant:
	var army: int = Baker.get_value("economy.army_ready", 5)
	return &"army_ready" if sides.has("blue") and sides["blue"]["units"].size() >= army else 0
