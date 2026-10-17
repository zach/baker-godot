#pragma once

// BakerRuntime: the game's Baker, as the autoload `Baker` (addons/baker/baker.gd extends
// it). It owns the Config, opens the stack the project file names, and, in debug builds,
// reloads what changed on disk once a frame. Everything runs on the main thread.
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

#include "godot_files.hpp"

#include <baker/config.hpp>
#include <baker/project.hpp>

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
    [[nodiscard]] baker::Config* config() noexcept { return config_.get(); }

    void _enter_tree() override;
    void _exit_tree() override;
    void _process(double delta) override;

    // --- The stack. ---
    // Opens the project file: its layers, pack levels, include directories, and defines.
    // A project file at res:// is opened on entering the tree.
    godot::Error open(const godot::String& project_path);
    [[nodiscard]] bool is_open() const noexcept { return config_ != nullptr; }
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
    godot::Error fail(const baker::Error& error);
    void present();
    void watch_files();
    void poll();
    godot::Error wrote(const baker::WriteReport& report);
    bool debugger_message(const godot::String& message, const godot::Array& data);

    static BakerRuntime* singleton_;
    std::unique_ptr<baker::Config> config_;
    godot::String project_path_ = "res://baker-project.dta";
    godot::String last_error_;
    godot::Dictionary cache_;  // tweak(): path -> value
    bool watch_ = true;        // poll the layers' files (debug builds)
    bool project_writes_ = false;
    bool capturing_ = false;      // registered with the editor's debugger
    bool session_dirty_ = false;  // the session changed since it was last sent
    godot::Array last_written_;

    struct Watched {
        std::string path;
        FileStamp stamp;
    };
    std::vector<Watched> watched_;
    static constexpr uint64_t kNoEpoch = ~uint64_t(0);
    uint64_t watched_epoch_ = kNoEpoch;  // stack_files_epoch() when watched_ was made
};

}  // namespace baker_godot
