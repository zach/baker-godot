#pragma once

// BakerBinding: binds properties of its parent to paths in the data.
//
//   Player (CharacterBody2D, player.gd: @export var jump_height := 96.0)
//   └─ BakerBinding   bindings = {"jump_height": "player.jump.height"}
//
// Each binding is a tweak of the property's own type, made at runtime, whose default is
// the value the scene gives the property. The property is assigned when it binds and
// whenever the data changes (a reload, a layer toggled, a live edit), so GDScript reads it
// as a plain property: no call into Baker per frame. A path the data lacks leaves the
// scene's value standing.
//
// Types: bool, int, float, String, StringName, Vector2, Vector3, Color.

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <memory>
#include <vector>

namespace baker_godot {

class BakerBinding : public godot::Node {
    GDCLASS(BakerBinding, godot::Node)

public:
    BakerBinding();
    ~BakerBinding() override;

    void _ready() override;
    void _exit_tree() override;

    void set_bindings(const godot::Dictionary& bindings);
    [[nodiscard]] godot::Dictionary get_bindings() const { return bindings_; }
    // {property: {path, bound, layer, file, line}}: where each property's value comes from.
    [[nodiscard]] godot::Dictionary sources() const;
    // Drops the bindings and binds again, as on _ready.
    void rebind();

    // One bound property; defined in the .cpp.
    struct Bound;

protected:
    static void _bind_methods();

private:
    void unbind();

    godot::Dictionary bindings_;  // property -> path
    std::vector<std::unique_ptr<Bound>> bound_;
};

}  // namespace baker_godot
