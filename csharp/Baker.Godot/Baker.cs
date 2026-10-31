using System;
using Godot;
using Godot.Collections;

namespace BakerGodot;

/// <summary>
/// Baker from C#: typed calls over the <c>Baker</c> autoload (a <c>BakerRuntime</c>, the
/// addon's native extension). Every call is made on the main thread, as the autoload's own
/// are; values that worker threads read come from a published snapshot instead.
/// </summary>
public static class Baker
{
    /// <summary>The autoload, or null before the scene tree has it.</summary>
    public static GodotObject? Runtime =>
        (Engine.GetMainLoop() as SceneTree)?.Root.GetNodeOrNull("Baker");

    static GodotObject Required =>
        Runtime ?? throw new InvalidOperationException(
            "Baker: no Baker autoload; enable the Baker for Godot addon");

    // --- Reading. ---

    /// <summary>The value at <paramref name="path"/>, read as the fallback's type; the
    /// fallback if the path is missing or does not convert.</summary>
    public static T Get<[MustBeVariant] T>(string path, T fallback) =>
        Required.Call("get_value", path, Variant.From(fallback)).As<T>();

    /// <summary>A keyed table, whole: each record a Dictionary of its fields; empty if the
    /// path is not a table.</summary>
    public static Dictionary GetTable(string path) =>
        Required.Call("get_value", path, new Dictionary()).AsGodotDictionary();

    /// <summary>A list of values, or of records of one tag.</summary>
    public static Godot.Collections.Array GetList(string path) =>
        Required.Call("get_value", path, new Godot.Collections.Array()).AsGodotArray();

    /// <summary>Where the value comes from: {layer, file, line}; empty if missing.</summary>
    public static Dictionary Source(string path) => Required.Call("source", path).AsGodotDictionary();

    /// <summary>Every layer that has a say in the value, bottom to top.</summary>
    public static Godot.Collections.Array Explain(string path) =>
        Required.Call("explain", path).AsGodotArray();

    // --- Live edits. ---

    /// <summary>Sets the value in the scratch layer, unsaved; bound properties follow.</summary>
    public static Error Set<[MustBeVariant] T>(string path, T value) =>
        (Error)Required.Call("set_value", path, Variant.From(value)).AsInt64();

    /// <summary>Drops a live edit, or every one when <paramref name="path"/> is empty.</summary>
    public static Error ClearScratch(string path = "") =>
        (Error)Required.Call("clear_scratch", path).AsInt64();

    /// <summary>The paths edited live and not kept.</summary>
    public static string[] Unsaved() => Required.Call("unsaved").AsStringArray();

    // --- Layers. ---

    /// <summary>The layers, bottom to top: [{name, enabled}].</summary>
    public static Godot.Collections.Array Layers() => Required.Call("get_layers").AsGodotArray();

    /// <summary>Turns a layer on or off.</summary>
    public static Error SetLayerEnabled(string layer, bool enabled) =>
        (Error)Required.Call("set_layer_enabled", layer, enabled).AsInt64();

    /// <summary>Toggles a layer; its new state.</summary>
    public static bool ToggleLayer(string layer) => Required.Call("toggle_layer", layer).AsBool();

    /// <summary>A layer from a file the game finds after it starts.</summary>
    public static Error PushLayer(string name, string path, LayerOptions? options = null) =>
        (Error)Required.Call("push_layer", name, path, (options ?? new()).ToDictionary()).AsInt64();

    /// <summary>A layer from text the game received; an #include in it is refused.</summary>
    public static Error PushLayerText(string name, string text, LayerOptions? options = null) =>
        (Error)Required.Call("push_layer_text", name, text, (options ?? new()).ToDictionary()).AsInt64();

    /// <summary>A directory of layers: a layer for each .dta file, and each folder with a
    /// layer.dta.</summary>
    public static Error PushLayerDir(string name, string dir, LayerOptions? options = null) =>
        (Error)Required.Call("push_layer_dir", name, dir, (options ?? new()).ToDictionary()).AsInt64();

    /// <summary>Removes a layer, leaving no trace.</summary>
    public static Error RemoveLayer(string name) => (Error)Required.Call("remove_layer", name).AsInt64();

    /// <summary>A layer as text, for a peer.</summary>
    public static string LayerText(string name) => Required.Call("layer_text", name).AsString();

    /// <summary>A text layer's text, replaced in place.</summary>
    public static Error SetLayerText(string name, string text) =>
        (Error)Required.Call("set_layer_text", name, text).AsInt64();

    /// <summary>Re-reads the files that changed; true if anything did.</summary>
    public static bool Reload() => Required.Call("reload").AsBool();

    // --- Rules and script. ---

    /// <summary>Calls a script function (a rule the data defines, or one a script binds)
    /// and reads its result as <typeparamref name="T"/>.</summary>
    public static T CallFunction<[MustBeVariant] T>(string name, params Variant[] args) =>
        Required.Call("call_function", name, new Godot.Collections.Array(args)).As<T>();

    /// <summary>Evaluates Baker script text, as the console does.</summary>
    public static Variant Eval(string text) => Required.Call("eval", text);

    /// <summary>Binds a function as Baker's script function <paramref name="name"/>.</summary>
    public static void BindFunction(string name, Callable function) =>
        Required.Call("bind_function", name, function);

    // --- Records. ---

    /// <summary>Binds every resource in <paramref name="dir"/> as a record of the table
    /// <paramref name="prefix"/>; the number bound.</summary>
    public static int BindRecords(string prefix, string dir, string key = "", string group = "Tuning")
    {
        var options = new Dictionary { ["group"] = group };
        if (key.Length > 0)
            options["key"] = key;
        return (int)Required.Call("bind_records", prefix, dir, options).AsInt64();
    }

    /// <summary>A record by its key, or null.</summary>
    public static T? Record<T>(string prefix, string key) where T : Resource =>
        Required.Call("record", prefix, key).As<Resource>() as T;

    // --- A match in lockstep. ---

    /// <summary>The data as the game reads it, as sixteen hex digits.</summary>
    public static string Fingerprint() => Required.Call("fingerprint").AsString();

    /// <summary>Locks the data for a match; its fingerprint.</summary>
    public static string Lock() => Required.Call("lock").AsString();

    /// <summary>At a turn's boundary: applies what waited; the new fingerprint.</summary>
    public static string ApplyHeld() => Required.Call("apply_held").AsString();

    /// <summary>Applies what waited, and unlocks.</summary>
    public static void Unlock() => Required.Call("unlock");

    // --- Signals. ---

    /// <summary>After every change of the data: a reload, a layer toggled, a live edit.</summary>
    public static event Action Changed
    {
        add => Required.Connect("changed", Callable.From(value));
        remove => Required.Disconnect("changed", Callable.From(value));
    }

    /// <summary>The runtime's last error message.</summary>
    public static string LastError => Required.Call("get_last_error").AsString();
}

/// <summary>Where a layer goes, and how: push_layer's options.</summary>
public sealed record LayerOptions(bool Optional = false, bool Enabled = true, string Below = "", string Above = "")
{
    internal Dictionary ToDictionary() => new()
    {
        ["optional"] = Optional,
        ["enabled"] = Enabled,
        ["below"] = Below,
        ["above"] = Above,
    };
}
