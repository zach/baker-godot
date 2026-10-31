using System.Linq;
using BakerGodot;
using Godot.Collections;

namespace BakerSample;

/// <summary>The skirmish's unit and building types, read whole from the data
/// (data/skirmish.dta), as skirmish/catalog.gd reads them: the C# port.</summary>
public static class SkirmishCatalog
{
    /// <summary>{kind: {hp, move, attack, armor, price, ...}}.</summary>
    public static Dictionary Units() => Baker.GetTable("units");

    /// <summary>{kind: {income, capture, ...}}.</summary>
    public static Dictionary Buildings() => Baker.GetTable("buildings");

    /// <summary>The unit kinds <paramref name="points"/> can build, cheapest first.</summary>
    public static string[] Affordable(int points)
    {
        var table = Units();
        return table.Keys.Select(k => k.AsString())
            .Where(k => table[k].AsGodotDictionary()["price"].AsInt32() <= points)
            .OrderBy(k => table[k].AsGodotDictionary()["price"].AsInt32())
            .ToArray();
    }

    /// <summary>What the AI builds first, in order.</summary>
    public static string[] Opening() =>
        Baker.GetList("ai.opening").Select(v => v.AsString()).ToArray();
}
