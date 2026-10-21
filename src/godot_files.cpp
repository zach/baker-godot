#include "godot_files.hpp"

#include "values.hpp"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>

#include <cstdio>

namespace baker_godot {

namespace {

bool starts_with(std::string_view s, std::string_view prefix) {
    return s.substr(0, prefix.size()) == prefix;
}

}  // namespace

std::string godot_path(std::string_view path) {
    if (path.find("://") != std::string_view::npos || starts_with(path, "/") ||
        (path.size() > 1 && path[1] == ':'))
        return std::string(path);
    return "res://" + std::string(path);
}

baker::LoadSession::Reader reader() {
    return [](std::string_view path, baker::TextSink out) {
        const godot::String p = to_godot(godot_path(path));
        if (!godot::FileAccess::file_exists(p)) return false;
        const godot::PackedByteArray bytes = godot::FileAccess::get_file_as_bytes(p);
        if (bytes.is_empty() && godot::FileAccess::get_open_error() != godot::OK) return false;
        out({reinterpret_cast<const char*>(bytes.ptr()), size_t(bytes.size())});
        return true;
    };
}

bool write_file(std::string_view path, std::string_view text, bool project) {
    const std::string target = godot_path(path);
    if (!project && !starts_with(target, "user://")) return false;
    const godot::String p = to_godot(target);
    if (godot::DirAccess::make_dir_recursive_absolute(p.get_base_dir()) != godot::OK) return false;
    const godot::Ref<godot::FileAccess> f = godot::FileAccess::open(p, godot::FileAccess::WRITE);
    if (f.is_null()) return false;
    godot::PackedByteArray bytes;
    bytes.resize(int64_t(text.size()));
    for (size_t i = 0; i < text.size(); ++i) bytes.set(int64_t(i), uint8_t(text[i]));
    return f->store_buffer(bytes);
}

bool write_output(const std::string& path, std::string_view text) {
    if (path.find("://") != std::string::npos) return write_file(path, text, true);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    return std::fclose(f) == 0 && ok;
}

bool list_files(std::string_view dir, const baker::Callback<void(std::string_view)>& file) {
    const godot::String d = to_godot(godot_path(dir));
    if (!godot::DirAccess::dir_exists_absolute(d)) return false;
    const godot::PackedStringArray names = godot::DirAccess::get_files_at(d);
    std::string base(dir);
    if (!base.empty() && base.back() != '/') base += '/';
    for (const godot::String& n : names) file(base + to_std(n));
    return true;
}

bool disk_path(std::string_view path, std::string& out) {
    const std::string p = godot_path(path);
    // An export's res:// is its pack, not a directory.
    if (starts_with(p, "res://") && godot::OS::get_singleton()->has_feature("template"))
        return false;
    const godot::String global =
        godot::ProjectSettings::get_singleton()->globalize_path(to_godot(p));
    out = to_std(global);
    return out.find("://") == std::string::npos;
}

}  // namespace baker_godot
