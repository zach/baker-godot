using System.Collections.Generic;
using System.Threading.Tasks;
using BakerGodot;
using Godot;

namespace BakerSample;

/// <summary>
/// The skirmish's economy in C#, the port of skirmish/match.gd: a turn a physics tick once
/// a scenario starts a match, each side earning its buildings' income less its units'
/// upkeep, building its opening, then buying one thing or saving by its seed and
/// ai.weights. The two sides' turns run on worker threads, in parallel, reading the data
/// from a PublishedValues snapshot taken once a turn: no call into the engine on a worker,
/// and a change (a live edit, a layer) reaches both sides whole at the next turn. The
/// same script functions as the GDScript match, so the same scenarios and objectives
/// judge it: <c>-- --skirmish cs</c> selects it (main.gd).
/// </summary>
public partial class SkirmishMatch : Node
{
    const int Never = 99;

    sealed class Side
    {
        public int Points;
        public readonly List<string> Units = new();
        public readonly List<string> Buildings = new();
        public int Opening;
        public readonly RandomNumberGenerator Rng = new();
        public readonly Dictionary<string, int> BuiltAt = new();  // the first turn building each kind
    }

    readonly Dictionary<string, Side> _sides = new();
    bool _running;
    int _turn;
    PublishedValues? _values;
    Published<IReadOnlyDictionary<string, object>> _units, _buildings, _weights;
    Published<IReadOnlyList<object>> _opening, _startBuildings;
    Published<int> _startPoints, _armyReady;

    public override void _Ready()
    {
        _values = new PublishedValues();
        _units = _values.DeclareTable("units");
        _buildings = _values.DeclareTable("buildings");
        _weights = _values.DeclareTable("ai.weights");
        _opening = _values.DeclareList("ai.opening");
        _startBuildings = _values.DeclareList("economy.buildings");
        _startPoints = _values.Declare("economy.start_points", 0);
        _armyReady = _values.Declare("economy.army_ready", 5);

        Baker.BindFunction("skirmish.start", Callable.From((int seed) => Start(seed)));
        Baker.BindFunction("skirmish.turn", Callable.From(() => _turn));
        Baker.BindFunction("skirmish.built_at", Callable.From((StringName kind) =>
            _sides.TryGetValue("blue", out var blue) && blue.BuiltAt.TryGetValue(kind, out int at) ? at : Never));
        Baker.BindFunction("skirmish.army", Callable.From((StringName side) =>
            _sides.TryGetValue(side, out var s) ? s.Units.Count : 0));
        Baker.BindFunction("skirmish.outcome", Callable.From(Outcome));
        Baker.BindFunction("skirmish.hits", Callable.From((StringName attacker, StringName defender, int level) =>
            SkirmishRules.HitsToKill(attacker, defender, level)));
        Baker.BindFunction("skirmish.card", Callable.From((StringName key, StringName field) => CardField(key, field)));
    }

    public override void _ExitTree() => _values?.Dispose();

    void Start(int seed)
    {
        _turn = 0;
        _sides.Clear();
        var now = _values!.Current;
        foreach (string side in new[] { "blue", "red" })
        {
            var s = new Side { Points = now.Get(_startPoints) };
            foreach (object b in now.Get(_startBuildings))
                s.Buildings.Add((string)b);
            s.Rng.Seed = (ulong)(seed * 2 + (side == "red" ? 1 : 0));
            _sides[side] = s;
        }
        _running = true;
    }

    public override void _PhysicsProcess(double delta)
    {
        if (!_running)
            return;
        _turn++;
        var now = _values!.Current;  // once a turn, for both sides
        var turns = new Task[_sides.Count];
        int i = 0;
        foreach (var (name, side) in _sides)
            turns[i++] = Task.Run(() => PlayTurn(side, now));
        Task.WaitAll(turns);
    }

    // One side's turn, on a worker thread: reads the snapshot and the side's own state only.
    void PlayTurn(Side s, Snapshot now)
    {
        var units = now.Get(_units);
        var buildings = now.Get(_buildings);
        var opening = now.Get(_opening);
        foreach (string b in s.Buildings)
            s.Points += (int)(long)Record(buildings, b)["income"];
        foreach (string u in s.Units)
            s.Points -= (int)(long)Record(units, u).GetValueOrDefault("upkeep", 0L);
        while (s.Opening < opening.Count)
        {
            string kind = (string)opening[s.Opening];
            int price = (int)(long)Record(units, kind)["price"];
            if (s.Points < price)
                break;
            Buy(s, kind, price, s.Units);
            s.Opening++;
        }
        if (s.Opening >= opening.Count)
            Choose(s, units, buildings, now.Get(_weights));
    }

    // One of what the side can afford, or nothing, by the weights and the side's seed.
    void Choose(Side s, IReadOnlyDictionary<string, object> units,
                IReadOnlyDictionary<string, object> buildings, IReadOnlyDictionary<string, object> weights)
    {
        var options = new List<(string kind, int cost, List<string>? list, double weight)>();
        foreach (var (kind, w) in weights)
        {
            double weight = w is long l ? l : (double)w;
            if (weight <= 0)
                continue;
            if (kind == "save")
                options.Add((kind, 0, null, weight));
            else if (units.ContainsKey(kind) && (long)Record(units, kind)["price"] <= s.Points)
                options.Add((kind, (int)(long)Record(units, kind)["price"], s.Units, weight));
            else if (buildings.ContainsKey(kind) && Record(buildings, kind).TryGetValue("cost", out var cost)
                     && (long)cost <= s.Points)
                options.Add((kind, (int)(long)cost, s.Buildings, weight));
        }
        double total = 0;
        foreach (var o in options)
            total += o.weight;
        if (total <= 0)
            return;
        double pick = s.Rng.Randf() * total;
        foreach (var o in options)
        {
            pick -= o.weight;
            if (pick < 0)
            {
                if (o.list != null)
                    Buy(s, o.kind, o.cost, o.list);
                return;
            }
        }
    }

    void Buy(Side s, string kind, int cost, List<string> list)
    {
        s.Points -= cost;
        list.Add(kind);
        s.BuiltAt.TryAdd(kind, _turn);
    }

    static IReadOnlyDictionary<string, object> Record(IReadOnlyDictionary<string, object> table, string key) =>
        (IReadOnlyDictionary<string, object>)table[key];

    Variant Outcome()
    {
        int army = _values!.Current.Get(_armyReady);
        return _sides.TryGetValue("blue", out var blue) && blue.Units.Count >= army
            ? new StringName("army_ready") : Variant.From(0);
    }

    /// <summary>{skirmish.card strike cost}: a card's field as the game reads it from its
    /// resource, a reference as the card's key.</summary>
    static Variant CardField(StringName key, StringName field)
    {
        var card = Baker.Record<Resource>("cards", key);
        if (card == null)
            return default;
        var value = card.Get(field);
        if (value.VariantType == Variant.Type.Object && value.AsGodotObject() is Resource other)
            foreach (var (k, r) in Baker.Records("cards"))
                if (r.AsGodotObject() == other)
                    return new StringName(k.AsString());
        return value;
    }
}
