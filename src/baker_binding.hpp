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
//
// The property's export hint is the tweak's metadata, which reference --json gives tools and
// agents, and which check holds the data to: @export_range(min, max, step) its range and step;
// @export_enum its allowed values (a String's strings, an int's named choices). The data's
// own (range ...) or (one_of ...) after a value wins, as for any tweak.
//
// Under the editor's debugger, a value set on a bound property from outside (the remote
// inspector) becomes a live edit of its path: in scratch, unsaved, in the journal. It is
// noticed by comparing each bound property with what it was last given, ten times a second,
// and only while a debugger is attached; a game run on its own never compares. Bound
// properties are tunables: code that changes one at runtime is taken for a tuner.

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
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
    void _process(double delta) override;

    void set_bindings(const godot::Dictionary& bindings);
    [[nodiscard]] godot::Dictionary get_bindings() const { return bindings_; }
    // {property: {path, bound, layer, file, line}}: where each property's value comes from.
    [[nodiscard]] godot::Dictionary sources() const;
    // Drops the bindings and binds again, as on _ready.
    void rebind();
    // Makes a live edit of each bound property whose value changed from outside since it
    // was last assigned; the number made.
    int64_t capture();
    // Every bound property of every binding: [{object_id, node, property, path}].
    [[nodiscard]] static godot::Array all_bindings();

    // One bound property; defined in the .cpp.
    struct Bound;

protected:
    static void _bind_methods();

private:
    void unbind();

    static constexpr double kCaptureInterval = 0.1;  // seconds

    godot::Dictionary bindings_;  // property -> path
    std::vector<std::unique_ptr<Bound>> bound_;
    double since_capture_ = 0;
    static std::vector<BakerBinding*> live_;  // the bindings with bound properties
};

}  // namespace baker_godot
