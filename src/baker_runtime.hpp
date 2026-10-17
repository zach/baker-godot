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
    // Writes the value into a layer's file: for the game, a user:// layer such as `local`.
    godot::Error write_back_to(const godot::String& path, const godot::String& layer);

    // --- Telemetry. ---
    // {reloads, unchanged, failures, files: [...], layers: [...]}, times in milliseconds.
    [[nodiscard]] godot::Dictionary loop_stats() const;
    [[nodiscard]] godot::String get_last_error() const { return last_error_; }

    void set_project_path(const godot::String& path) { project_path_ = path; }
    [[nodiscard]] godot::String get_project_path() const { return project_path_; }
    void set_watch(bool watch) { watch_ = watch; }
    [[nodiscard]] bool get_watch() const { return watch_; }

protected:
    static void _bind_methods();

private:
    // After every change of the data: clears tweak()'s cache and emits `changed`.
    void changed();
    godot::Error fail(const baker::Error& error);
    void present();
    void poll();

    static BakerRuntime* singleton_;
    std::unique_ptr<baker::Config> config_;
    godot::String project_path_ = "res://baker-project.dta";
    godot::String last_error_;
    godot::Dictionary cache_;  // tweak(): path -> value
    bool watch_ = true;        // poll the layers' files (debug builds)

    struct Watched {
        std::string path;
        FileStamp stamp;
    };
    std::vector<Watched> watched_;
    double since_full_ = 0;  // seconds since reload() last re-read every file
};

}  // namespace baker_godot
