#pragma once

// Baker's file access, through Godot's: a layer's path is a project path (res://), a user
// path (user://), or relative to the project's root.

#include <baker/callback.hpp>
#include <baker/parse.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace baker_godot {

// The path Godot opens for one Baker names: "data/a.dta" is res://data/a.dta; a path with a
// scheme (res://, user://) or an absolute one is as written.
[[nodiscard]] std::string godot_path(std::string_view path);

// Reads through Godot's file access.
[[nodiscard]] baker::LoadSession::Reader reader();

// Writes `text` to `path`, creating its directory. A user:// path, or with `project` (the
// editor's own writes) any: a game never writes into the project, which the editor owns.
[[nodiscard]] bool write_file(std::string_view path, std::string_view text, bool project);

// Writes a tool's output: a res:// or user:// path, or one on the disk as given (relative to
// where the tool runs).
[[nodiscard]] bool write_output(const std::string& path, std::string_view text);

// The entries of a directory Baker names, files then subdirectories, through Godot's file
// access (an export's pack too): `file` gets each one's path, the directory's joined with
// its name. False if there is no such directory.
[[nodiscard]] bool list_files(std::string_view dir,
                              const baker::Callback<void(std::string_view path)>& file);

// Where a path Baker names is on the disk, for the watcher: none for a file Godot packs.
[[nodiscard]] bool disk_path(std::string_view path, std::string& out);

}  // namespace baker_godot
