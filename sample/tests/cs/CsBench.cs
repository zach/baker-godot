using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using BakerGodot;
using Godot;

namespace BakerSample;

/// <summary>
/// What published values cost, measured headless:
///
///   godot --headless --path sample res://tests/cs_bench.tscn
///
/// A read on a worker thread, against a read through the autoload on the main thread; a
/// publish of every value the data holds; and how many frames a change takes to reach the
/// threads.
/// </summary>
public partial class CsBench : Node
{
    public override async void _Ready()
    {
        using var values = new PublishedValues();
        var height = values.Declare("player.jump.height", 0f);

        const int reads = 20_000_000;
        var clock = Stopwatch.StartNew();
        float sum = await Task.Run(() =>
        {
            float s = 0;
            for (int i = 0; i < reads; i++)
                s += values.Current.Get(height);
            return s;
        });
        double perRead = clock.Elapsed.TotalNanoseconds / reads;

        const int calls = 20_000;
        clock.Restart();
        float viaEngine = 0;
        for (int i = 0; i < calls; i++)
            viaEngine += Baker.Get("player.jump.height", 0f);
        double perCall = clock.Elapsed.TotalNanoseconds / calls;

        long threads = 0;
        using var stop = new CancellationTokenSource();
        var four = new Task[4];
        for (int w = 0; w < 4; w++)
            four[w] = Task.Run(() =>
            {
                long n = 0;
                while (!stop.IsCancellationRequested)
                {
                    var now = values.Current;
                    for (int i = 0; i < 1000; i++)
                        if (now.Get(height) > 0)
                            n++;
                }
                Interlocked.Add(ref threads, n);
            });
        clock.Restart();
        await Task.Delay(500);
        stop.Cancel();
        await Task.WhenAll(four);
        double perSecond = threads / clock.Elapsed.TotalSeconds;

        // Every value in the data, declared: what a publish costs at that size.
        var all = new PublishedValues();
        int declared = 0;
        foreach (var e in Baker.Runtime!.Call("data_paths").AsGodotArray())
        {
            all.Declare(e.AsGodotDictionary()["path"].AsString(), 0.0);
            declared++;
        }
        const int publishes = 200;
        clock.Restart();
        for (int i = 0; i < publishes; i++)
            all.Publish();
        double perPublish = clock.Elapsed.TotalMilliseconds / publishes;
        all.Dispose();

        long before = values.Current.Version;
        Baker.Set("player.jump.height", 101f);
        int frames = 0;
        while (values.Current.Version == before && frames < 10)
        {
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
            frames++;
        }
        Baker.ClearScratch();

        GD.Print($"read, a worker thread: {perRead:F2} ns ({sum > 0})");
        GD.Print($"read, through the autoload, main thread: {perCall:F0} ns ({viaEngine > 0})");
        GD.Print($"reads a second, four threads: {perSecond / 1e6:F0} million");
        GD.Print($"publish, {declared} values: {perPublish:F3} ms");
        GD.Print($"a change reaches the threads in: {frames} frame(s)");
        GetTree().Quit();
    }
}
