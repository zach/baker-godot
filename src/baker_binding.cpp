#include "baker_binding.hpp"

#include "baker_runtime.hpp"
#include "values.hpp"

#include <baker/config.hpp>
#include <baker/tweak.hpp>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <memory>
#include <vector>

namespace baker_godot {

using godot::Array;
using godot::Dictionary;
using godot::String;
using godot::Variant;

// A tweak of some type, and the property it assigns.
struct BakerBinding::Bound {
    virtual ~Bound() = default;
    [[nodiscard]] virtual const baker::TweakBase& tweak() const = 0;
    [[nodiscard]] virtual Variant value() const = 0;

    godot::Node* target = nullptr;
    godot::StringName property;
    godot::Variant last;  // what the property was last given, or seen to hold

    void assign() {
        last = value();
        target->set(property, last);
    }
};

namespace {

template <class T>
struct BoundAs final : BakerBinding::Bound {
    BoundAs(baker::Symbol path, T fallback, const baker::TweakMeta& meta)
        : t(path, std::move(fallback), meta) {}
    [[nodiscard]] const baker::TweakBase& tweak() const override { return t; }
    [[nodiscard]] Variant value() const override { return Variant(t.value()); }
    baker::Tweak<T> t;
};

// A StringName reads as a symbol: the tweak holds a Symbol, the property a StringName.
template <>
struct BoundAs<baker::Symbol> final : BakerBinding::Bound {
    BoundAs(baker::Symbol path, baker::Symbol fallback, const baker::TweakMeta& meta)
        : t(path, fallback, meta) {}
    [[nodiscard]] const baker::TweakBase& tweak() const override { return t; }
    [[nodiscard]] Variant value() const override {
        return godot::StringName(to_godot(t.value().str()));
    }
    baker::Tweak<baker::Symbol> t;
};

// A tweak whose default is the property's value from the scene.
std::unique_ptr<BakerBinding::Bound> make_bound(baker::Symbol path, const Variant& v,
                                                const baker::TweakMeta& m) {
    switch (v.get_type()) {
        case Variant::BOOL: return std::make_unique<BoundAs<bool>>(path, bool(v), m);
        case Variant::INT: return std::make_unique<BoundAs<int64_t>>(path, int64_t(v), m);
        case Variant::FLOAT: return std::make_unique<BoundAs<double>>(path, double(v), m);
        case Variant::STRING:
            return std::make_unique<BoundAs<godot::String>>(path, godot::String(v), m);
        case Variant::STRING_NAME:
            return std::make_unique<BoundAs<baker::Symbol>>(path, baker::Symbol(to_std(String(v))),
                                                            m);
        case Variant::VECTOR2:
            return std::make_unique<BoundAs<godot::Vector2>>(path, godot::Vector2(v), m);
        case Variant::VECTOR3:
            return std::make_unique<BoundAs<godot::Vector3>>(path, godot::Vector3(v), m);
        case Variant::COLOR:
            return std::make_unique<BoundAs<godot::Color>>(path, godot::Color(v), m);
        default: return nullptr;
    }
}

// What the property's export hint says, as the tweak's metadata, for tools and agents:
// @export_range(min, max, step) its range and step.
baker::TweakMeta meta_of(const Dictionary& property) {
    baker::TweakMeta meta;
    const int64_t hint = property.get("hint", 0);
    const godot::PackedStringArray parts = String(property.get("hint_string", "")).split(",");
    if (hint == godot::PROPERTY_HINT_RANGE && parts.size() >= 2 && parts[0].is_valid_float() &&
        parts[1].is_valid_float()) {
        meta.range = {parts[0].to_float(), parts[1].to_float()};
        if (parts.size() >= 3 && parts[2].is_valid_float()) meta.step = parts[2].to_float();
    }
    return meta;
}

}  // namespace

std::vector<BakerBinding*> BakerBinding::live_;

BakerBinding::BakerBinding() = default;
BakerBinding::~BakerBinding() {
    unbind();
}

void BakerBinding::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("set_bindings", "bindings"), &BakerBinding::set_bindings);
    ClassDB::bind_method(D_METHOD("get_bindings"), &BakerBinding::get_bindings);
    ClassDB::bind_method(D_METHOD("sources"), &BakerBinding::sources);
    ClassDB::bind_method(D_METHOD("rebind"), &BakerBinding::rebind);
    ClassDB::bind_method(D_METHOD("capture"), &BakerBinding::capture);
    ADD_PROPERTY(godot::PropertyInfo(Variant::DICTIONARY, "bindings",
                                     godot::PROPERTY_HINT_TYPE_STRING, "String:String"),
                 "set_bindings", "get_bindings");
}

void BakerBinding::set_bindings(const Dictionary& bindings) {
    bindings_ = bindings;
    if (is_node_ready()) rebind();
}

void BakerBinding::_ready() {
    if (godot::Engine::get_singleton()->is_editor_hint()) return;
    rebind();
    if constexpr (!baker::kShipping)
        set_process(godot::EngineDebugger::get_singleton()->is_active());
}

// Under the editor's debugger only: a few times a second, a property the remote inspector
// changed becomes a live edit.
void BakerBinding::_process(double delta) {
    if constexpr (!baker::kShipping) {
        since_capture_ += delta;
        if (since_capture_ < kCaptureInterval) return;
        since_capture_ = 0;
        capture();
    }
}

int64_t BakerBinding::capture() {
    BakerRuntime* runtime = BakerRuntime::get_singleton();
    if (!runtime || !runtime->is_open()) return 0;
    int64_t edits = 0;
    for (const std::unique_ptr<Bound>& b : bound_) {
        const Variant now = b->target->get(b->property);
        if (now == b->last) continue;
        b->last = now;
        if (runtime->set_value(to_godot(b->tweak().path()), now) == godot::OK) ++edits;
    }
    return edits;
}

Array BakerBinding::all_bindings() {
    Array out;
    for (const BakerBinding* binding : live_) {
        for (const std::unique_ptr<Bound>& b : binding->bound_) {
            Dictionary d;
            d["object_id"] = int64_t(b->target->get_instance_id());
            d["node"] = String(b->target->get_path());
            d["property"] = b->property;
            d["path"] = to_godot(b->tweak().path());
            out.push_back(d);
        }
    }
    return out;
}

void BakerBinding::_exit_tree() {
    unbind();
}

void BakerBinding::unbind() {
    bound_.clear();
    std::erase(live_, this);
}

void BakerBinding::rebind() {
    unbind();
    godot::Node* target = get_parent();
    if (!target) return;
    const godot::Array properties = bindings_.keys();
    for (int64_t i = 0; i < properties.size(); ++i) {
        const godot::StringName property = properties[i];
        const String path = bindings_[properties[i]];
        Dictionary info;
        const Variant current = target->get(property);
        for (const Variant& p : target->get_property_list())
            if (godot::StringName(Dictionary(p)["name"]) == property) info = p;
        if (info.is_empty()) {
            godot::UtilityFunctions::push_warning("BakerBinding: ", target->get_name(),
                                                  " has no property ", property);
            continue;
        }
        std::unique_ptr<Bound> b = make_bound(baker::Symbol(to_std(path)), current, meta_of(info));
        if (!b) {
            godot::UtilityFunctions::push_warning("BakerBinding: ", property, " is a ",
                                                  Variant::get_type_name(current.get_type()),
                                                  ", which data cannot hold");
            continue;
        }
        b->target = target;
        b->property = property;
        Bound* raw = b.get();
        // The tweak outlives no binding: it is destroyed with `b`, and with it this callback.
        const_cast<baker::TweakBase&>(raw->tweak()).on_change([raw] { raw->assign(); });
        if (raw->tweak().bound())
            raw->assign();
        else
            raw->last = current;
        bound_.push_back(std::move(b));
    }
    if (!bound_.empty()) live_.push_back(this);
}

Dictionary BakerBinding::sources() const {
    Dictionary out;
    for (const std::unique_ptr<Bound>& b : bound_) {
        Dictionary d;
        const baker::TweakBase& t = b->tweak();
        d["path"] = to_godot(t.path());
        d["bound"] = t.bound();
        if (auto s = t.source()) {
            d["layer"] = to_godot(s->layer.str());
            d["file"] = to_godot(s->file.str());
            d["line"] = s->line;
        }
        out[b->property] = d;
    }
    return out;
}

}  // namespace baker_godot
