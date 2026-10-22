#include "values.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <baker/tweak.hpp>

#include <algorithm>
#include <godot_cpp/variant/string_name.hpp>
#include <optional>
#include <vector>

namespace baker_godot {

using godot::String;
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

namespace {

// The symbol an array is tagged by, or none.
std::optional<baker::Symbol> tag_of(const baker::Node& n) {
    const baker::Array* a = n.array();
    if (!a || a->size() == 0) return std::nullopt;
    if (const auto s = (*a)[0].try_sym()) return *s;
    return std::nullopt;
}

// An array's values after its tag.
baker::Values after_tag(const baker::Node& n) {
    const baker::Array* a = n.array();
    return baker::Values(a->nodes().subspan(1), a);
}

Variant table_entry(const baker::Node& n, bool record);

}  // namespace

Variant to_table(const baker::Values& all) {
    const baker::Values values = baker::split_metadata(all).values;
    // Keyed: every value an array tagged by a symbol, each tag once.
    bool keyed = !values.empty();
    bool one_tag = !values.empty();  // a list of records: every value tagged alike
    std::optional<baker::Symbol> first;
    std::vector<baker::Symbol> seen;
    for (const baker::Node& n : values) {
        const auto t = tag_of(n);
        if (!t) {
            keyed = one_tag = false;
            break;
        }
        if (!first) first = t;
        if (*t != *first) one_tag = false;
        if (std::find(seen.begin(), seen.end(), *t) != seen.end()) keyed = false;
        seen.push_back(*t);
    }
    if (keyed && !(one_tag && values.size() > 1)) {
        godot::Dictionary d;
        for (const baker::Node& n : values) d[to_godot(tag_of(n)->str())] = to_table(after_tag(n));
        return d;
    }
    if (values.size() == 1 && !values[0].array()) return to_variant(values[0]);
    godot::Array out;
    for (const baker::Node& n : values) out.push_back(table_entry(n, one_tag));
    return out;
}

namespace {

// One value in a list: a record if its list is records of one tag, a list of its own if an
// untagged array, else itself.
Variant table_entry(const baker::Node& n, bool record) {
    if (!n.array()) return to_variant(n);
    if (record) return to_table(after_tag(n));
    if (!tag_of(n)) return to_table(baker::Values(n.array()->nodes(), n.array()));
    godot::Dictionary d;
    d[to_godot(tag_of(n)->str())] = to_table(after_tag(n));
    return d;
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
        case Variant::DICTIONARY: {
            const Variant table = to_table(values);
            return table.get_type() == Variant::DICTIONARY ? table : Variant();
        }
        case Variant::ARRAY: {
            // A list: of values, or of records; one value is a list of one.
            const Variant table = to_table(values);
            if (table.get_type() == Variant::ARRAY) return table;
            if (table.get_type() == Variant::DICTIONARY) return {};
            godot::Array one;
            one.push_back(table);
            return one;
        }
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
            // A value of several nodes, as to_variant gives it: each a node of its own; a
            // record or a list in it, an untagged array.
            const godot::Array a = value;
            for (int64_t i = 0; i < a.size(); ++i) {
                const Variant& v = a[i];
                const Variant::Type t = v.get_type();
                if (t == Variant::ARRAY || t == Variant::DICTIONARY) {
                    baker::ValueList inner;
                    if (!to_values(v, inner)) return false;
                    out.push(baker::Node(baker::Array::make_copy(inner.nodes())));
                } else if (!to_values(v, out)) {
                    return false;
                }
            }
            return !a.is_empty();
        }
        case Variant::DICTIONARY: {
            // A table: each entry an array tagged by its key, its value after the tag.
            const godot::Dictionary d = value;
            const godot::Array keys = d.keys();
            for (int64_t i = 0; i < keys.size(); ++i) {
                baker::ValueList entry;
                entry.push(baker::Node(baker::Symbol(to_std(String(keys[i])))));
                const Variant v = d[keys[i]];
                if (v.get_type() == Variant::DICTIONARY) {
                    if (!to_values(v, entry)) return false;  // its entries, tagged, inline
                } else if (!to_values(v, entry)) {
                    return false;
                }
                out.push(baker::Node(baker::Array::make_copy(entry.nodes())));
            }
            return !keys.is_empty();
        }
        default: return false;
    }
}

}  // namespace baker_godot
