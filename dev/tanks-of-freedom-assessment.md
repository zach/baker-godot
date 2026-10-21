# Tanks of Freedom II as a test of Baker for Godot

An assessment of Baker for Godot 0.1.0 against a nontrivial, shipped Godot game: what the
game keeps as data, what the adapter serves as it is, where it falls short, and what to
build next. October 21, 2026.

**The game.** Tanks of Freedom II (P1X, MIT for code and art), a turn-based strategy game
on Godot 4.4 with a campaign, skirmish against an AI, hot-seat, LAN and online play, a map
editor with scripted missions, and DLC campaigns. Its master branch was read at
`github.com/P1X-in/Tanks-of-Freedom-II`. The original Tanks of Freedom (2015, Godot 1.x)
was only glanced at; its units set their stats in `_init` of a script per unit, which is the
same shape with one fewer indirection. Nothing from either game was copied into this
repository; this note describes them.

| | |
|---|---|
| GDScript files | 292, about 24,000 lines |
| Numeric tunables set in scene files | 166, across 82 `.tscn` files |
| Numeric constants in scripts | 93 |
| Units on a campaign map at start | 47 to 116 on the larger ones |
| Randomness in the rules | none in combat; only collateral damage, music choice, the AI's spawn tile, camera shake |

## 1. Where the game keeps its numbers

The game is more data-driven than most Godot projects, which makes it a fair test: the
question is not "can Baker add data" but "what does Baker add to data the engine already
holds".

- **Unit stats are `@export` properties** (`max_hp`, `max_move`, `attack`, `armor`,
  `max_attacks`, `unit_value`, `can_fly`, `can_capture`) on one base script, set in seven
  template scenes (infantry, mobile infantry, tank, rocket artillery, helicopter, scout,
  truck). Each of four factions has a scene per unit that inherits its template, so the 28
  faction scenes carry no numbers. All eight heroes share one stat block and differ by
  their abilities; the four leaders share another.
- **Building stats are duplicated four times.** `ap_gain`, `capture_value` and
  `require_crew` are set in each faction's scene for each of five building types: twenty
  files, and a change to the barracks' income is four identical edits. The same holds for
  unit prices: each faction has six production-ability scenes carrying `ap_cost`, and the
  numbers are the same in all four. Hero prices are eight files, all 150.
- **Ability numbers are split three ways:** cooldown, range and AP cost in the ability's
  scene; damage and heal as `@export` on its script; and a few as script constants
  (precision strike's damage, supply's heal).
- **The rules are code.** Experience per level and the level cap, the stat bonus at each
  level (and the different schedule for heroes), the AP cap, that an attack costs one AP
  and one move, that capturing consumes the crew, the collateral chance, retaliation, and
  what each passive hero ability does (a flat discount on a hard-coded list of air unit
  templates, and the like) are literals in GDScript.
- **The AI is about forty magic numbers:** unit limits, a soft limit, proximity bonuses
  by squared distance, a death penalty, thresholds per hero brain. Balance patches have
  touched these (the change log's "extended AI lookup range", and a mission's AI income).
- **Settings** are a flat dictionary with defaults in code, saved as JSON under `user://`,
  with a Steam Deck profile that overrides eleven of them when the platform is detected.
  Several are enumerations: antialiasing as 0/2/4/8, volume as 0 to 10 mapped to
  decibels, end-turn speed as "x1", "x2", "x4".
- **Maps, campaigns, saves and progress are JSON.** A map holds its tiles and a `scripts`
  section: triggers (ten types, each with typed details) that start stories (sequences of
  about thirty outcome actions with details and delays). It is a small event language
  that a GUI in the map editor writes, and that the game interprets. Campaign manifests
  list missions with their players and starting AP. DLC campaigns are read from a
  directory beside the executable, a third root besides `res://` and `user://`.
- **Multiplayer is lockstep by relayed input.** Every peer runs the same rules on its own
  machine; what crosses the network is tile selections, ability indices and end-turn, with
  the one random outcome (collateral) sent explicitly. Two peers with different numbers
  desynchronize silently.
- **Combat is deterministic.** Damage is attack minus armor; a kill is attack at least HP
  plus armor. This makes balance questions answerable by a headless run.

## 2. What the adapter serves as it stands

- **Unit stats, in one file instead of seven scenes.** A `BakerBinding` under the base
  unit scene binds the stat properties to paths; the faction scenes inherit it. The data's
  values win, the scene's stand as defaults, and reads stay property reads. The `@export`
  hints become the tweaks' ranges, so `check` holds a proposal to them (requirement 0010).
- **The duplicated building and price numbers collapse.** Four scenes bound to one path
  (`buildings.barracks.ap_gain`) take one value, and the four-file edit becomes a one-line
  diff. `explain` shows which layer set it.
- **Balance passes as layers.** An experiment layer (`cheap_air`, `tough_heroes`) toggled
  while a match plays is the demonstration this game needs: a designer compares two
  tunings over the same turns. The `agent` layer and `promote --patch` give an agent the
  same loop.
- **Platform profiles as layers.** The Steam Deck overrides are a disabled layer the game
  enables at startup when it detects the platform, and `explain` answers "why is MSAA
  off on this machine". A user's choices go to the `local` layer in `user://` through
  `write_back_to`, which is what the settings file does today, with provenance.
- **Enumerated settings are requirement 0014's case exactly:** the antialiasing levels and
  the end-turn speed strings are the allowed values of an integer and a string tweak,
  which `check` cannot hold today and a panel cannot offer.
- **DLC is the cookbook's keyed-table case.** A DLC campaign that adds units, abilities or
  prices is a layer that merges into the keyed tables, declared as a pack with a version.
  The game's own gate, abilities carrying a DLC version that a map's editor version may
  disable, is a pack requirement in Baker's terms.
- **Headless balance checks fit `sim`.** Combat has no randomness and the AI's choices
  are deterministic apart from its spawn tile, so scenarios such as "a tank cannot be
  killed by one infantry attack", "an HQ's income affords a tank by turn 3" or "the AI
  takes an undefended tower within five turns" judge a tuning repeatably. The game runs
  under `--headless` already (its CI-less tests are manual, so this would be its first
  automated judgement of balance).
- **The engine's binary with arguments (requirement 0016)** is this game's case too: it is
  started as `godot --headless --path . -- ...`, not as its own executable.

## 3. Gaps, ranked by what the game would hit first

### G1. One binding per instance does not scale to a board of units

`BakerBinding` makes a `Tweak<T>` per bound property per node instance, on `_ready`,
walking the node's property list once per property. A campaign map starts with up to 116
units of seven stats each: over 800 tweaks for 50 paths, registered as the map loads, and
more each time a factory produces a unit mid-turn. Under the editor's debugger every
instance is compared ten times a second for capture. `reference` and the dock's session
list would carry each path dozens of times.

**What the game needs:** one tweak per path, however many nodes read it. Two shapes serve:

- a binding with one tweak per path and a list of targets, so the second instance of a
  scene attaches to the first's tweak instead of registering again;
- binding the properties of a shared `Resource` (the engine's own pattern for stats a
  scene's instances share) once, at startup, from a list in the project file or an
  autoload, with instances reading the resource as they do now.

Both are adapter work. The second also gives a game that already keeps stats in resources
a path in without touching its scenes.

### G2. Paths that take their key from the node

The base building scene is inherited by all twenty faction-and-type scenes, and the base
unit scene by every unit. A binding in the base scene cannot name the path, because the
type is in a property (`template_name`, `unit_class`). A placeholder in a path, filled from
the node's own properties at bind time, lets one binding in the base scene serve every
descendant:

```
bindings = {"ap_gain": "buildings.{template_name}.ap_gain",
            "max_hp":  "units.{unit_class}.max_hp"}
```

With G1 this is one binding node in two base scenes for the whole game's stats, instead of
twenty-seven.

### G3. Keyed tables, read whole

Units by name, abilities by name, buildings by name: the game's data is tables of records,
and the cookbook's advice for DLC is to keep them keyed. The adapter reads scalars and
value lists; a keyed array (`(units (infantry (hp 10) (attack 6)) ...)`) has no reading as a
`Dictionary`, and an array of records none as an `Array` of them. Code that iterates a
table (the production menu, the AI's unit mix, a hero's list of affected templates) needs
`get_value("units", {})` to give a Dictionary of Dictionaries, with nested values
converted as `to_variant` does today. Writing a Dictionary back is the same in reverse,
for an editor that builds data.

With tables read whole, nothing holds them to a type: the shape is the check. The adapter
could emit a `(shapes ...)` declaration from a GDScript class's exported properties and
hints, so `check` validates `units.*` against the class that reads it.

### G4. Rules in data, callable from GDScript

Level bonuses, experience per level, the passive abilities' discounts and the AI's
valuation are formulas and lists. The sample keeps formulas in GDScript and only their
inputs in data; this game has enough of them, changed often enough (the hero rebalance in
its 1.0.0 notes), that some belong in script: `{func level_armor_bonus ...}` in a rules
pack, with the affected template lists as `(list symbol)` data, so an experiment layer can
change a rule, not only a number.

The adapter has no call into Baker's script from GDScript that returns a value:
`eval_request` answers console lines as JSON text. A `Baker.call_function(name, args)`
and a `Baker.eval(text)` returning a Variant close this. The cost is a script evaluation
per call, which is nothing for a turn-based game and wrong for a per-frame path; the
documentation should say so.

### G5. Lockstep peers need the same data, and a way to know they have it

Every peer runs the rules locally. A `local` layer, an enabled experiment, a live edit from
the remote inspector on one machine, or a DLC layer the other peer lacks, changes a number
on one side and the match drifts with no error. The game needs, before a match:

- a **fingerprint** of the effective data (a hash over the merged tree, or over each
  enabled layer's bytes and the stack's names), cheap enough to exchange in the lobby and
  to store in a save, so a save made under other tuning is detected too;
- a **lock** for the duration of a match: no layer toggles, no live edits, no reload
  taking effect, unless all peers do the same. The adapter can hold edits and toggles
  while locked and apply them at the next boundary, which is also what live overlays need
  (baker-liveops applies at a lobby or a session start for the same reason).

The lock is adapter work. The fingerprint can be adapter work (hashing the layer files it
reads), but a digest of the merged tree that every host computes the same way is a Baker
feature; see §5.

### G6. Layers that appear at runtime

The project file names the stack once. This game discovers content after it starts: DLC
campaigns in a directory beside the executable, maps downloaded from its online service
into `user://`, user maps from the editor. The runtime offers `open(project)` and toggles,
but no `push_layer(name, path, options)` (with `optional`, `below` and `dev_only`),
`push_layer_text(name, text)` for content that arrived as text and must stay text, or
`remove_layer(name)`. The same calls are what a live-ops client in a Godot game would use.

The directory beside the executable is a third root. The adapter maps bare paths to
`res://`; an absolute path computed at startup already works, so this is a documentation
point once layers can be pushed at runtime.

### G7. Scenarios in turns, not seconds

A `SimRun` steps per physics tick at a fixed rate and a scenario is written in seconds. A
turn-based game's unit of time is the turn, and its AI advances through wall-clock timers
and camera tweens that `--fixed-fps` makes fast but not free (each AI action waits a
tenth of a second of game time, and a camera move a second). A scenario for this game
wants `(at turn 5 ...)` and "judge when a side has lost", not `(seconds 40)` with a guess at
how long five turns take.

The adapter's driver can map a step to "advance one turn with animation and waits
disabled", so seconds become turns by convention. A scenario time in host-defined steps,
and a run that ends on a condition, would say it in the data; see §5.

### G8. Reading a tree that is not a layer

Maps and campaign manifests are read as JSON documents, not as tuning. If scripted missions
were written in Baker's format (a trigger is `(trigger name (type turn) (player 0) (turn 1)
(one_off) (story welcome))`, and `check` would catch an unknown action, which the game
today reports at load with a print), the game would need to read one file as a tree, with
`text_only` and `refuse_includes` because maps are user content shared online. The
adapter has no `Baker.load_tree(path)` returning a Dictionary, nor the reverse for an
editor that saves one. This is an opportunity more than a gap: the game's JSON works, and
its editor GUI is the real author of these files.

### G9. The editor's views are per scene; the game's data is project-wide

The dock's Tweaks tab lists the edited scene's bound properties. With stats bound in two
base scenes and read by eighty, a designer wants the data by path: every path the stack
holds, grouped by prefix, with its value and layer chain, whichever scene is open. The
`reference --json` command already has the list; the dock needs a view over it. The
inspector plugin's per-property chain stays useful on the base scenes.

### G10. Smaller points

- **A per-binding change signal.** The game copies `max_hp` into `hp` at spawn; a live
  edit of `max_hp` changes the ceiling and not the current value. The global `changed`
  signal fires; a `property_changed(name)` on the binding lets the unit re-derive what it
  copied, which is the cookbook's "migrate or punt" at the right granularity.
- **Translations** are outside Baker's scope; a tweak's `desc` is for tools, not players.
- **The capture rule** ("code that changes a bound property at runtime is taken for a
  tuner") holds for this game by luck: stat modifiers are kept apart from the exported
  stats. Document it as a requirement on the host's code.

## 4. What a port would look like

A sketch, in Baker's format, of the files this game would gain; written here as a design,
not taken from the game.

```dta
; baker-project.dta
(project
   (layers
      (default "data/rules.dta")               ; #include units, buildings, abilities, ai
      (steamdeck "data/platform/steamdeck.dta" disabled)   ; enabled when detected
      (local "user://baker/local.dta" optional dev_only)
      (agent "data/agent.dta" optional dev_only)
      (cheap_air "data/experiments/cheap_air.dta" disabled))
   (scenarios "sim/scenarios")
   (objectives "sim/objectives"))
```

```dta
; data/units.dta: one record per unit class, read by every faction's scene.
(units
   (infantry  (hp 10) (move 4) (attack 6 (range 0 20)) (armor 0) (price 25) (captures #t))
   (tank      (hp 10) (move 6) (attack 10) (armor 2) (price 50))
   (heli      (hp 10) (move 7) (attack 8) (armor 1) (price 70) (flies #t)))
(buildings
   (barracks (income 5) (capture_value 70) (crew #t))
   (hq       (income 20) (capture_value 1000) (crew #t))
   (tower    (income 15) (crew #f)))
(rules
   (exp_per_level 2) (max_level 3)
   (level_bonus (armor_at 2) (move_at 3))
   (hero_level_bonus (armor_at 1) (move_at 2))
   (ap_cap 999) (collateral_chance 0.5))
(ai
   (units_hard_limit 5) (hard_limit_per_building 1.3) (units_soft_limit 10)
   (kill_bonus 100) (retaliation_penalty 10) (counter_death_penalty 20))
```

```dta
; data/experiments/cheap_air.dta
(units (heli (price 60)) (scout (price 40)))
```

```dta
; sim/scenarios/economy.dta
(scenario hq_affords_tank (desc "An HQ and a factory afford a tank by turn 3")
   (seconds 3)                            ; by convention: one step per turn (G7)
   (start {board.load "2p_oasis"} {board.set_ai_off})
   (objectives economy.tank_by_turn_3))
```

The binding side is one node in the base unit scene and one in the base building scene,
with placeholder paths (G2) and shared tweaks (G1). The AI's brains read `ai.*` through
`Baker.tweak` once per turn. The settings menu keeps its UI and reads and writes the
`local` layer. Multiplayer exchanges `Baker.fingerprint()` in the lobby and locks the
runtime for the match (G5).

## 5. Candidate requirements for Baker

Nothing above needs Baker to change before the adapter can serve this game; G1, G2, G3, G4,
G6, G8 and G9 are adapter work, and the lock in G5 and the turn convention in G7 are too.
Two items would be better done once in Baker, and the owner may want them as notes in
`requirements/`, in Baker's terms:

1. **A digest of a Config's effective data.** A stable hash over the merged tree (or over
   the enabled layers' content and order), the same on every machine and build that holds
   the same data, cheap enough to compute at a session boundary and short enough to store
   in a save or exchange between peers. Hosts that run the same rules on several machines
   (lockstep multiplayer, replays, dedicated servers) detect a divergence before it
   silently decides a match; a save records what data it was made under.
2. **Scenario time in host-defined steps, and a run that ends on a condition.** A host
   whose simulation advances in discrete turns rather than seconds names its steps so, and
   writes `(at ...)` in them; a scenario may end when a command's value is nonzero (a side
   has lost) rather than after a fixed count, with the count as a ceiling. This is every
   turn-based and every event-stepped game.

Requirements 0014 (allowed values for string and integer tweaks) and 0016 (the project's
tool entry with arguments), already with Baker, are confirmed by this game.

## 6. Recommended order

1. **Shared tweaks and placeholder paths** (G1, G2): both change `BakerBinding` only, and
   together they make a game with many instances of few types bindable in two scenes.
2. **Keyed tables as Dictionaries** (G3), with shapes emitted from a class as the check.
3. **Runtime layers** (G6) and **the match lock and fingerprint** (G5); the fingerprint as
   the adapter's own hash over layer files until Baker offers a digest.
4. **Script calls from GDScript** (G4).
5. **A project-wide data view in the dock** (G9).
6. **A standalone tree read** (G8), when a game wants Baker's format for its content.

A second sample, smaller than this game but turn-based with a table of unit types, several
instances per type and two peers, would exercise 1 to 4 the way the platformer exercises
the first release.

## Evidence

Read in the game's repository, master branch, October 2026:

- Unit stats: `scenes/tiles/units/unit.gd`, `scenes/tiles/units/templates/*.tscn`,
  `scenes/tiles/units/{blue,red,green,yellow}/*.tscn`, `scenes/tiles/units/heroes/hero.tscn`.
- Buildings and prices: `scenes/tiles/buildings/*/*.tscn`, `scenes/abilities/production/*/*.tscn`.
- Rules: `scenes/board/board.gd` (`battle`, `capture`, `gain_building_ap`),
  `scenes/board/logic/state.gd`, `scenes/board/logic/collateral.gd`, `scenes/abilities/`.
- AI: `scenes/board/logic/ai/ai.gd`, `ai/brains/abstract_unit_brain.gd`,
  `ai/brains/abstract_building_brain.gd`, `ai/brains/heroes/*.gd`.
- Settings and persistence: `scripts/services/settings.gd`, `saves_manager.gd`,
  `campaign.gd`, `map_manager.gd`, `filesystem.gd`.
- Scripted missions: `scenes/board/logic/scripting.gd`, `scripting/triggers/`,
  `scripting/outcomes/`, `docs/MANUAL.md`, `assets/campaigns/*/maps/*.tofmap.json`.
- Multiplayer: `scenes/board_multiplayer/board_multiplayer.gd`, `scenes/board_online/board_online.gd`.
- Balance history: `docs/CHANGELOG.md`.
