#include "values.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace baker_godot {

using godot::Variant;

godot::String to_godot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

std::string to_std(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data(), size_t(utf8.length())};
}

Variant to_variant(const baker::Node& node) {
    switch (node.type()) {
        case baker::Type::Int: return Variant(*node.try_int());
        case baker::Type::Float: return Variant(*node.try_float());
        case baker::Type::Symbol: return godot::StringName(to_godot(node.try_sym()->str()));
        case baker::Type::String: return to_godot(*node.try_str());
        case baker::Type::Array: {
            godot::Array out;
            for (const baker::Node& n : node.array()->nodes()) out.push_back(to_variant(n));
            return out;
        }
        default: return {};  // commands, variables, objects: not values a property holds
    }
}

namespace {

template <class T>
Variant read(const baker::Values& values) {
    T out{};
    if (baker::Convert<T>::from(values, out)) return Variant(out);
    return {};
}

}  // namespace

Variant to_variant(const baker::Values& values, Variant::Type type) {
    switch (type) {
        case Variant::BOOL: return read<bool>(values);
        case Variant::INT: return read<int64_t>(values);
        case Variant::FLOAT: return read<double>(values);
        case Variant::STRING: return read<godot::String>(values);
        case Variant::STRING_NAME: {
            baker::Symbol s;
            if (baker::Convert<baker::Symbol>::from(values, s))
                return godot::StringName(to_godot(s.str()));
            return {};
        }
        case Variant::VECTOR2: return read<godot::Vector2>(values);
        case Variant::VECTOR3: return read<godot::Vector3>(values);
        case Variant::COLOR: return read<godot::Color>(values);
        default: break;
    }
    if (values.size() == 1) return to_variant(values[0]);
    godot::Array out;
    for (const baker::Node& n : values) out.push_back(to_variant(n));
    return out;
}

bool to_values(const Variant& value, baker::ValueList& out) {
    switch (value.get_type()) {
        case Variant::BOOL: out.add(bool(value)); return true;
        case Variant::INT: out.add(int64_t(value)); return true;
        case Variant::FLOAT: out.add(double(value)); return true;
        case Variant::STRING: out.add(godot::String(value)); return true;
        case Variant::STRING_NAME:
            out.push(baker::Node(baker::Symbol(to_std(godot::String(value)))));
            return true;
        case Variant::VECTOR2: baker::Convert<godot::Vector2>::to(value, out); return true;
        case Variant::VECTOR3: baker::Convert<godot::Vector3>::to(value, out); return true;
        case Variant::COLOR: baker::Convert<godot::Color>::to(value, out); return true;
        case Variant::ARRAY: {
            // A value of several nodes, as to_variant gives it: each a node of its own.
            const godot::Array a = value;
            for (int64_t i = 0; i < a.size(); ++i) {
                const Variant& v = a[i];
                if (v.get_type() == Variant::ARRAY || !to_values(v, out)) return false;
            }
            return !a.is_empty();
        }
        default: return false;
    }
}

}  // namespace baker_godot
