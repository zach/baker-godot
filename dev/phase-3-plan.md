# Phase 3: C#, on Godot 4.7

Larger Godot simulations are written in C#, and the games assessed for phase 2 read their
tunables from worker threads during a simulation update. Today the integration speaks
GDScript and C++: a C# game can call the `Baker` autoload, but untyped, through Godot's
Variant on every read, and only on the main thread. Phase 3 gives C# games a typed package,
and values a threaded simulation reads without a call into the engine, proven on the
skirmish ported to C#.

## Decisions

- **Godot 4.7.** The integration moves from 4.5 to 4.7 (4.7.2, the latest stable), the
  version the games in use are on, and stays on one version line. godot-cpp no longer tags
  a release per Godot version: its 10.0.0 targets 4.3 to 4.7 by an API version, which the
  build sets to 4.7.
- **The skirmish, ported to C#.** The same data, rules, scenarios and objectives, so `sim`
  and `compare` judge the C# port against the GDScript skirmish on the same scenarios; the
  platformer stays GDScript.
- **A NuGet package.** The C# API ships as a NuGet package (`Baker.Godot`), the way C#
  projects take dependencies; the native extension stays the addon, which a NuGet package
  cannot place in a Godot project. The release workflow builds the `.nupkg` and attaches it
  to the draft release; publishing it to nuget.org is the owner's step.
- **No macOS.** CI's macOS job stays paused; phase 3 is built and verified on Linux and
  Windows.

## Steps

| Step | Delivers | Proven by |
|---|---|---|
| 1. Godot 4.7 | godot-cpp 10.0.0 at API 4.7; the sample, CI, the export templates and the docs on 4.7.2 | Every suite green on 4.7.2, Linux and Windows |
| 2. The C# package | `Baker.Godot`: typed reads (`Baker.Get<float>(path, fallback)`), tables as dictionaries and records, layers, live edits, `CallFunction<T>`, records, lockstep and bindings from C#, over the runtime's methods; a `.nupkg` built in CI and by the release workflow | C# tests run headless in the Godot .NET build, as the runtime test does |
| 3. Published values for threads | C# code declares the paths it reads from threads; at the frame's start (where a reload lands, phase 2 step 4) the runtime publishes them into an immutable managed snapshot swapped by one reference, which worker threads read with no engine call; a read's, a publish's and a reload's cost written down | Workers reading every tick see a reload, a toggle or a live edit whole, at the next tick, never half; the costs measured in a benchmark |
| 4. The skirmish in C# | The match, the rules' callers, the catalog and the cards in C#, the economy stepped on worker threads; the same scenarios and objectives | `sim` over seeds 1-5 gives the GDScript skirmish's results; `compare` between the two shows nothing moved |
| 5. Tooling and agents | CI with the .NET build; the export of a C# game; the agent skill and README for C# | `export.sh` on the C# sample; the evaluation's skirmish tasks rerun on the port |

## Results

**Step 3, published values** (`tests/cs_bench.tscn`, Linux x86_64, Godot 4.7.2 .NET, a
debug build of the extension, 2026-10-31):

| | |
|---|---|
| a read on a worker thread (`values.Current.Get(h)`, the snapshot taken each read) | 8.8 ns |
| the same read through the autoload, on the main thread (`Baker.Get`) | 8,074 ns |
| reads a second, four worker threads, the snapshot taken once a thousand reads | 639 million |
| a publish of every value the sample's data holds (60) | 0.68 ms |
| frames from a live edit to the threads | 1 (the next frame's start) |

A publish reads each declared value through the autoload, so it costs what that many
`Baker.Get` calls cost, once a frame and only after a change; a reader pays nothing for it.
The C# test runs four threads reading two values the data keeps equal while both change
each frame, as a layer's text and as two live edits: over millions of reads, none saw one
without the other, and every change reached the threads at the next frame's start.

**Step 4, the skirmish in C#:** `sim` over seeds 1-5 on each gives the same 30 runs,
outcome by outcome (the economy's army on turns 9, 9, 10, 9, 11 in both), and `compare`
moves nothing; a dearer tank in the agent's layer fails both the same. The sides' turns run
on two worker threads from one snapshot a turn; the random draws use the engine's generator
per side, seeded as the GDScript match seeds it, which the port calls from the worker.

**Step 5, tooling and agents:** CI runs every suite in Godot 4.7.2's .NET build and packs
the package (step 2); `export.sh` exports the C# sample with the .NET templates (step 2);
the evaluation's six skirmish tasks rerun on the port give 18 of 18 ($2.54), the agents'
fixes those of the GDScript round, with `--skirmish cs` in every command. Phase 3 is done.

## With Baker

Probably nothing new: the snapshot is built from what the runtime already reads. If a
publish cannot be made cheap enough from outside Baker, that becomes a requirement note.
Note 0021 (tweaks the data may leave out) is on Baker's main, for its next release.
