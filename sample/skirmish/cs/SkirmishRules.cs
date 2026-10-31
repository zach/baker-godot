using System;
using BakerGodot;

namespace BakerSample;

/// <summary>The skirmish's rules, which the data defines in Baker's script, read with the
/// unit table (skirmish/rules.gd's port): a layer that defines a rule again changes what
/// these return. Main thread only: a rule is a call into Baker's script.</summary>
public static class SkirmishRules
{
    /// <summary>The damage of one hit of <paramref name="attacker"/> on
    /// <paramref name="defender"/>, at the attacker's <paramref name="level"/>.</summary>
    public static int Damage(string attacker, string defender, int level = 0)
    {
        var units = SkirmishCatalog.Units();
        return Baker.CallFunction<int>("skirmish.damage",
            units[attacker].AsGodotDictionary()["attack"].AsInt32(),
            units[defender].AsGodotDictionary()["armor"].AsInt32(), level);
    }

    /// <summary>How many hits of <paramref name="attacker"/> destroy a fresh
    /// <paramref name="defender"/>.</summary>
    public static int HitsToKill(string attacker, string defender, int level = 0)
    {
        int hp = SkirmishCatalog.Units()[defender].AsGodotDictionary()["hp"].AsInt32();
        return (int)Math.Ceiling((double)hp / Damage(attacker, defender, level));
    }
}
