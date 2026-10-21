#pragma once

// BakerRuntime: the game's Baker, as the autoload `Baker` (addons/baker/baker.gd extends
// it). Baker's host loop (baker::host::Live) owns the Config: it opens the stack the
// project file names and, in debug builds, reloads what was saved, watching the files
// through the operating system. Everything runs on the main thread.
//
//   var h = Baker.get_value("player.jump.height", 96.0)   # a read through the Config
//   var g = Baker.tweak("world.gravity", 980.0)           # cached until the data changes
//   Baker.toggle_layer("floaty")                          # an experiment, A/B
//
// Properties of scene nodes are bound with a BakerBinding instead, which costs nothing per
// frame.
//
// Under the editor's debugger, the game sends its session (the unsaved values, the layers'
// states, the bindings) to the editor whenever it changes, as the message baker:session,
// and takes baker:toggle [layer, enabled], baker:kept [path] and baker:discard [path]. In
// the editor, a BakerRuntime opened by the plugin may write into the project
// (project_writes), so that "Keep" writes a value back from the editor process.
//
// For tools and agents, in debug builds (the user arguments after `--`):
//   --console-port N    Baker's loopback console, with the Config tools bound (0: a free
//                       port, which it prints and get_console_port() gives):
//                       {config.explain p}, {config.set p v}, {config.toggle layer}, ...
//   --console-write     also config.write_back and config.promote
//   --baker COMMAND...  run one of Baker's tool commands (check, reference, eval, diff,
//                       promote --patch, sim, compare) once the scene is ready, then quit;
//                       --out FILE writes its output there too. Shipping builds run sim
//                       and compare only.
//
// sim runs the project's scenarios (sim/scenarios.dta) from the game's own frames: before
// each run it reloads the current scene, whose script calls sim_ready() once it is set up,
// then each physics tick is one step of the run, at the rate the run asks for. Functions a
// script binds (bind_function) are Baker's script functions: in scenarios and objectives,
// in check, and at the console.
//   --no-local          leave out the `local` layer; every --baker command does, in shipping
//                       builds too

#include <baker/config.hpp>
#include <baker/host/commands.hpp>
#include <baker/host/live.hpp>
#include <baker/script.hpp>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <memory>
#include <string>
#include <vector>

namespace baker_godot {

class BakerRuntime : public godot::Node {
    GDCLASS(BakerRuntime, godot::Node)

public:
    BakerRuntime();
    ~BakerRuntime() override;

    // The runtime in the scene tree, or null.
    [[nodiscard]] static BakerRuntime* get_singleton() noexcept { return singleton_; }
    // Null until the project is open.
    // Null until the project is open, and while a tool command or sim runs, which owns its
    // own Config: reads go to that one (the const overload), edits are refused.
    [[nodiscard]] baker::Config* config() noexcept { return live_ ? &live_->config() : nullptr; }
    [[nodiscard]] const baker::Config* config() const noexcept {
        return live_ ? &live_->config() : tool_config_;
    }

    void _enter_tree() override;
    void _exit_tree() override;
    void _process(double delta) override;
    void _physics_process(double delta) override;

    // --- The stack. ---
    // Opens the project file: its layers, pack levels, include directories, and defines.
    // A project file at res:// is opened on entering the tree.
    godot::Error open(const godot::String& project_path);
    [[nodiscard]] bool is_open() const noexcept { return live_ != nullptr; }
    // [{name, enabled, file}], bottom to top, ending with "scratch".
    [[nodiscard]] godot::Array get_layers() const;
    godot::Error set_layer_enabled(const godot::String& layer, bool enabled);
    // The new state.
    bool toggle_layer(const godot::String& layer);
    // Re-reads the files that changed; true if anything did.
    bool reload();

    // --- Reading. ---
    // The value at `path`, read as `fallback`'s type; `fallback` if the path is missing or
    // does not convert.
    [[nodiscard]] godot::Variant get_value(const godot::String& path,
                                           const godot::Variant& fallback) const;
    // get_value, cached until the data next changes: for one-off reads in GDScript.
    godot::Variant tweak(const godot::String& path, const godot::Variant& fallback);
    // {layer, file, line}; empty if the path is missing.
    [[nodiscard]] godot::Dictionary source(const godot::String& path) const;
    // Every contributor to the value, bottom to top: [{layer, value, file, line, standing}].
    [[nodiscard]] godot::Array explain(const godot::String& path) const;

    // --- Live edits and the journal. ---
    // Sets the value in the scratch layer, unsaved; bound properties follow.
    godot::Error set_value(const godot::String& path, const godot::Variant& value);
    godot::Error clear_scratch(const godot::String& path);
    [[nodiscard]] godot::PackedStringArray unsaved() const;
    // [{path, before, after, layer, file, line, fate}], oldest first.
    [[nodiscard]] godot::Array journal() const;
    // Writes the value into the file of the highest enabled layer that supplies it,
    // replacing only its tokens: comments and formatting stay, and the diff is one line.
    godot::Error write_back(const godot::String& path);
    // Writes the value into a layer's file: for the game, a user:// layer such as `local`.
    godot::Error write_back_to(const godot::String& path, const godot::String& layer);
    // What the last write wrote: [{file, godot_path, line}].
    [[nodiscard]] godot::Array get_last_written() const { return last_written_; }
    // What a layer changes: [{path, value, below, file, line, shadowed}].
    [[nodiscard]] godot::Array layer_effect(const godot::String& layer) const;
    // What the editor is sent: {layers, unsaved: [{path, value, before, layer, file, line}],
    // bindings: [{object_id, node, property, path}]}. `layer`, `file` and `line` are where
    // the value came from before it was tuned: where Keep writes it.
    [[nodiscard]] godot::Dictionary session() const;

    // --- Tools and agents. ---
    // Answers one console line: a request (ID {command}) with one line of baker.reply/1.1
    // JSON, anything else with text. Debug builds only; the Config tools that write files
    // are bound only with --console-write.
    godot::String eval_request(const godot::String& line);
    // Binds `function` as Baker's script function `name`: {name arg...} calls it with the
    // arguments as Godot values, and its result is the call's value (nil gives 0). A call
    // with more arguments than it takes, or fewer than it needs without its defaults, is an
    // error at the call. Bound in the console's Context now, and in every tool command's.
    // Binding a name again replaces the function, so a scene that is reloaded binds its own.
    void bind_function(const godot::String& name, const godot::Callable& function);
    // Under --baker sim: whether a run is in progress, and, from the scene the run reloaded,
    // that it is set up and the run may step.
    [[nodiscard]] bool is_simulating() const noexcept { return sim_ != nullptr; }
    void sim_ready();
    // The console's port, or 0 if none listens.
    [[nodiscard]] int get_console_port() const;
    // A shipping build (an export): no reloads, live edits, write-back, console or tools.
    [[nodiscard]] static bool is_shipping() { return baker::kShipping; }
    // The files of the layers a project file marks dev_only, as Godot paths: what an export
    // leaves out. Empty if the project file does not read.
    [[nodiscard]] static godot::PackedStringArray dev_only_files(const godot::String& project_path);

    // --- Telemetry. ---
    // {reloads, unchanged, failures, files: [...], layers: [...]}, times in milliseconds.
    [[nodiscard]] godot::Dictionary loop_stats() const;
    [[nodiscard]] godot::String get_last_error() const { return last_error_; }

    void set_project_path(const godot::String& path) { project_path_ = path; }
    [[nodiscard]] godot::String get_project_path() const { return project_path_; }
    void set_watch(bool watch) { watch_ = watch; }
    [[nodiscard]] bool get_watch() const { return watch_; }
    // Writes into res:// allowed: only in the editor, never in a game.
    void set_project_writes(bool writes);
    [[nodiscard]] bool get_project_writes() const { return project_writes_; }

protected:
    static void _bind_methods();

private:
    // After every change of the data: clears tweak()'s cache and emits `changed`.
    void changed();
    // changed(), unless it was already called for the Config's epoch: Live reports at the
    // next frame what a call here already reported.
    void changed_since();
    godot::Error fail(const baker::Error& error);
    void present();
    godot::Error wrote(const baker::WriteReport& report);
    void read_arguments();
    std::string answer(const std::string& line);
    void run_tool_command();
    void bind_functions(baker::Context& ctx);
    void bind_into(baker::Context& ctx, baker::Symbol name);
    void finish_tool(int status);
    bool debugger_message(const godot::String& message, const godot::Array& data);

    static BakerRuntime* singleton_;
    godot::String project_path_ = "res://baker-project.dta";
    godot::String last_error_;
    godot::Dictionary cache_;  // tweak(): path -> value
    bool watch_ = true;        // watch the layers' files (debug builds), from the next open
    bool project_writes_ = false;
    bool capturing_ = false;      // registered with the editor's debugger
    bool session_dirty_ = false;  // the session changed since it was last sent
    godot::Array last_written_;

    // Live binds the Config's tools into the Context, which outlives it.
    std::unique_ptr<baker::Context> context_;
    std::unique_ptr<baker::host::Live> live_;
    static constexpr uint64_t kNoEpoch = ~uint64_t(0);
    uint64_t seen_epoch_ = kNoEpoch;  // the Config's epoch() when changed() last ran

    // Tools and agents (debug builds).
    std::vector<std::string> tool_args_;  // --baker COMMAND ...: run once the scene is ready
    std::vector<const char*> tool_argv_;  // what the command line's views point into
    std::string tool_project_;            // the project file, as the commands open it
    std::string tool_output_;             // what the command wrote
    baker::host::CommandHooks tool_hooks_;
    const baker::Config* tool_config_ = nullptr;  // the command's, once it has loaded
    std::unique_ptr<baker::host::SimRun> sim_;    // --baker sim, in progress
    bool sim_reloading_ = false;                  // the scene is being reset for the next run
    godot::Dictionary functions_;                 // bind_function: name -> Callable
    std::string tool_out_;                        // --out FILE
    int tool_frames_ = 0;
    int console_port_ = -1;  // --console-port N; -1: none
    bool console_write_ = false;
    bool skip_local_ = false;
};

}  // namespace baker_godot
