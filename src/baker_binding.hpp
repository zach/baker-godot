#pragma once

// BakerBinding: binds properties of its parent, or of a Resource, to paths in the data.
//
//   Player (CharacterBody2D, player.gd: @export var jump_height := 96.0)
//   └─ BakerBinding   bindings = {"jump_height": "player.jump.height"}
//
// Each path is one tweak of the property's own type, made at runtime and shared by every
// bound property that names the path: a hundred units of one kind bind one tweak. Its
// default is the first binder's value from its scene. The property is assigned when it binds
// and whenever the data changes (a reload, a layer toggled, a live edit), so GDScript reads
// it as a plain property: no call into Baker per frame; property_changed(property) follows
// each assignment, for state the node derives from it. A path the data lacks leaves each
// node's own scene value standing.
//
// A path may take a key from the node: "units.{kind}.hp" reads the node's `kind` when it
// binds, so one binding in a base scene serves every scene that inherits it.
//
// Types: bool, int, float, String, StringName, Vector2, Vector3, Color; a Curve, as its
// points; and a reference to a record (Baker.bind_records), as the record's key.
//
// The property's export hint is the tweak's metadata, which reference --baker-json gives tools and
// agents, and which check holds the data to: @export_range(min, max, step) its range and step;
// @export_enum its allowed values (a String's strings, an int's named choices). The data's
// own (@range ...) or (@one_of ...) after a value wins, as for any tweak.
//
// Under the editor's debugger, a value set on a bound property from outside (the remote
// inspector) becomes a live edit of its path, and so of every node that binds the path: in
// scratch, unsaved, in the journal. It is
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
    // What the bindings bind: the parent node, unless a target is set (a Resource, whose
    // properties are bound as a node's are; Baker.bind_resource sets it). Kept alive while
    // it is the target.
    void set_target(godot::Object* target);
    [[nodiscard]] godot::Object* get_target() const;
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
    struct Use;
    // How many paths are bound, each one tweak however many nodes bind it.
    [[nodiscard]] static int64_t shared_tweak_count();
    // The file of the resource whose property binds `path`, or empty: the bottom of the
    // path's chain, beneath the layers.
    [[nodiscard]] static godot::String resource_of(const godot::String& path);

protected:
    static void _bind_methods();

private:
    void unbind();
    [[nodiscard]] godot::Object* bound_target() const;

    static constexpr double kCaptureInterval = 0.1;  // seconds

    godot::Dictionary bindings_;  // property -> path
    godot::Variant target_;       // set_target's, or nil: the parent
    std::vector<std::unique_ptr<Use>> uses_;
    double since_capture_ = 0;
    static std::vector<BakerBinding*> live_;  // the bindings with bound properties
};

}  // namespace baker_godot
