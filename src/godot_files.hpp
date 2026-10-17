#pragma once

// Baker's file access, through Godot's: a layer's path is a project path (res://), a user
// path (user://), or relative to the project's root.

#include <baker/parse.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace baker_godot {

// The path Godot opens for one Baker names: "data/a.dta" is res://data/a.dta. Baker
// normalizes "user://a" to "user:/a" (requirements/0002), which is read back here.
[[nodiscard]] std::string godot_path(std::string_view path);

// Reads through Godot's file access.
[[nodiscard]] baker::LoadSession::Reader reader();

// Writes `text` to `path`, creating its directory. Only user:// paths: a game never writes
// into the project, which the editor owns (plan step 2 writes from the editor process).
[[nodiscard]] bool write_user_file(std::string_view path, std::string_view text);

// What tells a file's versions apart without reading it: when it was last written, in the
// file system's clock, and its size. Two writes in one tick of a coarse file clock differ
// by size, if at all. Empty for a file that does not exist or has no path on disk (an
// exported game's packed files).
struct FileStamp {
    bool exists = false;
    int64_t time_ns = 0;
    uint64_t size = 0;
    friend bool operator==(const FileStamp&, const FileStamp&) = default;
};
[[nodiscard]] FileStamp file_stamp(std::string_view path);
// A file time as Baker's default time source (the steady clock) gives it, for
// ReloadRequest::origin_ns.
[[nodiscard]] uint64_t steady_from_file_time(int64_t file_ns);

}  // namespace baker_godot
