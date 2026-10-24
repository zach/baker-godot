#pragma once

// BakerRuntime: the game's Baker, as the autoload `Baker` (addons/baker/baker.gd extends
// it). Baker's host loop (baker::host::Live) owns the Config: it opens the stack the
// project file names and, in debug builds, reloads what was saved, watching the files
// through the operating system. A reload lands at the start of a frame, before its first
// physics step or else its process step. Everything runs on the main thread.
//
//   var h = Baker.get_value("player.jump.height", 96.0)   # a read through the Config
//   var g = Baker.tweak("world.gravity", 980.0)           # cached until the data changes
//   Baker.toggle_layer("floaty")                          # an experiment, A/B
//   var d = Baker.call_function("skirmish.damage", [6, 2, 0])  # a rule the data defines
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
//                       promote --baker-patch, sim, compare) once the scene is ready, then quit;
//                       --baker-output FILE writes its output there too. Shipping builds run sim
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
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <functional>
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
    // A layer the game finds after it starts: a content pack, a downloaded map's settings,
    // a mod. On top of the stack, or where `options` say: {optional, enabled, below, above}
    // (below or above a layer's name). Its file is watched as the project's layers are.
    godot::Error push_layer(const godot::String& name, const godot::String& path,
                            const godot::Dictionary& options);
    // A layer from text the game received (from a server, a mod store, a save): read from
    // the text alone, never a cache, and an #include in it is refused, so content from
    // outside the game reaches no other file. Write-back refuses its values: it has no file.
    godot::Error push_layer_text(const godot::String& name, const godot::String& text,
                                 const godot::Dictionary& options);
    // Removes a layer, leaving no trace: what it set goes back to the layers below.
    godot::Error remove_layer(const godot::String& name);
    // Binds a Resource's properties to paths, as a BakerBinding binds a node's: the same
    // shared tweaks, keyed paths ({kind}) and property_changed. A shader's parameters are a
    // ShaderMaterial's properties ("shader_parameter/glow"). Returns the binding, a child of
    // this runtime: free it, or unbind_resource, to unbind.
    class BakerBinding* bind_resource(const godot::Ref<godot::Resource>& resource,
                                      const godot::Dictionary& bindings);
    void unbind_resource(const godot::Ref<godot::Resource>& resource);
    // Binds every resource in `dir` (.tres, .res) as a record of the table `prefix`: the
    // properties under its script's @export_group `group` (default "Tuning") bind to
    // "prefix.KEY.property", where KEY is the file's name, or the field `key` names. The
    // data holds only what a layer changes; the resource's own value is the bottom of the
    // chain, and explain() names it. A property that holds a resource of a class bound as
    // records is a key in the data, its tweak's one_of the table's keys: a layer retunes a
    // reference by naming another record. A Curve is its points. Binding a prefix again
    // replaces it; the number of records bound.
    int64_t bind_records(const godot::String& prefix, const godot::String& dir,
                         const godot::Dictionary& options);
    void unbind_records(const godot::String& prefix);
    // A record by its key, or null; every record of a table, {key: resource}.
    [[nodiscard]] godot::Ref<godot::Resource> record(const godot::String& prefix,
                                                     const godot::String& key) const;
    [[nodiscard]] godot::Dictionary records(const godot::String& prefix) const;
    // The table whose records are of `class_name` (a script's global name), or empty.
    [[nodiscard]] godot::String record_table(const godot::String& class_name) const;
    // A record's key in its table, or empty.
    [[nodiscard]] godot::String record_key(const godot::String& prefix,
                                           const godot::Object* resource) const;
    // Re-reads the files that changed; true if anything did.
    bool reload();

    // --- A match, in lockstep. ---
    // The data as the game reads it, every enabled layer merged and the live edits, as
    // sixteen hex digits: equal data gives an equal fingerprint on every platform. Peers
    // compare theirs before a match and at each turn's boundary.
    [[nodiscard]] godot::String fingerprint() const;
    // Locks the data for a match, and returns its fingerprint. Until unlock(), what would
    // change the data waits, in order: a toggle (from the game or the editor), a live edit,
    // a layer pushed or removed, a reload, a file saved; and the console waits with it.
    // toggle_layer() returns the state the layer will have. Reads see the locked data.
    godot::String lock();
    // At a turn's boundary: applies what waited, all at once, and returns the new
    // fingerprint. The data stays locked.
    godot::String apply_held();
    // Applies what waited, and unlocks.
    void unlock();
    [[nodiscard]] bool is_locked() const noexcept { return locked_; }
    [[nodiscard]] int held_count() const noexcept { return int(held_.size()); }

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
    // Sets the value in the scratch layer, unsaved; bound properties follow. A Dictionary
    // sets each field it names, each a live edit of its own path, so a record's other fields
    // stay: set_value("units.tank", {"price": 45}).
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
    // Calls the script function `name` with `args` and returns its result as a Godot value:
    // a rule the data defines ({func ...} in a layer's #autorun, which a layer above defines
    // again to change it), or a function a script binds. Each argument is one value: an
    // Array or a Vector2 is one list. Nil, with the error pushed, if the call fails. While a
    // tool command or sim runs, in that command's Context.
    godot::Variant call_function(const godot::String& name, const godot::Array& args);
    // Evaluates Baker script text, as the console does, and returns its value.
    godot::Variant eval(const godot::String& text);
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
    // While locked, keeps `change` for apply_held(); true if it was kept.
    bool hold(std::function<void()> change);
    // Live's frame, at the start of the engine's: before the first physics step, else before
    // process, so that no node, and no work a node starts, sees part of a reload.
    void begin_frame();
    void physics_frame();
    void process_frame();
    void present();
    godot::Error wrote(const baker::WriteReport& report);
    void read_arguments();
    std::string answer(const std::string& line);
    void run_tool_command();
    void bind_functions(baker::Context& ctx);
    void bind_into(baker::Context& ctx, baker::Symbol name);
    // The Context scripts run in: a tool command's or sim's while one runs, else the game's.
    [[nodiscard]] baker::Context* script_context() noexcept;
    godot::Variant evaluated(const baker::Expected<baker::Node>& result);
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
    bool frame_begun_ = false;    // Live's frame began in this iteration of the main loop
    bool locked_ = false;
    std::vector<std::function<void()>> held_;  // what waits for apply_held()
    godot::Array last_written_;
    godot::Dictionary records_;        // bind_records: prefix -> {key: resource}
    godot::Dictionary record_tables_;  // a record's script class -> its prefix

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
    baker::Context* tool_context_ = nullptr;      // the command's, while its data is loaded
    std::unique_ptr<baker::host::SimRun> sim_;    // --baker sim, in progress
    bool sim_reloading_ = false;                  // the scene is being reset for the next run
    godot::Dictionary functions_;                 // bind_function: name -> Callable
    std::string tool_out_;                        // --baker-output FILE
    int tool_frames_ = 0;
    int console_port_ = -1;  // --console-port N; -1: none
    bool console_write_ = false;
    bool skip_local_ = false;
};

}  // namespace baker_godot
