#pragma once

// Godot values and Baker data, both ways.
//
// A Godot value reads from data by the type it already has: a float property reads
// (height 96) as 96.0, a Vector2 reads (gravity 0 980), a Color (tint 1 0.5 0 1). The
// Convert specializations below make Godot's math types ordinary tweak types, so a
// BakerBinding's property is a Tweak<T> of its own type.

#include <baker/array.hpp>

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <string>
#include <vector>

template <>
struct baker::Convert<godot::Vector2> {
    static constexpr std::string_view name = "vec2";
    static bool from(const baker::Values& v, godot::Vector2& out) {
        return v.size() >= 2 && v.read(0, out.x) && v.read(1, out.y);
    }
    static void to(const godot::Vector2& v, baker::ValueList& out) {
        out.add(v.x);
        out.add(v.y);
    }
};

template <>
struct baker::Convert<godot::Vector3> {
    static constexpr std::string_view name = "vec3";
    static bool from(const baker::Values& v, godot::Vector3& out) {
        return v.size() >= 3 && v.read(0, out.x) && v.read(1, out.y) && v.read(2, out.z);
    }
    static void to(const godot::Vector3& v, baker::ValueList& out) {
        out.add(v.x);
        out.add(v.y);
        out.add(v.z);
    }
};

// Three or four channels, 0..1; alpha defaults to 1.
template <>
struct baker::Convert<godot::Color> {
    static constexpr std::string_view name = "color";
    static bool from(const baker::Values& v, godot::Color& out) {
        out.a = 1;
        return v.size() >= 3 && v.read(0, out.r) && v.read(1, out.g) && v.read(2, out.b) &&
               (v.size() < 4 || v.read(3, out.a));
    }
    static void to(const godot::Color& v, baker::ValueList& out) {
        out.add(v.r);
        out.add(v.g);
        out.add(v.b);
        if (v.a != 1) out.add(v.a);
    }
};

template <>
struct baker::Convert<godot::String> {
    static constexpr std::string_view name = "string";
    static constexpr bool scalar = true;
    static bool from(const baker::Values& v, godot::String& out) {
        if (v.empty()) return false;
        if (const auto s = v[0].try_str()) {
            out = godot::String::utf8(s->data(), static_cast<int64_t>(s->size()));
            return true;
        }
        return false;
    }
    static void to(const godot::String& v, baker::ValueList& out) {
        const godot::CharString utf8 = v.utf8();
        out.push(baker::Node(baker::String::make({utf8.get_data(), size_t(utf8.length())})));
    }
};

namespace baker_godot {

// The values at a path, uninterpreted, to be read as a type chosen at runtime (a GDScript
// fallback's, a property's): get<RawValues> finds them, to_variant reads them.
struct RawValues {
    std::vector<baker::Node> nodes;
    [[nodiscard]] baker::Values values() const { return {nodes, nullptr}; }
};

// One node as a Variant: numbers, symbols (as StringName), strings, arrays (as Array).
[[nodiscard]] godot::Variant to_variant(const baker::Node& node);
// `values` read as `type`, if they convert; nil otherwise. NIL: any type, by the values:
// one node is its own value, several are an Array.
[[nodiscard]] godot::Variant to_variant(const baker::Values& values, godot::Variant::Type type);
// `value` as data: false for a type data cannot hold.
[[nodiscard]] bool to_values(const godot::Variant& value, baker::ValueList& out);

[[nodiscard]] godot::String to_godot(std::string_view text);
[[nodiscard]] std::string to_std(const godot::String& text);

}  // namespace baker_godot

template <>
struct baker::Convert<baker_godot::RawValues> {
    static constexpr std::string_view name = "value";
    static bool from(const baker::Values& v, baker_godot::RawValues& out) {
        out.nodes.assign(v.begin(), v.end());
        return !v.empty();
    }
};
