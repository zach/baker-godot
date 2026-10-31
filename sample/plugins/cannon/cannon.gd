## The cannon minigame, a plugin: its tuning binds under its own name (its scene's
## BakerBinding: "minigames.{plugin}.gravity", {plugin} being this directory's name).
extends Node

@export_range(100, 2000) var gravity := 980.0
