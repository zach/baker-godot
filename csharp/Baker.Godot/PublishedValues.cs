using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Threading;
using Godot;

namespace BakerGodot;

/// <summary>
/// Values a threaded simulation reads without a call into the engine. On the main thread,
/// declare the paths the threads read; the values are published as an immutable
/// <see cref="Snapshot"/> at the start of each frame after the data changed (where the
/// runtime lands a reload, before the frame's physics and process steps), all of a frame's
/// changes at once, by swapping one reference. A worker takes <see cref="Current"/> once a
/// tick and reads it: an array index, no engine call, no allocation, and never part of a
/// reload.
/// </summary>
/// <example><code>
/// var values = new PublishedValues();
/// var speed = values.Declare("units.tank.move", 6);
/// var units = values.DeclareTable("units");
/// // on any thread, once a tick:
/// var now = values.Current;
/// int move = now.Get(speed);
/// </code></example>
public sealed class PublishedValues : IDisposable
{
    readonly List<Func<object>> _readers = new();
    readonly SceneTree _tree;
    readonly Callable _onFrame;
    readonly Action _onChanged;
    Snapshot _current = new(Array.Empty<object>(), 0);
    bool _dirty;
    long _version;

    /// <summary>Starts following the data. On the main thread, with the Baker autoload in
    /// the tree.</summary>
    public PublishedValues()
    {
        _tree = Engine.GetMainLoop() as SceneTree
            ?? throw new InvalidOperationException("Baker: no scene tree");
        _onFrame = Callable.From(PublishIfChanged);
        // After the runtime's own handlers, which connected first: a reload has landed.
        _tree.Connect(SceneTree.SignalName.PhysicsFrame, _onFrame);
        _tree.Connect(SceneTree.SignalName.ProcessFrame, _onFrame);
        _onChanged = () => _dirty = true;
        Baker.Changed += _onChanged;
    }

    /// <summary>The values as last published: take it once a tick, on any thread.</summary>
    public Snapshot Current => Volatile.Read(ref _current);

    /// <summary>A value the threads read, as <paramref name="fallback"/>'s type; published at
    /// once with the values declared before it.</summary>
    public Published<T> Declare<[MustBeVariant] T>(string path, T fallback)
    {
        return new Published<T>(Add(() => Baker.Get(path, fallback)!), path);
    }

    /// <summary>A keyed table the threads read whole, as .NET values: a record is an
    /// <c>IReadOnlyDictionary&lt;string, object&gt;</c> of its fields (long, double, string,
    /// bool, lists and tables of them), a table of scalars their values.</summary>
    public Published<IReadOnlyDictionary<string, object>> DeclareTable(string path) =>
        new(Add(() => ToDotNet(Baker.GetTable(path))), path);

    /// <summary>A list the threads read whole, as .NET values.</summary>
    public Published<IReadOnlyList<object>> DeclareList(string path) =>
        new(Add(() => ToDotNet(Baker.GetList(path))), path);

    int Add(Func<object> reader)
    {
        _readers.Add(reader);
        Publish();
        return _readers.Count - 1;
    }

    void PublishIfChanged()
    {
        if (!_dirty)
            return;
        _dirty = false;
        Publish();
    }

    /// <summary>Publishes now, outside the frame's start: after declaring, or in a test.</summary>
    public void Publish()
    {
        var values = new object[_readers.Count];
        for (int i = 0; i < values.Length; i++)
            values[i] = _readers[i]();
        Volatile.Write(ref _current, new Snapshot(values, ++_version));
    }

    /// <summary>Stops following the data; the last snapshot stays readable.</summary>
    public void Dispose()
    {
        Baker.Changed -= _onChanged;
        if (_tree.IsConnected(SceneTree.SignalName.PhysicsFrame, _onFrame))
            _tree.Disconnect(SceneTree.SignalName.PhysicsFrame, _onFrame);
        if (_tree.IsConnected(SceneTree.SignalName.ProcessFrame, _onFrame))
            _tree.Disconnect(SceneTree.SignalName.ProcessFrame, _onFrame);
    }

    /// <summary>A Variant as a .NET value no engine call reads: a number, a string, a bool,
    /// a list or a table of them.</summary>
    static object ToDotNet(Variant v)
    {
        switch (v.VariantType)
        {
            case Variant.Type.Bool: return v.AsBool();
            case Variant.Type.Int: return v.AsInt64();
            case Variant.Type.Float: return v.AsDouble();
            case Variant.Type.String:
            case Variant.Type.StringName: return v.AsString();
            case Variant.Type.Array:
            {
                var list = new List<object>();
                foreach (var item in v.AsGodotArray())
                    list.Add(ToDotNet(item));
                return list.AsReadOnly();
            }
            case Variant.Type.Dictionary:
            {
                // In the data's order: a Dictionary enumerates in insertion order while
                // nothing is removed, and nothing is.
                var d = new Dictionary<string, object>();
                foreach (var (key, value) in v.AsGodotDictionary())
                    d[key.AsString()] = ToDotNet(value);
                return new ReadOnlyDictionary<string, object>(d);
            }
            default: return v.ToString();
        }
    }
}

/// <summary>A declared value: its place in every <see cref="Snapshot"/>.</summary>
public readonly struct Published<T>
{
    internal Published(int index, string path)
    {
        Index = index;
        Path = path;
    }

    internal int Index { get; }

    /// <summary>The path it reads.</summary>
    public string Path { get; }
}

/// <summary>The declared values at one publish: immutable, safe to read on any thread.</summary>
public sealed class Snapshot
{
    readonly object[] _values;

    internal Snapshot(object[] values, long version)
    {
        _values = values;
        Version = version;
    }

    /// <summary>Counts up at each publish.</summary>
    public long Version { get; }

    /// <summary>A declared value, as of this publish.</summary>
    public T Get<T>(Published<T> value) => (T)_values[value.Index];
}
