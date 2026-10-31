#include "baker_binding.hpp"

#include "baker_runtime.hpp"
#include "values.hpp"

#include <baker/config.hpp>
#include <baker/tweak.hpp>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace baker_godot {

using godot::Array;
using godot::Dictionary;
using godot::String;
using godot::Variant;

// One bound property of one node: where a path's value goes.
struct BakerBinding::Use {
    BakerBinding* binding = nullptr;
    godot::Object* target = nullptr;
    godot::StringName property;
    godot::Variant last;   // what the property was last given, or seen to hold
    godot::Variant scene;  // its own value from the scene, while the data lacks the path
    struct Shared* shared = nullptr;

    // Sets the property to the data's value, as the property holds it.
    void assign(const Variant& value);
    void assign_held(const Variant& value) {
        last = value;
        target->set(property, value);
        binding->emit_signal("property_changed", property);
    }
};

namespace {

// A tweak of some type.
struct Tweaked {
    virtual ~Tweaked() = default;
    [[nodiscard]] virtual baker::TweakBase& tweak() = 0;
    [[nodiscard]] virtual Variant value() const = 0;
};

template <class T>
struct TweakAs final : Tweaked {
    TweakAs(baker::Symbol path, T fallback, const baker::TweakMeta& meta)
        : t(path, std::move(fallback), meta) {}
    [[nodiscard]] baker::TweakBase& tweak() override { return t; }
    [[nodiscard]] Variant value() const override { return Variant(t.value()); }
    baker::Tweak<T> t;
};

// A StringName reads as a symbol: the tweak holds a Symbol, the property a StringName.
template <>
struct TweakAs<baker::Symbol> final : Tweaked {
    TweakAs(baker::Symbol path, baker::Symbol fallback, const baker::TweakMeta& meta)
        : t(path, fallback, meta) {}
    [[nodiscard]] baker::TweakBase& tweak() override { return t; }
    [[nodiscard]] Variant value() const override {
        return godot::StringName(to_godot(t.value().str()));
    }
    baker::Tweak<baker::Symbol> t;
};

// A curve: Godot's Curve on the property, Baker's points in the data.
struct CurveTweak final : Tweaked {
    CurveTweak(baker::Symbol path, std::vector<baker::CurvePoint> defaults, baker::CurveMode mode,
               const baker::TweakMeta& meta)
        : points(std::move(defaults)), t(path, baker::Curve(points, mode), meta) {}
    [[nodiscard]] baker::TweakBase& tweak() override { return t; }
    [[nodiscard]] Variant value() const override {
        String warning;
        const godot::Ref<godot::Curve> curve = to_godot_curve(t.value(), warning);
        if (!warning.is_empty())
            godot::UtilityFunctions::push_warning("BakerBinding: ", to_godot(t.path()), ": ",
                                                  warning);
        return curve;
    }
    std::vector<baker::CurvePoint> points;  // the default's: first, as the tweak views them
    baker::Tweak<baker::Curve> t;
};

// A tweak whose default is the first binder's value from its scene.
std::unique_ptr<Tweaked> make_tweak(baker::Symbol path, const Variant& v,
                                    const baker::TweakMeta& m) {
    switch (v.get_type()) {
        case Variant::BOOL: return std::make_unique<TweakAs<bool>>(path, bool(v), m);
        case Variant::INT: return std::make_unique<TweakAs<int64_t>>(path, int64_t(v), m);
        case Variant::FLOAT: return std::make_unique<TweakAs<double>>(path, double(v), m);
        case Variant::STRING:
            return std::make_unique<TweakAs<godot::String>>(path, godot::String(v), m);
        case Variant::STRING_NAME:
            return std::make_unique<TweakAs<baker::Symbol>>(path, baker::Symbol(to_std(String(v))),
                                                            m);
        case Variant::VECTOR2:
            return std::make_unique<TweakAs<godot::Vector2>>(path, godot::Vector2(v), m);
        case Variant::VECTOR3:
            return std::make_unique<TweakAs<godot::Vector3>>(path, godot::Vector3(v), m);
        case Variant::COLOR:
            return std::make_unique<TweakAs<godot::Color>>(path, godot::Color(v), m);
        default: return nullptr;
    }
}

// What the tweak's metadata points at: an enum hint's allowed values, by interned names.
struct Allowed {
    std::vector<std::string_view> one_of;
    std::vector<baker::TweakChoice> choices;
};

// What the property's export hint says, as the tweak's metadata, for tools, agents and
// check: @export_range(min, max, step) its range and step; @export_enum its allowed values,
// a String's as one_of, an int's as named choices ("Easy,Hard:5": 0 easy, 5 hard).
baker::TweakMeta meta_of(const Dictionary& property, Allowed& allowed) {
    baker::TweakMeta meta;
    const int64_t hint = property.get("hint", 0);
    const godot::PackedStringArray parts = String(property.get("hint_string", "")).split(",");
    if (hint == godot::PROPERTY_HINT_RANGE && parts.size() >= 2 && parts[0].is_valid_float() &&
        parts[1].is_valid_float()) {
        meta.range = {parts[0].to_float(), parts[1].to_float()};
        if (parts.size() >= 3 && parts[2].is_valid_float()) meta.step = parts[2].to_float();
    } else if (hint == godot::PROPERTY_HINT_ENUM) {
        const auto type = Variant::Type(int64_t(property.get("type", 0)));
        int64_t next = 0;
        for (const String& part : parts) {
            const String name = part.get_slice(":", 0).strip_edges();
            if (name.is_empty()) continue;
            const std::string_view interned = baker::Symbol(to_std(name)).str();  // for good
            if (type == Variant::STRING) {
                allowed.one_of.push_back(interned);
            } else if (type == Variant::INT) {
                const String value = part.get_slice(":", 1).strip_edges();
                if (part.contains(":") && value.is_valid_int()) next = value.to_int();
                allowed.choices.push_back({next++, interned});
            }
        }
        meta.one_of = allowed.one_of;
        meta.choices = allowed.choices;
    }
    return meta;
}

// What a warning calls the bound object: a node's name, else the object's own text.
String name_of(godot::Object& o) {
    if (const auto* n = godot::Object::cast_to<godot::Node>(&o)) return n->get_name();
    return o.to_string();
}

// The plugin a node or resource belongs to: the directory its scene, or its own file, is in
// ("res://plugins/cannon/cannon.tscn": "cannon").
String plugin_of(godot::Object& o) {
    String file;
    if (const auto* n = godot::Object::cast_to<godot::Node>(&o)) {
        const godot::Node* scene = n->get_owner() ? n->get_owner() : n;
        file = scene->get_scene_file_path();
    } else if (const auto* r = godot::Object::cast_to<godot::Resource>(&o)) {
        file = r->get_path();
    }
    return file.is_empty() ? String() : file.get_base_dir().get_file();
}

// "units.{kind}.hp" with the node's `kind` in place of {kind}; empty, and `error` set, if
// the node has no such property or it is empty. {plugin}, when the node has no `plugin`,
// is the plugin it belongs to (plugin_of).
String resolve(const String& path, godot::Object& target, String& error) {
    String out;
    int64_t at = 0;
    while (at < path.length()) {
        const int64_t open = path.find("{", at);
        if (open < 0) {
            out += path.substr(at);
            break;
        }
        const int64_t close = path.find("}", open);
        if (close < 0) {
            error = "an unclosed { in " + path;
            return {};
        }
        out += path.substr(at, open - at);
        const godot::StringName key = path.substr(open + 1, close - open - 1);
        Variant v = target.get(key);
        if (v.get_type() == Variant::NIL && key == godot::StringName("plugin"))
            v = plugin_of(target);
        const String text = v.get_type() == Variant::NIL ? String() : String(v);
        if (text.is_empty()) {
            error = name_of(target) + " has no " + String(key) + " for " + path;
            return {};
        }
        out += text;
        at = close + 1;
    }
    return out;
}

}  // namespace

// One path's tweak, shared by every bound property that binds the path: the first binder's
// type, default and metadata; destroyed when the last one unbinds.
struct Shared {
    std::unique_ptr<Allowed> allowed;  // first: the tweak's metadata points into it
    std::unique_ptr<Tweaked> tweaked;
    Variant::Type type = Variant::NIL;
    std::vector<BakerBinding::Use*> uses;
    String table;  // a reference to a record of this table: the data holds its key

    // The data's value as the property holds it: a key as its record.
    [[nodiscard]] Variant to_property(const Variant& v) const {
        if (table.is_empty()) return v;
        const BakerRuntime* runtime = BakerRuntime::get_singleton();
        return runtime ? Variant(runtime->record(table, String(v))) : Variant();
    }
    // The property's value as data: a record as its key.
    [[nodiscard]] Variant from_property(const Variant& v) const {
        if (table.is_empty()) return v;
        const BakerRuntime* runtime = BakerRuntime::get_singleton();
        const String key = runtime ? runtime->record_key(table, v) : String();
        return godot::StringName(key);
    }
};

void BakerBinding::Use::assign(const Variant& value) {
    assign_held(shared->to_property(value));
}

namespace {

std::unordered_map<std::string, std::unique_ptr<Shared>>& shared_tweaks() {
    static std::unordered_map<std::string, std::unique_ptr<Shared>> tweaks;
    return tweaks;
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
    ClassDB::bind_method(D_METHOD("set_target", "target"), &BakerBinding::set_target);
    ClassDB::bind_method(D_METHOD("get_target"), &BakerBinding::get_target);
    ClassDB::bind_method(D_METHOD("sources"), &BakerBinding::sources);
    ClassDB::bind_method(D_METHOD("rebind"), &BakerBinding::rebind);
    ClassDB::bind_method(D_METHOD("capture"), &BakerBinding::capture);
    ClassDB::bind_static_method("BakerBinding", D_METHOD("shared_tweak_count"),
                                &BakerBinding::shared_tweak_count);
    ClassDB::bind_static_method("BakerBinding", D_METHOD("resource_of", "path"),
                                &BakerBinding::resource_of);
    ADD_PROPERTY(godot::PropertyInfo(Variant::DICTIONARY, "bindings",
                                     godot::PROPERTY_HINT_TYPE_STRING, "String:String"),
                 "set_bindings", "get_bindings");
    // After the data assigns a bound property: a reload, a toggle, a live edit.
    ADD_SIGNAL(godot::MethodInfo("property_changed",
                                 godot::PropertyInfo(Variant::STRING_NAME, "property")));
}

godot::Object* BakerBinding::bound_target() const {
    if (target_.get_type() == Variant::OBJECT) return target_;  // a Resource, say
    return get_parent();
}

void BakerBinding::set_target(godot::Object* target) {
    target_ = target ? Variant(target) : Variant();  // a Ref, so a Resource stays alive
    if (is_node_ready()) rebind();
}

godot::Object* BakerBinding::get_target() const {
    return bound_target();
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

// A live edit of the path, so of every instance that binds it.
int64_t BakerBinding::capture() {
    BakerRuntime* runtime = BakerRuntime::get_singleton();
    if (!runtime || !runtime->is_open()) return 0;
    int64_t edits = 0;
    for (const std::unique_ptr<Use>& u : uses_) {
        const Variant now = u->target->get(u->property);
        if (now == u->last) continue;
        u->last = now;
        const Variant edit = u->shared->from_property(now);
        if (runtime->set_value(to_godot(u->shared->tweaked->tweak().path()), edit) == godot::OK)
            ++edits;
    }
    return edits;
}

Array BakerBinding::all_bindings() {
    Array out;
    for (const BakerBinding* binding : live_) {
        for (const std::unique_ptr<Use>& u : binding->uses_) {
            Dictionary d;
            d["object_id"] = int64_t(u->target->get_instance_id());
            const auto* node = godot::Object::cast_to<godot::Node>(u->target);
            d["node"] = node ? String(node->get_path()) : String();
            d["property"] = u->property;
            d["path"] = to_godot(u->shared->tweaked->tweak().path());
            out.push_back(d);
        }
    }
    return out;
}

int64_t BakerBinding::shared_tweak_count() {
    return int64_t(shared_tweaks().size());
}

String BakerBinding::resource_of(const String& path) {
    const auto it = shared_tweaks().find(to_std(path));
    if (it == shared_tweaks().end()) return {};
    for (const BakerBinding::Use* u : it->second->uses)
        if (const auto* r = godot::Object::cast_to<godot::Resource>(u->target))
            return r->get_path();
    return {};
}

void BakerBinding::_exit_tree() {
    unbind();
}

void BakerBinding::unbind() {
    for (const std::unique_ptr<Use>& u : uses_) {
        Shared* s = u->shared;
        std::erase(s->uses, u.get());
        if (s->uses.empty()) shared_tweaks().erase(std::string(s->tweaked->tweak().path()));
    }
    uses_.clear();
    std::erase(live_, this);
}

void BakerBinding::rebind() {
    unbind();
    godot::Object* target = bound_target();
    if (!target) return;
    Dictionary infos;  // property -> its info, from one walk of the list
    for (const Variant& p : target->get_property_list()) {
        const Dictionary info = p;
        infos[godot::StringName(info["name"])] = info;
    }
    const Array properties = bindings_.keys();
    for (int64_t i = 0; i < properties.size(); ++i) {
        const godot::StringName property = properties[i];
        if (!infos.has(property)) {
            godot::UtilityFunctions::push_warning("BakerBinding: ", name_of(*target),
                                                  " has no property ", property);
            continue;
        }
        String error;
        const String path = resolve(String(bindings_[properties[i]]), *target, error);
        if (path.is_empty()) {
            godot::UtilityFunctions::push_warning("BakerBinding: ", error);
            continue;
        }
        // A property that holds nothing yet (a shader parameter left at the shader's
        // default) is bound as the type it declares, from that type's zero; nil stays its
        // own value, so dropping the path gives the shader's default back.
        const Variant held = target->get(property);
        const auto declared = Variant::Type(int64_t(Dictionary(infos[property]).get("type", 0)));
        Variant current = held.get_type() == Variant::NIL && declared != Variant::NIL
                              ? godot::UtilityFunctions::type_convert(Variant(), declared)
                              : held;
        // A resource property: a Curve, or a reference to a record (by its key).
        String table;
        bool curve = false;
        if (declared == Variant::OBJECT) {
            const Dictionary info = infos[property];
            const String type = info.get("hint_string", "");
            curve = int64_t(info.get("hint", 0)) == godot::PROPERTY_HINT_RESOURCE_TYPE &&
                    type == "Curve";
            if (BakerRuntime* runtime = BakerRuntime::get_singleton(); runtime && !curve)
                table = runtime->record_table(type);
            if (!table.is_empty())
                current = godot::StringName(BakerRuntime::get_singleton()->record_key(
                    table, held.operator godot::Object*()));
        }
        const std::string key = to_std(path);
        std::unique_ptr<Shared>& slot = shared_tweaks()[key];
        if (!slot) {
            auto s = std::make_unique<Shared>();
            s->allowed = std::make_unique<Allowed>();
            if (curve) {
                std::vector<baker::CurvePoint> points;
                baker::CurveMode mode = baker::CurveMode::Monotone;
                if (const auto* c = godot::Object::cast_to<godot::Curve>(held))
                    curve_points(*c, points, mode);
                s->tweaked = std::make_unique<CurveTweak>(baker::Symbol(key), std::move(points),
                                                          mode, baker::TweakMeta{});
            } else if (!table.is_empty()) {
                // The table's keys are the reference's allowed values.
                const Array keys = BakerRuntime::get_singleton()->records(table).keys();
                for (int64_t k = 0; k < keys.size(); ++k)
                    s->allowed->one_of.push_back(baker::Symbol(to_std(String(keys[k]))).str());
                baker::TweakMeta meta;
                meta.one_of = s->allowed->one_of;
                s->tweaked = make_tweak(baker::Symbol(key), current, meta);
                s->table = table;
            } else {
                s->tweaked =
                    make_tweak(baker::Symbol(key), current, meta_of(infos[property], *s->allowed));
            }
            if (!s->tweaked) {
                shared_tweaks().erase(key);
                godot::UtilityFunctions::push_warning("BakerBinding: ", property, " is a ",
                                                      Variant::get_type_name(current.get_type()),
                                                      ", which data cannot hold");
                continue;
            }
            s->type = current.get_type();
            Shared* raw = s.get();
            // The tweak goes when its Shared does, and with it this callback.
            // The data's value, or each node's own when the data no longer has the path.
            raw->tweaked->tweak().on_change([raw] {
                const bool bound = raw->tweaked->tweak().bound();
                const Variant v = raw->tweaked->value();
                for (BakerBinding::Use* u : raw->uses) {
                    if (bound)
                        u->assign(v);
                    else
                        u->assign_held(u->scene);
                }
            });
            slot = std::move(s);
        } else if (slot->type != current.get_type()) {
            godot::UtilityFunctions::push_warning(
                "BakerBinding: ", name_of(*target), ".", property, " is a ",
                Variant::get_type_name(current.get_type()), ", but ", path, " is bound as a ",
                Variant::get_type_name(slot->type));
            continue;
        }
        auto u = std::make_unique<Use>();
        u->binding = this;
        u->target = target;
        u->property = property;
        u->shared = slot.get();
        u->scene = held;
        slot->uses.push_back(u.get());
        // The data's value if it has one; the scene's own until then.
        if (slot->tweaked->tweak().bound())
            u->assign(slot->tweaked->value());
        else
            u->last = held;
        uses_.push_back(std::move(u));
    }
    if (!uses_.empty()) live_.push_back(this);
}

Dictionary BakerBinding::sources() const {
    Dictionary out;
    for (const std::unique_ptr<Use>& u : uses_) {
        Dictionary d;
        const baker::TweakBase& t = u->shared->tweaked->tweak();
        d["path"] = to_godot(t.path());
        d["bound"] = t.bound();
        if (auto src = t.source()) {
            d["layer"] = to_godot(src->layer.str());
            d["file"] = to_godot(src->file.str());
            d["line"] = src->line;
        }
        out[u->property] = d;
    }
    return out;
}

}  // namespace baker_godot
