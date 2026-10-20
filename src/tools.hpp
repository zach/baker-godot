#pragma once

// The game as its own tool, for agents, CI and people, run headless with the scene loaded
// (so the bound properties' tweaks exist):
//
//   godot --headless --path . -- --baker check [--json]
//   godot --headless --path . -- --baker reference [--json]
//   godot --headless --path . -- --baker eval [--json] "{config.explain player.jump.height}"...
//   godot --headless --path . -- --baker diff
//   godot --headless --path . -- --baker promote [--to default] --patch FILE
//
// Each prints its result (Baker's JSON with --json: baker.check/1, baker.reference/1 with
// baker.tweaks/1 and baker.shapes/1, one baker.reply/1.1 line per command), and with
// --out FILE writes it there too. The layers are the project's without `local`, so a run
// does not depend on whose machine it is on. Nothing writes into the project: promote
// gives a patch for a person to apply. Exit status: 0, 1 if the check or a command failed,
// 2 if the command could not run.

#include <baker/config.hpp>
#include <baker/script.hpp>

#include <string>
#include <vector>

namespace baker_godot {

struct ToolResult {
    int status = 0;
    std::string out;  // what the command prints
    std::string err;  // what goes to stderr
};

// Runs one command over an open Config and a Context with its Config tools bound. `args`
// begins with the command's name.
[[nodiscard]] ToolResult run_tool(const std::vector<std::string>& args, baker::Config& cfg,
                                  baker::Context& ctx);

// Writes a tool's output: a path on disk, as given (relative to where the tool runs), or a
// res:// or user:// path.
[[nodiscard]] bool write_output(const std::string& path, const std::string& text);

// A unified diff of `before` and `after`, named a/`name` and b/`name`, with three lines of
// context: git apply -p1 applies it from the project's root. Empty if they are the same.
[[nodiscard]] std::string unified_diff(const std::string& before, const std::string& after,
                                       const std::string& name);

}  // namespace baker_godot
