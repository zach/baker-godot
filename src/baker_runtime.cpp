#include "baker_runtime.hpp"

#include "baker_binding.hpp"
#include "godot_files.hpp"
#include "tools.hpp"
#include "values.hpp"

#include <baker/error.hpp>
#include <baker/project.hpp>
#include <baker/std_interop.hpp>
#include <baker/std_script.hpp>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdio>
#include <string>
#include <string_view>

namespace baker_godot {

using godot::Array;
using godot::Dictionary;
using godot::String;
using godot::Variant;

BakerRuntime* BakerRuntime::singleton_ = nullptr;

namespace {

String name_of(baker::Symbol s) {
    return to_godot(s.str());
}

void put_source(Dictionary& d, const baker::Source& s) {
    d["layer"] = name_of(s.layer);
    d["file"] = name_of(s.file);
    d["line"] = s.line;
}

const char* standing_name(baker::Standing s) {
    switch (s) {
        case baker::Standing::Wins: return "wins";
        case baker::Standing::Shadowed: return "shadowed";
        case baker::Standing::Removes: return "removes";
        case baker::Standing::Disabled: return "disabled";
    }
    return "";
}

const char* fate_name(baker::EditFate f) {
    switch (f) {
        case baker::EditFate::Unsaved: return "unsaved";
        case baker::EditFate::Written: return "written";
        case baker::EditFate::Reverted: return "reverted";
        case baker::EditFate::Superseded: return "superseded";
    }
    return "";
}

// A journal entry's value: nil for none, one node as itself, several as an Array.
Variant journal_value(const baker::Node (&nodes)[4], uint32_t count) {
    if (count == 0) return {};
    const uint32_t kept = count < 4 ? count : 4;
    return to_variant(baker::Values({nodes, kept}, nullptr), Variant::NIL);
}

// The project file as Baker names it: relative to the project root for a res:// path, so
// its layers' paths stay project paths.
std::string project_file(const String& path) {
    std::string p = to_std(path);
    if (p.rfind("res://", 0) == 0) p.erase(0, 6);
    return p;
}

}  // namespace

BakerRuntime::BakerRuntime() = default;
BakerRuntime::~BakerRuntime() = default;

void BakerRuntime::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("open", "project_path"), &BakerRuntime::open);
    ClassDB::bind_method(D_METHOD("is_open"), &BakerRuntime::is_open);
    ClassDB::bind_method(D_METHOD("get_layers"), &BakerRuntime::get_layers);
    ClassDB::bind_method(D_METHOD("set_layer_enabled", "layer", "enabled"),
                         &BakerRuntime::set_layer_enabled);
    ClassDB::bind_method(D_METHOD("toggle_layer", "layer"), &BakerRuntime::toggle_layer);
    ClassDB::bind_method(D_METHOD("reload"), &BakerRuntime::reload);
    ClassDB::bind_method(D_METHOD("get_value", "path", "fallback"), &BakerRuntime::get_value,
                         DEFVAL(Variant()));
    ClassDB::bind_method(D_METHOD("tweak", "path", "fallback"), &BakerRuntime::tweak);
    ClassDB::bind_method(D_METHOD("source", "path"), &BakerRuntime::source);
    ClassDB::bind_method(D_METHOD("explain", "path"), &BakerRuntime::explain);
    ClassDB::bind_method(D_METHOD("set_value", "path", "value"), &BakerRuntime::set_value);
    ClassDB::bind_method(D_METHOD("clear_scratch", "path"), &BakerRuntime::clear_scratch,
                         DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("unsaved"), &BakerRuntime::unsaved);
    ClassDB::bind_method(D_METHOD("journal"), &BakerRuntime::journal);
    ClassDB::bind_method(D_METHOD("write_back", "path"), &BakerRuntime::write_back);
    ClassDB::bind_method(D_METHOD("get_last_written"), &BakerRuntime::get_last_written);
    ClassDB::bind_method(D_METHOD("layer_effect", "layer"), &BakerRuntime::layer_effect);
    ClassDB::bind_method(D_METHOD("session"), &BakerRuntime::session);
    ClassDB::bind_method(D_METHOD("set_project_writes", "writes"),
                         &BakerRuntime::set_project_writes);
    ClassDB::bind_method(D_METHOD("get_project_writes"), &BakerRuntime::get_project_writes);
    ClassDB::bind_method(D_METHOD("write_back_to", "path", "layer"), &BakerRuntime::write_back_to);
    ClassDB::bind_method(D_METHOD("eval_request", "line"), &BakerRuntime::eval_request);
    ClassDB::bind_method(D_METHOD("get_console_port"), &BakerRuntime::get_console_port);
    ClassDB::bind_static_method("BakerRuntime", D_METHOD("is_shipping"),
                                &BakerRuntime::is_shipping);
    ClassDB::bind_method(D_METHOD("loop_stats"), &BakerRuntime::loop_stats);
    ClassDB::bind_method(D_METHOD("get_last_error"), &BakerRuntime::get_last_error);
    ClassDB::bind_method(D_METHOD("set_project_path", "path"), &BakerRuntime::set_project_path);
    ClassDB::bind_method(D_METHOD("get_project_path"), &BakerRuntime::get_project_path);
    ClassDB::bind_method(D_METHOD("set_watch", "watch"), &BakerRuntime::set_watch);
    ClassDB::bind_method(D_METHOD("get_watch"), &BakerRuntime::get_watch);
    ClassDB::bind_method(D_METHOD("present"), &BakerRuntime::present);
    ADD_PROPERTY(
        godot::PropertyInfo(Variant::STRING, "project_path", godot::PROPERTY_HINT_FILE, "*.dta"),
        "set_project_path", "get_project_path");
    ADD_PROPERTY(godot::PropertyInfo(Variant::BOOL, "watch"), "set_watch", "get_watch");
    ADD_PROPERTY(godot::PropertyInfo(Variant::BOOL, "project_writes"), "set_project_writes",
                 "get_project_writes");
    // After every change of the data: a reload, a layer toggled, a live edit.
    ADD_SIGNAL(godot::MethodInfo("changed"));
}

void BakerRuntime::_enter_tree() {
    if (godot::Engine::get_singleton()->is_editor_hint()) return;
    if (singleton_ && singleton_ != this) {
        godot::UtilityFunctions::push_error("Baker: a second BakerRuntime; only one is used");
        return;
    }
    singleton_ = this;
    read_arguments();
    if (!live_ && godot::FileAccess::file_exists(project_path_)) (void)open(project_path_);
    if constexpr (!baker::kShipping) {
        set_process(true);
        if (get_console_port())
            godot::UtilityFunctions::print("Baker: console on 127.0.0.1:", get_console_port());
        // Edit-to-see ends when the frame showing the edit is drawn.
        godot::RenderingServer::get_singleton()->connect("frame_post_draw",
                                                         callable_mp(this, &BakerRuntime::present));
        godot::EngineDebugger* debugger = godot::EngineDebugger::get_singleton();
        if (debugger->is_active() && !debugger->has_capture("baker")) {
            debugger->register_message_capture("baker",
                                               callable_mp(this, &BakerRuntime::debugger_message));
            capturing_ = true;
            session_dirty_ = true;
        }
    }
}

void BakerRuntime::_exit_tree() {
    if (singleton_ != this) return;
    if constexpr (!baker::kShipping) {
        godot::RenderingServer* rs = godot::RenderingServer::get_singleton();
        const godot::Callable present = callable_mp(this, &BakerRuntime::present);
        if (rs->is_connected("frame_post_draw", present))
            rs->disconnect("frame_post_draw", present);
        if (capturing_) godot::EngineDebugger::get_singleton()->unregister_message_capture("baker");
        capturing_ = false;
    }
    live_.reset();
    context_.reset();
    singleton_ = nullptr;
}

godot::Error BakerRuntime::open(const String& project_path) {
    if (&project_path != &project_path_)
        project_path_ = project_path;  // a self-assignment empties it
    // A Config binds the global tweak registry: close the old one before making another.
    live_.reset();
    context_.reset();
    seen_epoch_ = kNoEpoch;
    context_ = std::make_unique<baker::Context>();
    live_ = std::make_unique<baker::host::Live>(*context_);
    live_->config().set_writer([this](std::string_view path, std::string_view text) {
        return write_file(path, text, project_writes_);
    });
    const std::string file = project_file(project_path);
    const bool game = !godot::Engine::get_singleton()->is_editor_hint();
    baker::host::LiveOptions options{
        .project = file,
        .reader = reader(),
        .disk_path =
            [](std::string_view path, baker::TextSink out) {
                std::string disk;
                if (!disk_path(path, disk)) return false;
                out(disk);
                return true;
            },
        .watch = watch_ && game && godot::OS::get_singleton()->is_debug_build(),
        .console_port = uint16_t(console_port_ > 0 ? console_port_ : 0),
        .console_write = console_write_,
        .changed = [this](const baker::Config&) { changed_since(); },
        .log =
            [](const baker::Error& e) {
                godot::UtilityFunctions::push_warning("Baker: ", to_godot(baker::to_string(e)));
            },
    };
    if (skip_local_) (void)options.open.skip.add("local");
    if (auto r = live_->open(options); !r) {
        live_.reset();
        context_.reset();
        return fail(r.error());
    }
    files_epoch_ = live_->config().stack_files_epoch();  // as Live watched them
    return godot::OK;
}

void BakerRuntime::_process(double) {
    if constexpr (!baker::kShipping) {
        if (!tool_args_.empty() && ++tool_frames_ == 2) run_tool_command();
        if (!live_) return;
        live_->begin_frame();  // reloads what was saved; answers the console
        // The stack's files changed (one appeared, an #include was added): Live watched them
        // anew, which takes an edit made since the last poll for seen (requirement 0008).
        // One reload catches it.
        if (const uint64_t files = config()->stack_files_epoch(); files != files_epoch_) {
            files_epoch_ = files;
            (void)reload();
        }
        if (capturing_ && session_dirty_) {
            session_dirty_ = false;
            godot::EngineDebugger::get_singleton()->send_message("baker:session",
                                                                 Array::make(session()));
        }
    }
}

void BakerRuntime::read_arguments() {
    const godot::PackedStringArray args = godot::OS::get_singleton()->get_cmdline_user_args();
    for (int64_t i = 0; i < args.size(); ++i) {
        const std::string a = to_std(args[i]);
        const bool more = i + 1 < args.size();
        if (a == "--no-local" || a == "--baker") {
            skip_local_ = true;  // a run's results do not depend on whose machine it is on
            if (a == "--no-local") continue;
            // sim and compare run in the scene (scenarios/runner.gd), shipping builds too;
            // every other command is Baker's, in development builds only.
            const std::string command = more ? to_std(args[i + 1]) : "";
            if (is_shipping() || command == "sim" || command == "compare") break;
            for (++i; i < args.size(); ++i) {
                const std::string t = to_std(args[i]);
                if (t == "--out" && i + 1 < args.size())
                    tool_out_ = to_std(args[++i]);
                else
                    tool_args_.push_back(t);
            }
            if (tool_args_.empty()) tool_args_.push_back("");
        } else if constexpr (baker::kShipping) {
            continue;  // no console in a shipping build
        } else if (a == "--console-port" && more) {
            console_port_ = int(args[++i].to_int());
        } else if (a == "--console-write") {
            console_write_ = true;
        }
    }
}

std::string BakerRuntime::answer(const std::string& line) {
    if (!live_) return "error: no project is open";
    baker::Context* ctx = context_.get();
    std::string reply;
    if (const size_t first = line.find_first_not_of(" \t");
        first != std::string::npos && line[first] == '(') {
        baker::eval_request(*ctx, line, baker::string_out(reply));
        if (!reply.empty() && reply.back() == '\n') reply.pop_back();
    } else {
        auto r = ctx->eval(line, "<console>");
        reply = r ? "=> " + baker::to_string(*r) : std::string(baker::describe(r.error()));
        if (!reply.empty() && reply.back() == '\n') reply.pop_back();
    }
    changed_since();  // a command may have changed the data
    return reply;
}

String BakerRuntime::eval_request(const String& line) {
    if constexpr (baker::kShipping) {
        return "error: no console in a shipping build";
    } else {
        return to_godot(answer(to_std(line)));
    }
}

int BakerRuntime::get_console_port() const {
    return live_ ? int(live_->console_port()) : 0;
}

// --baker COMMAND: runs once the scene is ready, its bound properties' tweaks registered,
// then quits with the command's status.
void BakerRuntime::run_tool_command() {
    ToolResult r;
    if (!live_)
        r = {2, {}, "baker: " + to_std(last_error_) + "\n"};
    else
        r = run_tool(tool_args_, live_->config(), *context_);
    std::fwrite(r.out.data(), 1, r.out.size(), stdout);
    std::fflush(stdout);
    std::fwrite(r.err.data(), 1, r.err.size(), stderr);
    std::fflush(stderr);
    if (!tool_out_.empty() && !write_output(tool_out_, r.out)) r.status = 2;
    tool_args_.clear();
    get_tree()->quit(r.status);
}

// The editor's messages: the prefix "baker:" is taken off before they reach here.
bool BakerRuntime::debugger_message(const String& message, const Array& data) {
    if (message == "toggle" && data.size() == 2) {
        (void)set_layer_enabled(data[0], data[1]);
        return true;
    }
    if ((message == "kept" || message == "discard") && data.size() == 1) {
        // Kept: the file now holds the value, which the watcher reloads; discarded: the
        // value goes back to the files'. Either way scratch lets go of it.
        (void)clear_scratch(data[0]);
        return true;
    }
    return false;
}

void BakerRuntime::set_project_writes(bool writes) {
    if (writes && !godot::Engine::get_singleton()->is_editor_hint()) {
        godot::UtilityFunctions::push_error("Baker: a game never writes into the project");
        return;
    }
    project_writes_ = writes;
}

void BakerRuntime::present() {
    if (live_) live_->end_frame();
}

bool BakerRuntime::reload() {
    if (!live_) return false;
    const bool reloaded = live_->reload_now();
    changed_since();
    return reloaded;
}

void BakerRuntime::changed_since() {
    if (live_ && live_->config().epoch() != seen_epoch_) changed();
}

void BakerRuntime::changed() {
    if (live_) seen_epoch_ = live_->config().epoch();
    session_dirty_ = true;
    cache_.clear();
    emit_signal("changed");
}

godot::Error BakerRuntime::fail(const baker::Error& error) {
    last_error_ = to_godot(baker::to_string(error));
    godot::UtilityFunctions::push_error("Baker: ", last_error_);
    return godot::FAILED;
}

Array BakerRuntime::get_layers() const {
    Array out;
    if (!config()) return out;
    for (baker::Symbol name : config()->layers()) {
        Dictionary d;
        d["name"] = name_of(name);
        d["enabled"] = config()->layer_enabled(name.str());
        out.push_back(d);
    }
    return out;
}

godot::Error BakerRuntime::set_layer_enabled(const String& layer, bool enabled) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    const std::string name = to_std(layer);
    if (config()->layer_enabled(name) == enabled) return godot::OK;
    if (auto r = config()->set_layer_enabled(name, enabled); !r) return fail(r.error());
    changed();
    return godot::OK;
}

bool BakerRuntime::toggle_layer(const String& layer) {
    if (!config()) return false;
    auto r = config()->toggle_layer(to_std(layer));
    if (!r) return fail(r.error()), false;
    changed();
    return *r;
}

Variant BakerRuntime::get_value(const String& path, const Variant& fallback) const {
    if (!config()) return fallback;
    const baker::Path p(to_std(path));
    auto r = config()->get<RawValues>(p, RawValues{});
    if (!r) {
        godot::UtilityFunctions::push_warning("Baker: ", to_godot(baker::to_string(r.error())));
        return fallback;
    }
    if (r->nodes.empty()) return fallback;
    const Variant v = to_variant(r->values(), fallback.get_type());
    return v.get_type() == Variant::NIL ? fallback : v;
}

Variant BakerRuntime::tweak(const String& path, const Variant& fallback) {
    if (cache_.has(path)) return cache_[path];
    const Variant v = get_value(path, fallback);
    cache_[path] = v;
    return v;
}

Dictionary BakerRuntime::source(const String& path) const {
    Dictionary d;
    if (!config()) return d;
    if (auto s = config()->source(baker::Path(to_std(path)))) put_source(d, *s);
    return d;
}

Array BakerRuntime::explain(const String& path) const {
    Array out;
    if (!config()) return out;
    auto entries = config()->explain(baker::Path(to_std(path)));
    if (!entries) return out;
    for (const baker::ExplainEntry& e : *entries) {
        Dictionary d;
        put_source(d, e.source);
        d["layer"] = name_of(e.layer);
        d["value"] = to_variant(e.value);
        d["standing"] = standing_name(e.standing);
        out.push_back(d);
    }
    return out;
}

godot::Error BakerRuntime::set_value(const String& path, const Variant& value) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    baker::ValueList values;
    if (!to_values(value, values)) {
        last_error_ =
            "Baker: a " + Variant::get_type_name(value.get_type()) + " cannot be set as data";
        godot::UtilityFunctions::push_error(last_error_);
        return godot::ERR_INVALID_PARAMETER;
    }
    if (auto r =
            config()->set(baker::Path(to_std(path)), std::span<const baker::Node>(values.nodes()));
        !r)
        return fail(r.error());
    changed();
    return godot::OK;
}

godot::Error BakerRuntime::clear_scratch(const String& path) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (auto r = config()->clear_scratch(to_std(path)); !r) return fail(r.error());
    changed();
    return godot::OK;
}

godot::PackedStringArray BakerRuntime::unsaved() const {
    godot::PackedStringArray out;
    if (config())
        for (baker::Symbol path : config()->unsaved()) out.push_back(name_of(path));
    return out;
}

Array BakerRuntime::journal() const {
    Array out;
    if (!config()) return out;
    config()->journal([&](const baker::JournalEntry& e) {
        Dictionary d;
        put_source(d, e.source);
        d["path"] = name_of(e.path);
        d["before"] = journal_value(e.before, e.before_count);
        d["after"] = journal_value(e.after, e.after_count);
        d["fate"] = fate_name(e.fate);
        out.push_back(d);
    });
    return out;
}

godot::Error BakerRuntime::write_back(const String& path) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    last_written_.clear();
    auto r = config()->write_back(baker::Path(to_std(path)));
    if (!r) return fail(r.error());
    return wrote(*r);
}

godot::Error BakerRuntime::write_back_to(const String& path, const String& layer) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    last_written_.clear();
    auto r = config()->write_back_to(baker::Path(to_std(path)), to_std(layer));
    if (!r) return fail(r.error());
    return wrote(*r);
}

godot::Error BakerRuntime::wrote(const baker::WriteReport& report) {
    for (const baker::WrittenLine& w : report.written) {
        Dictionary d;
        d["file"] = name_of(w.file);
        d["godot_path"] = to_godot(godot_path(w.file.str()));
        d["line"] = w.line;
        last_written_.push_back(d);
    }
    changed();
    return godot::OK;
}

Array BakerRuntime::layer_effect(const String& layer) const {
    Array out;
    if (!config()) return out;
    auto entries = config()->layer_effect(to_std(layer));
    if (!entries) return out;
    for (const baker::LayerEffectEntry& e : *entries) {
        Dictionary d;
        put_source(d, e.source);
        d["path"] = name_of(e.path);
        d["value"] = to_variant(e.value);
        d["below"] = e.below ? to_variant(*e.below) : Variant();
        d["shadowed"] = e.shadowed;
        out.push_back(d);
    }
    return out;
}

Dictionary BakerRuntime::session() const {
    Dictionary out;
    out["layers"] = get_layers();
    Array unsaved;
    if (config()) {
        // For each unsaved path, its oldest journal entry since it was last written or
        // reverted: the value before tuning, and where it came from.
        Dictionary first;
        config()->journal([&](const baker::JournalEntry& e) {
            const String path = name_of(e.path);
            if (e.fate == baker::EditFate::Written || e.fate == baker::EditFate::Reverted) {
                first.erase(path);
                return;
            }
            if (first.has(path)) return;
            Dictionary d;
            put_source(d, e.source);
            d["path"] = path;
            d["before"] = journal_value(e.before, e.before_count);
            first[path] = d;
        });
        for (baker::Symbol path : config()->unsaved()) {
            const String p = name_of(path);
            Dictionary d = first.has(p) ? Dictionary(first[p]) : Dictionary();
            d["path"] = p;
            d["value"] = get_value(p, Variant());
            unsaved.push_back(d);
        }
    }
    out["unsaved"] = unsaved;
    out["bindings"] = BakerBinding::all_bindings();
    return out;
}

Dictionary BakerRuntime::loop_stats() const {
    Dictionary d;
    if (!config()) return d;
    const baker::LoopStats stats = config()->loop_stats();
    d["reloads"] = stats.reloads;
    d["unchanged"] = stats.unchanged;
    d["failures"] = stats.failures;
    auto rows = [](const baker::List<baker::LoopRow>& list) {
        Array out;
        for (const baker::LoopRow& row : list) {
            Dictionary r;
            r["name"] = name_of(row.name);
            r["reloads"] = row.reloads;
            r["failures"] = row.failures;
            r["edits"] = row.edits;
            r["edit_median_ms"] = double(row.edit_median_ns) / 1e6;
            r["edit_worst_ms"] = double(row.edit_worst_ns) / 1e6;
            r["reload_median_ms"] = double(row.reload_median_ns) / 1e6;
            r["reload_worst_ms"] = double(row.reload_worst_ns) / 1e6;
            out.push_back(r);
        }
        return out;
    };
    d["files"] = rows(stats.files);
    d["layers"] = rows(stats.layers);
    return d;
}

}  // namespace baker_godot
