# Baker.Godot

The C# API of [Baker for Godot](https://github.com/zach/baker-godot): Baker's layered tuning
data in a Godot 4.7 C# game, typed.

It calls the `Baker` autoload, which the Baker for Godot addon provides: install the addon
(`addons/baker`, from the project's GitHub releases) and enable it, then add this package.

```csharp
using BakerGodot;

float height = Baker.Get("player.jump.height", 96f);       // a read, as the fallback's type
var units = Baker.GetTable("units");                       // a keyed table, whole
var gravity = new Tweak<float>("world.gravity", 980f);     // cached, current after every change
Baker.Changed += () => GD.Print("the data changed");
int damage = Baker.CallFunction<int>("skirmish.damage", 6, 2, 0);   // a rule the data defines
```
