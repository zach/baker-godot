#include "baker_binding.hpp"

#include "values.hpp"

#include <baker/config.hpp>
#include <baker/tweak.hpp>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace baker_godot {

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

    void assign() const { target->set(property, value()); }
};

namespace {

template <class T>
struct BoundAs final : BakerBinding::Bound {
    explicit BoundAs(baker::Symbol path, T fallback) : t(path, std::move(fallback)) {}
    [[nodiscard]] const baker::TweakBase& tweak() const override { return t; }
    [[nodiscard]] Variant value() const override { return Variant(t.value()); }
    baker::Tweak<T> t;
};

// A StringName reads as a symbol: the tweak holds a Symbol, the property a StringName.
template <>
struct BoundAs<baker::Symbol> final : BakerBinding::Bound {
    BoundAs(baker::Symbol path, baker::Symbol fallback) : t(path, fallback) {}
    [[nodiscard]] const baker::TweakBase& tweak() const override { return t; }
    [[nodiscard]] Variant value() const override {
        return godot::StringName(to_godot(t.value().str()));
    }
    baker::Tweak<baker::Symbol> t;
};

// A tweak whose default is the property's value from the scene.
std::unique_ptr<BakerBinding::Bound> make_bound(baker::Symbol path, const Variant& v) {
    switch (v.get_type()) {
        case Variant::BOOL: return std::make_unique<BoundAs<bool>>(path, bool(v));
        case Variant::INT: return std::make_unique<BoundAs<int64_t>>(path, int64_t(v));
        case Variant::FLOAT: return std::make_unique<BoundAs<double>>(path, double(v));
        case Variant::STRING:
            return std::make_unique<BoundAs<godot::String>>(path, godot::String(v));
        case Variant::VECTOR2:
            return std::make_unique<BoundAs<godot::Vector2>>(path, godot::Vector2(v));
        case Variant::VECTOR3:
            return std::make_unique<BoundAs<godot::Vector3>>(path, godot::Vector3(v));
        case Variant::COLOR: return std::make_unique<BoundAs<godot::Color>>(path, godot::Color(v));
        default: return nullptr;
    }
}

}  // namespace

BakerBinding::BakerBinding() = default;
BakerBinding::~BakerBinding() = default;

void BakerBinding::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("set_bindings", "bindings"), &BakerBinding::set_bindings);
    ClassDB::bind_method(D_METHOD("get_bindings"), &BakerBinding::get_bindings);
    ClassDB::bind_method(D_METHOD("sources"), &BakerBinding::sources);
    ClassDB::bind_method(D_METHOD("rebind"), &BakerBinding::rebind);
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
}

void BakerBinding::_exit_tree() {
    unbind();
}

void BakerBinding::unbind() {
    bound_.clear();
}

void BakerBinding::rebind() {
    unbind();
    godot::Node* target = get_parent();
    if (!target) return;
    const godot::Array properties = bindings_.keys();
    for (int64_t i = 0; i < properties.size(); ++i) {
        const godot::StringName property = properties[i];
        const String path = bindings_[properties[i]];
        bool has = false;
        const Variant current = target->get(property);
        for (const Variant& p : target->get_property_list())
            if (godot::StringName(Dictionary(p)["name"]) == property) has = true;
        if (!has) {
            godot::UtilityFunctions::push_warning("BakerBinding: ", target->get_name(),
                                                  " has no property ", property);
            continue;
        }
        const baker::Symbol symbol(to_std(path));
        std::unique_ptr<Bound> b = current.get_type() == Variant::STRING_NAME
                                       ? std::make_unique<BoundAs<baker::Symbol>>(
                                             symbol, baker::Symbol(to_std(String(current))))
                                       : make_bound(symbol, current);
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
        if (raw->tweak().bound()) raw->assign();
        bound_.push_back(std::move(b));
    }
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
