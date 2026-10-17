#include "godot_files.hpp"

#include "values.hpp"

#include <baker/clock.hpp>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>

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

FileStamp file_stamp(std::string_view path) {
    const godot::String global =
        godot::ProjectSettings::get_singleton()->globalize_path(to_godot(godot_path(path)));
    const std::filesystem::path p = to_std(global);
    std::error_code ec;
    const auto written = std::filesystem::last_write_time(p, ec);
    if (ec) return {};
    const uintmax_t size = std::filesystem::file_size(p, ec);
    if (ec) return {};
    return {
        true,
        std::chrono::duration_cast<std::chrono::nanoseconds>(written.time_since_epoch()).count(),
        size};
}

uint64_t steady_from_file_time(int64_t file_ns) {
    // The file's age by the file clock, subtracted from the steady clock's now: the error is
    // the time between the two now()s.
    const int64_t now_file = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::file_clock::now().time_since_epoch())
                                 .count();
    const uint64_t now = baker::steady_now();
    const int64_t age = now_file - file_ns;
    return age <= 0 ? now : now - std::min<uint64_t>(now, uint64_t(age));
}

}  // namespace baker_godot
