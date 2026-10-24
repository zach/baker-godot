## A card of the skirmish, as a game whose content is the engine's own resources keeps
## one: a file a card (cards/*.tres), its text beside its tuning. Baker.bind_records binds the
## fields under "Tuning" to cards.<file>.<field>, so a layer retunes a card by name and the
## file stays the bottom of the chain; the rest (its title, its text) is not data's.
class_name SkirmishCard
extends Resource

@export var title := ""
@export_multiline var text := ""

@export_group("Tuning")
@export_range(0, 10) var cost := 1
@export_range(0, 50) var damage := 0
## The card this one becomes when upgraded: a key in the data (cards.strike.upgrade bash).
@export var upgrade: SkirmishCard
## Damage by the card's level, x the level, y a multiplier.
@export var scaling: Curve
