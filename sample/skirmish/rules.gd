## The skirmish's rules, which the data defines in Baker's script (data/skirmish.dta), read
## with the unit table: a layer that defines a rule again changes what these return.
class_name SkirmishRules
extends RefCounted


## The damage of one hit of `attacker` on `defender`, at the attacker's `level`.
static func damage(attacker: String, defender: String, level: int = 0) -> int:
	var units := SkirmishCatalog.units()
	return int(Baker.call_function("skirmish.damage",
		[units[attacker]["attack"], units[defender]["armor"], level]))


## How many hits of `attacker` destroy a fresh `defender`.
static func hits_to_kill(attacker: String, defender: String, level: int = 0) -> int:
	var hp: int = SkirmishCatalog.units()[defender]["hp"]
	return ceili(float(hp) / damage(attacker, defender, level))
