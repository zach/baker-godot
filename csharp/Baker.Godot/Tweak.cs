using System;
using Godot;

namespace BakerGodot;

/// <summary>
/// A value at a path, read once and again after every change of the data, so a read is a
/// field: for main-thread code that reads a value every frame. Dispose it to stop following.
/// </summary>
public sealed class Tweak<[MustBeVariant] T> : IDisposable
{
    readonly Action _refresh;

    /// <summary>Reads <paramref name="path"/> now, as <paramref name="fallback"/>'s type.</summary>
    public Tweak(string path, T fallback)
    {
        Path = path;
        Fallback = fallback;
        Value = Baker.Get(path, fallback);
        _refresh = () => Value = Baker.Get(Path, Fallback);
        Baker.Changed += _refresh;
    }

    /// <summary>The path read.</summary>
    public string Path { get; }

    /// <summary>The value when the data lacks the path.</summary>
    public T Fallback { get; }

    /// <summary>The value, as of the last change of the data.</summary>
    public T Value { get; private set; }

    /// <summary>The value.</summary>
    public static implicit operator T(Tweak<T> tweak) => tweak.Value;

    /// <summary>Stops following the data.</summary>
    public void Dispose() => Baker.Changed -= _refresh;
}
