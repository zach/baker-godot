## The skirmish's unit and building types, read whole from the data (data/skirmish.dta):
## the production menu and the AI read the tables here rather than path by path.
class_name SkirmishCatalog
extends RefCounted


## {kind: {hp, move, attack, armor, price, ...}}.
static func units() -> Dictionary:
	return Baker.get_value("units", {})


## {kind: {income, capture}}.
static func buildings() -> Dictionary:
	return Baker.get_value("buildings", {})


## The unit kinds `points` can build, cheapest first: the production menu.
static func affordable(points: int) -> Array:
	var table := units()
	var kinds := table.keys().filter(func(k): return table[k]["price"] <= points)
	kinds.sort_custom(func(a, b): return table[a]["price"] < table[b]["price"])
	return kinds


## What the AI builds first, in order.
static func opening() -> Array:
	return Baker.get_value("ai.opening", [])
