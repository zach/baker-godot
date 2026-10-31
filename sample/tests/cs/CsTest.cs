using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using BakerGodot;
using Godot;

namespace BakerSample;

/// <summary>
/// The C# package's tests, run headless as the runtime test is:
///
///   godot --headless --path sample res://tests/cs_test.tscn
///
/// Prints one line per check and quits with the number of failures.
/// </summary>
public partial class CsTest : Node
{
    int _failures;

    void Check(bool ok, string what)
    {
        GD.Print((ok ? "ok   " : "FAIL ") + what);
        if (!ok)
            _failures++;
    }

    public override async void _Ready()
    {
        await Run();
        GD.Print($"{_failures} failure(s)");
        GetTree().Quit(_failures);
    }

    async Task Frames(int n)
    {
        for (int i = 0; i < n; i++)
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
    }

    async Task Run()
    {
        Check(Baker.Runtime != null, "the Baker autoload, from C#");
        Check(Baker.Get("player.jump.height", 0f) == 96f, "Get<float>: the data's value, typed");
        Check(Baker.Get("units.tank.price", 0) == 50, "Get<int>: a record's field");
        Check(Baker.Get("no.such.path", 7) == 7, "  a missing path gives the fallback");
        var units = Baker.GetTable("units");
        Check(units.Count == 4 && units["tank"].AsGodotDictionary()["price"].AsInt32() == 50,
            $"GetTable: the unit table, by kind: {units.Count} kinds");
        Check(Baker.GetList("ai.opening").Count == 3, "GetList: the AI's opening");
        Check(Baker.Source("player.jump.height")["file"].AsString() == "data/tuning.dta",
            "Source: file and line");

        int changes = 0;
        void Count() => changes++;
        Baker.Changed += Count;
        using var height = new Tweak<float>("player.jump.height", 0f);
        Check(Baker.Set("player.jump.height", 111f) == Error.Ok, "Set: a live edit");
        Check(height.Value == 111f && changes >= 1,
            $"  a Tweak<float> follows it, and Changed fires: {height.Value}, {changes}");
        Check(System.Array.IndexOf(Baker.Unsaved(), "player/jump/height") >= 0, "  Unsaved lists it");
        Baker.ClearScratch();
        Check(height.Value == 96f, "  ClearScratch: back to the data's");
        Baker.Changed -= Count;

        Check(Baker.CallFunction<int>("skirmish.damage", 6, 2, 0) == 4,
            "CallFunction<int>: a rule the data defines");
        Check(Baker.ToggleLayer("piercing"), "ToggleLayer: piercing on");
        await Frames(3);
        Check(Baker.CallFunction<int>("skirmish.damage", 6, 2, 0) == 5,
            "  the rule the layer defines");
        Baker.SetLayerEnabled("piercing", false);
        await Frames(3);
        Check(Baker.CallFunction<int>("skirmish.damage", 6, 2, 0) == 4, "  and back");
        Check(Baker.Eval("{+ 1 2}").AsInt32() == 3, "Eval: script text");

        Check(Baker.PushLayerText("lobby", "(match (turn_seconds 45))") == Error.Ok
            && Baker.Get("match.turn_seconds", 0) == 45, "PushLayerText: a received layer");
        Check(Baker.LayerText("lobby").Contains("turn_seconds 45"), "  LayerText: to send");
        Baker.RemoveLayer("lobby");

        Check(Baker.BindRecords("cards", "res://skirmish/cards") == 40, "BindRecords: forty cards");
        var strike = Baker.Record<Resource>("cards", "strike");
        Check(strike != null && strike.Get("cost").AsInt32() == 1, "  Record: the card's resource");
        Check(Baker.Record<Resource>("cards", "fireball")!.Get("cost").AsInt32() == 2,
            "  with the data's change");

        string print = Baker.Fingerprint();
        Check(Baker.Lock() == print, "Lock: the fingerprint");
        Baker.ToggleLayer("floaty");
        Check(Baker.ApplyHeld() != print, "  ApplyHeld: the toggle, at the boundary");
        Baker.ToggleLayer("floaty");
        Baker.Unlock();
        Check(Baker.Fingerprint() == print, "  Unlock: and back");

        await Threads();
    }

    // Values worker threads read: published at the frame's start, whole.
    async Task Threads()
    {
        Baker.PushLayerText("pair", "(pair (x 0) (y 0))");
        using var values = new PublishedValues();
        var x = values.Declare("pair.x", -1);
        var y = values.Declare("pair.y", -1);
        var units = values.DeclareTable("units");
        var first = values.Current;
        Check(first.Get(x) == 0 && first.Get(y) == 0, "PublishedValues: declared values, published");
        Check((long)first.Get(units)["tank"]["price"] == 50,
            "  a table, whole, as .NET values: the tank's price");

        long reads = 0, torn = 0, lastSeen = 0;
        var versions = new HashSet<long>();
        using var stop = new CancellationTokenSource();
        var workers = new List<Task>();
        for (int w = 0; w < 4; w++)
            workers.Add(Task.Run(() =>
            {
                long mine = 0, mineTorn = 0;
                var seen = new HashSet<long>();
                while (!stop.IsCancellationRequested)
                {
                    var now = values.Current;  // once a tick
                    if (now.Get(x) != now.Get(y))
                        mineTorn++;
                    seen.Add(now.Version);
                    mine++;
                }
                Interlocked.Add(ref reads, mine);
                Interlocked.Add(ref torn, mineTorn);
                lock (versions)
                    versions.UnionWith(seen);
            }));
        // Each frame both values change: as a layer's text, then as two live edits.
        for (int i = 1; i <= 40; i++)
        {
            if (i % 2 == 1)
            {
                Baker.ClearScratch();  // the last frame's live edits would stand over the layer
                Baker.SetLayerText("pair", $"(pair (x {i}) (y {i}))");
            }
            else
            {
                Baker.Set("pair.x", i);
                Baker.Set("pair.y", i);
            }
            await Frames(1);
            if (values.Current.Get(x) == i)
                lastSeen++;
        }
        stop.Cancel();
        await Task.WhenAll(workers);
        Check(torn == 0 && reads > 1000,
            $"  four threads read {reads} times while both values changed each frame: never one without the other ({torn})");
        Check(versions.Count >= 20, $"  and saw the publishes: {versions.Count} versions");
        Check(lastSeen == 40, $"  each change published at the next frame's start: {lastSeen} of 40");
        long before = values.Current.Version;
        await Frames(3);
        Check(values.Current.Version == before, "  a quiet frame publishes nothing");
        Baker.ClearScratch();
        Baker.RemoveLayer("pair");
    }
}
