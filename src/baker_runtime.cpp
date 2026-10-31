#include "baker_runtime.hpp"

#include "baker_binding.hpp"
#include "godot_files.hpp"
#include "values.hpp"

#include <baker/error.hpp>
#include <baker/host/commands.hpp>
#include <baker/project.hpp>
#include <baker/std_interop.hpp>
#include <baker/std_script.hpp>
#include <baker/tools/layers.hpp>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

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

const char* standing_name(baker::tools::Standing s) {
    switch (s) {
        case baker::tools::Standing::Wins: return "wins";
        case baker::tools::Standing::Shadowed: return "shadowed";
        case baker::tools::Standing::Removes: return "removes";
        case baker::tools::Standing::Disabled: return "disabled";
        case baker::tools::Standing::Composes: return "composes";
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
    ClassDB::bind_method(D_METHOD("push_layer", "name", "path", "options"),
                         &BakerRuntime::push_layer, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("push_layer_text", "name", "text", "options"),
                         &BakerRuntime::push_layer_text, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("remove_layer", "name"), &BakerRuntime::remove_layer);
    ClassDB::bind_method(D_METHOD("push_layer_dir", "name", "dir", "options"),
                         &BakerRuntime::push_layer_dir, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("layer_text", "name"), &BakerRuntime::layer_text);
    ClassDB::bind_method(D_METHOD("set_layer_text", "name", "text"), &BakerRuntime::set_layer_text);
    ClassDB::bind_method(D_METHOD("set_defines", "defines"), &BakerRuntime::set_defines);
    ClassDB::bind_method(D_METHOD("get_defines"), &BakerRuntime::get_defines);
    ClassDB::bind_method(D_METHOD("bind_resource", "resource", "bindings"),
                         &BakerRuntime::bind_resource);
    ClassDB::bind_method(D_METHOD("unbind_resource", "resource"), &BakerRuntime::unbind_resource);
    ClassDB::bind_method(D_METHOD("bind_records", "prefix", "dir", "options"),
                         &BakerRuntime::bind_records, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("unbind_records", "prefix"), &BakerRuntime::unbind_records);
    ClassDB::bind_method(D_METHOD("record", "prefix", "key"), &BakerRuntime::record);
    ClassDB::bind_method(D_METHOD("records", "prefix"), &BakerRuntime::records);
    ClassDB::bind_method(D_METHOD("reload"), &BakerRuntime::reload);
    ClassDB::bind_method(D_METHOD("fingerprint"), &BakerRuntime::fingerprint);
    ClassDB::bind_method(D_METHOD("lock"), &BakerRuntime::lock);
    ClassDB::bind_method(D_METHOD("apply_held"), &BakerRuntime::apply_held);
    ClassDB::bind_method(D_METHOD("unlock"), &BakerRuntime::unlock);
    ClassDB::bind_method(D_METHOD("is_locked"), &BakerRuntime::is_locked);
    ClassDB::bind_method(D_METHOD("held_count"), &BakerRuntime::held_count);
    ClassDB::bind_method(D_METHOD("get_value", "path", "fallback"), &BakerRuntime::get_value,
                         DEFVAL(Variant()));
    ClassDB::bind_method(D_METHOD("tweak", "path", "fallback"), &BakerRuntime::tweak);
    ClassDB::bind_method(D_METHOD("source", "path"), &BakerRuntime::source);
    ClassDB::bind_method(D_METHOD("data_paths"), &BakerRuntime::data_paths);
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
    ClassDB::bind_method(D_METHOD("bind_function", "name", "function"),
                         &BakerRuntime::bind_function);
    ClassDB::bind_method(D_METHOD("call_function", "name", "args"), &BakerRuntime::call_function,
                         DEFVAL(Array()));
    ClassDB::bind_method(D_METHOD("eval", "text"), &BakerRuntime::eval);
    ClassDB::bind_method(D_METHOD("is_simulating"), &BakerRuntime::is_simulating);
    ClassDB::bind_method(D_METHOD("sim_ready"), &BakerRuntime::sim_ready);
    ClassDB::bind_static_method("BakerRuntime", D_METHOD("is_shipping"),
                                &BakerRuntime::is_shipping);
    ClassDB::bind_static_method("BakerRuntime", D_METHOD("dev_only_files", "project_path"),
                                &BakerRuntime::dev_only_files);
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
    if (!tool_args_.empty()) set_process(true);
    if constexpr (!baker::kShipping) {
        set_process(true);
        if (get_console_port())
            godot::UtilityFunctions::print("Baker: console on 127.0.0.1:", get_console_port());
        // Edit-to-see ends when the frame showing the edit is drawn.
        godot::RenderingServer::get_singleton()->connect("frame_post_draw",
                                                         callable_mp(this, &BakerRuntime::present));
        get_tree()->connect("physics_frame", callable_mp(this, &BakerRuntime::physics_frame));
        get_tree()->connect("process_frame", callable_mp(this, &BakerRuntime::process_frame));
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
        godot::SceneTree* tree = get_tree();
        const godot::Callable physics = callable_mp(this, &BakerRuntime::physics_frame);
        const godot::Callable process = callable_mp(this, &BakerRuntime::process_frame);
        if (tree->is_connected("physics_frame", physics))
            tree->disconnect("physics_frame", physics);
        if (tree->is_connected("process_frame", process))
            tree->disconnect("process_frame", process);
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
    bind_functions(*context_);
    live_ = std::make_unique<baker::host::Live>(*context_);
    live_->config().set_writer([this](std::string_view path, std::string_view text) {
        return write_file(path, text, project_writes_);
    });
    const std::string file = project_file(project_path);
    const bool game = !godot::Engine::get_singleton()->is_editor_hint();
    defines_ = feature_defines();
    const std::vector<std::string_view> defines(defines_.begin(), defines_.end());
    baker::host::LiveOptions options{
        .project = file,
        .reader = reader(),
        .lister =
            [](std::string_view dir, baker::Callback<void(std::string_view)> entry) {
                return list_files(dir, entry);
            },
        .disk_path =
            [](std::string_view path, baker::TextSink out) {
                std::string disk;
                if (!disk_path(path, disk)) return false;
                out(disk);
                return true;
            },
        .watch = watch_ && game && godot::OS::get_singleton()->is_debug_build(),
        .console_port = uint16_t(console_port_ > 0 ? console_port_ : 0),
        .console_any_port = console_port_ == 0,
        .console_write = console_write_,
        .changed = [this](const baker::Config&) { changed_since(); },
        .log =
            [](const baker::Error& e) {
                godot::UtilityFunctions::push_warning("Baker: ", to_godot(baker::to_string(e)));
            },
    };
    if (skip_local_) (void)options.open.skip.add("local");
    options.open.defines = defines;
    if (auto r = live_->open(options); !r) {
        live_.reset();
        context_.reset();
        return fail(r.error());
    }
    return godot::OK;
}

void BakerRuntime::_process(double) {
    if (!tool_args_.empty() && ++tool_frames_ == 2) run_tool_command();
    if constexpr (!baker::kShipping) {
        if (!live_) return;
        if (capturing_ && session_dirty_) {
            session_dirty_ = false;
            godot::EngineDebugger::get_singleton()->send_message("baker:session",
                                                                 Array::make(session()));
        }
    }
}

void BakerRuntime::begin_frame() {
    if (!live_ || frame_begun_ || locked_) return;
    frame_begun_ = true;
    live_->begin_frame();  // reloads what was saved, assigning every binding; answers the console
}

// The scene tree's signals, before any node's step: physics_frame before each physics step
// (none, some frames), process_frame once, after them.
void BakerRuntime::physics_frame() {
    begin_frame();
}

void BakerRuntime::process_frame() {
    begin_frame();
    frame_begun_ = false;  // the next iteration's first signal begins the next
}

void BakerRuntime::read_arguments() {
    const godot::PackedStringArray args = godot::OS::get_singleton()->get_cmdline_user_args();
    for (int64_t i = 0; i < args.size(); ++i) {
        const std::string a = to_std(args[i]);
        const bool more = i + 1 < args.size();
        if (a == "--no-local" || a == "--baker") {
            skip_local_ = true;  // a run's results do not depend on whose machine it is on
            if (a == "--no-local") continue;
            for (++i; i < args.size(); ++i) {
                const std::string t = to_std(args[i]);
                if (t == "--baker-output" && i + 1 < args.size())
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

godot::PackedStringArray BakerRuntime::dev_only_files(const String& project_path) {
    godot::PackedStringArray out;
    auto project = baker::read_project(project_file(project_path), reader());
    if (!project) {
        godot::UtilityFunctions::push_error("Baker: ", to_godot(baker::to_string(project.error())));
        return out;
    }
    for (const baker::ProjectLayer& l : project->layers)
        if (l.dev_only) out.push_back(to_godot(godot_path(l.path.str())));
    return out;
}

int BakerRuntime::get_console_port() const {
    return live_ ? int(live_->console_port()) : 0;
}

// --baker COMMAND: Baker's tool commands (host/commands.hpp), run once the scene is ready
// and its bound properties' tweaks are registered, then quits with the command's status.
void BakerRuntime::run_tool_command() {
    tool_argv_ = {"baker"};
    for (const std::string& a : tool_args_) tool_argv_.push_back(a.c_str());
    auto line = baker::host::parse_command_line(int(tool_argv_.size()), tool_argv_.data());
    if (!line) {
        const std::string message(baker::to_string(line.error()));
        std::fprintf(stderr, "baker: %s\n", message.c_str());
        return finish_tool(baker::host::kCommandError);
    }
    if (is_shipping() && line->command != "sim" && line->command != "compare") {
        std::fputs("baker: a shipping build runs sim and compare only\n", stderr);
        return finish_tool(baker::host::kCommandError);
    }
    if (line->write) {
        std::fputs(
            "baker: a game never writes into the project: promote --baker-patch FILE gives "
            "the change for a person to apply\n",
            stderr);
        return finish_tool(baker::host::kCommandError);
    }
    if constexpr (!baker::kShipping) {
        // The bindings of scenes no one has loaded, so reference and check know every
        // tunable in the project (addons/baker/scan.gd), as if each scene were open.
        const String scan = "res://addons/baker/scan.gd";
        if (godot::ResourceLoader::get_singleton()->exists(scan)) {
            const godot::Ref<godot::Script> script =
                godot::ResourceLoader::get_singleton()->load(scan);
            if (script.is_valid()) script->call("bind_unloaded", this);
        }
    }
    tool_project_ = project_file(project_path_);
    line->project = tool_project_;
    tool_hooks_.reader = reader();
    tool_hooks_.list = [](std::string_view dir, baker::Callback<void(std::string_view)> file) {
        return list_files(dir, file);
    };
    tool_hooks_.output = [this](std::string_view text) { tool_output_.append(text); };
    tool_hooks_.bind = [this](baker::Context& ctx) {
        tool_context_ = &ctx;
        bind_functions(ctx);
    };
    tool_hooks_.unbind = [this](baker::Context&) { tool_context_ = nullptr; };
    // The game's own reads (get_value, explain, ...) see the command's data, as it loads.
    tool_hooks_.changed = [this](const baker::Config& cfg) {
        tool_config_ = &cfg;
        cache_.clear();
    };
    if (skip_local_) (void)tool_hooks_.open.skip.add("local");
    defines_ = feature_defines();
    tool_defines_.assign(defines_.begin(), defines_.end());
    tool_hooks_.open.defines = tool_defines_;
    // The commands open a Config of their own, which binds the tweak registry, the bound
    // properties' among them: close this one first.
    live_.reset();
    context_.reset();
    if (line->command == "sim") {
        // Driven from the game's frames (_physics_process): each run reloads the scene.
        sim_ = std::make_unique<baker::host::SimRun>(*line, tool_hooks_);
        godot::Engine::get_singleton()->set_physics_ticks_per_second(
            int(std::lround(1.0 / sim_->step_seconds())));
        set_physics_process_priority(1 << 30);  // after the game's nodes: their step is done
        set_physics_process(true);
        return;
    }
    int status =
        line->command == "csv" ? run_csv(*line) : baker::host::run_command(*line, tool_hooks_);
    tool_config_ = nullptr;  // gone with the command
    if (status < 0) {
        std::fprintf(stderr, "baker: no command '%s'\n%s", tool_argv_[1],
                     std::string(baker::host::command_help()).c_str());
        status = baker::host::kCommandError;
    }
    finish_tool(status);
}

namespace {

// A CSV field: quoted when it holds a comma, a quote or a line break, its quotes doubled.
std::string csv_field(const String& text) {
    std::string s = to_std(text);
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

}  // namespace

// csv: every tweak as a row of a designer's spreadsheet, from the reference's tweaks:
// path, type, value, default, min, max, step, one_of, layer, file, line, desc.
int BakerRuntime::run_csv(const baker::host::CommandLine& line) {
    baker::host::CommandLine reference = line;
    reference.command = "reference";
    reference.json = true;
    reference.output = {};
    const size_t start = tool_output_.size();
    const int status = baker::host::run_command(reference, tool_hooks_);
    const String json = to_godot(std::string_view(tool_output_).substr(start));
    tool_output_.resize(start);
    if (status != 0) return status;
    const Dictionary doc = godot::JSON::parse_string(json);
    const Array tweaks = Dictionary(doc.get("tweaks", Dictionary())).get("tweaks", Array());
    std::string out = "path,type,value,default,min,max,step,one_of,layer,file,line,desc\n";
    for (int64_t i = 0; i < tweaks.size(); ++i) {
        const Dictionary t = tweaks[i];
        const Variant range = t.get("range", Variant());
        const Array r = range.get_type() == Variant::ARRAY ? Array(range) : Array();
        const Variant source = t.get("source", Variant());
        const Dictionary src =
            source.get_type() == Variant::DICTIONARY ? Dictionary(source) : Dictionary();
        const double step = t.get("step", 0.0);
        const String fields[] = {
            t.get("path", ""),
            t.get("type", ""),
            t.get("value", ""),
            t.get("default", ""),
            r.size() == 2 ? String(Variant(r[0])) : String(),
            r.size() == 2 ? String(Variant(r[1])) : String(),
            step != 0 ? String::num(step) : String(),
            String(" ").join(godot::PackedStringArray(Array(t.get("one_of", Array())))),
            src.get("layer", ""),
            src.get("file", ""),
            src.has("line") ? String::num_int64(int64_t(src["line"])) : String(),
            t.get("desc", ""),
        };
        for (size_t f = 0; f < std::size(fields); ++f) {
            if (f) out += ',';
            out += csv_field(fields[f]);
        }
        out += '\n';
    }
    tool_output_ += out;
    return 0;
}

void BakerRuntime::finish_tool(int status) {
    std::fwrite(tool_output_.data(), 1, tool_output_.size(), stdout);
    std::fflush(stdout);
    std::fflush(stderr);
    if (!tool_out_.empty() && !write_output(tool_out_, tool_output_))
        status = baker::host::kCommandError;
    tool_args_.clear();
    get_tree()->quit(status);
}

// Under --baker sim, once a physics tick, after the game's nodes have taken their step.
void BakerRuntime::_physics_process(double) {
    if (!sim_) return;
    switch (sim_->wants()) {
        case baker::host::SimRun::Wants::Reset:
            // A fresh scene for each run; its script calls sim_ready() once it is set up.
            if (!sim_reloading_) {
                sim_reloading_ = true;
                get_tree()->reload_current_scene();
            }
            break;
        case baker::host::SimRun::Wants::Steps: sim_->step(); break;
        case baker::host::SimRun::Wants::Done: {
            const int status = sim_->status();
            sim_.reset();
            tool_config_ = nullptr;
            set_physics_process(false);
            finish_tool(status);
            break;
        }
    }
}

void BakerRuntime::sim_ready() {
    if (!sim_ || !sim_reloading_ || sim_->wants() != baker::host::SimRun::Wants::Reset) return;
    sim_reloading_ = false;
    sim_->ready();
}

void BakerRuntime::bind_function(const String& name, const godot::Callable& function) {
    const bool known = functions_.has(name);
    functions_[name] = function;
    if (!known && context_) bind_into(*context_, baker::Symbol(to_std(name)));
}

void BakerRuntime::bind_functions(baker::Context& ctx) {
    const Array names = functions_.keys();
    for (int64_t i = 0; i < names.size(); ++i)
        bind_into(ctx, baker::Symbol(to_std(String(names[i]))));
}

namespace {

// How many arguments `f` takes: at most its parameters, at least those without a default.
// The defaults are known for an object's method; a lambda must be given all of them.
// Baker's name for the type a GDScript parameter declares; "value" for any other, or none.
std::string_view baker_type(Variant::Type t) {
    switch (t) {
        case Variant::BOOL: return "bool";
        case Variant::INT: return "int";
        case Variant::FLOAT: return "float";
        case Variant::STRING: return "string";
        case Variant::STRING_NAME: return "symbol";
        default: return "value";
    }
}

// `f`'s parameters as Baker describes them: an object's method by its declared types, the
// ones with defaults optional; a lambda by its count alone, each any value.
std::vector<baker::ParamDescription> describe(const godot::Callable& f) {
    std::vector<baker::ParamDescription> params(
        size_t(std::max<int64_t>(0, f.get_argument_count())),
        baker::ParamDescription{.type = "value"});
    const godot::Object* object = f.is_custom() ? nullptr : f.get_object();
    if (!object) return params;
    const godot::StringName method = f.get_method();
    const Array methods = object->get_method_list();
    for (int64_t i = 0; i < methods.size(); ++i) {
        const Dictionary m = methods[i];
        if (godot::StringName(m.get("name", "")) != method) continue;
        const Array args = m.get("args", Array());
        for (size_t k = 0; k < params.size() && int64_t(k) < args.size(); ++k)
            params[k].type =
                baker_type(Variant::Type(int64_t(Dictionary(args[int64_t(k)]).get("type", 0))));
        const size_t defaults = size_t(Array(m.get("default_args", Array())).size());
        for (size_t k = params.size() - std::min(defaults, params.size()); k < params.size(); ++k)
            params[k].kind = "optional";
        break;
    }
    return params;
}

}  // namespace

// {name arg...}: the arguments evaluated, as Godot values, to the function bound under the
// name now (a reloaded scene binds its own). Declared with the function's parameters, so
// check counts a call's arguments and checks their types, and a call is held to them.
void BakerRuntime::bind_into(baker::Context& ctx, baker::Symbol name) {
    if (ctx.has_func(name)) return;
    const std::vector<baker::ParamDescription> params =
        describe(functions_.get(to_godot(name.str()), godot::Callable()));
    const baker::OverloadDescription overload{.params = params};
    auto body = [this, name](baker::Args& a) -> baker::Expected<baker::Node> {
        const String key = to_godot(name.str());
        const godot::Callable f = functions_.get(key, godot::Callable());
        if (!f.is_valid())
            return baker::Unexpected(a.error("its function is gone", baker::ErrorCode::NotFound));
        Array args;
        for (size_t i = 0; i < a.size(); ++i) {
            auto v = a.eval(i);
            if (!v) return v;
            args.push_back(to_variant(*v));
        }
        const Variant result = f.callv(args);
        if (result.get_type() == Variant::NIL) return baker::Node(int64_t(0));
        baker::ValueList values;
        if (!to_values(result, values) || values.nodes().size() != 1)
            return baker::Unexpected(a.error("returned a " +
                                             to_std(Variant::get_type_name(result.get_type())) +
                                             ", not one value"));
        return values.nodes()[0];
    };
    auto r = ctx.declare(name, std::span<const baker::OverloadDescription>(&overload, 1), body);
    if (!r) (void)fail(r.error());
}

baker::Context* BakerRuntime::script_context() noexcept {
    return tool_context_ ? tool_context_ : context_.get();
}

Variant BakerRuntime::call_function(const String& name, const Array& args) {
    baker::Context* ctx = script_context();
    if (!ctx) {
        last_error_ = "Baker: no project is open";
        godot::UtilityFunctions::push_error(last_error_);
        return Variant();
    }
    baker::ValueList command;
    command.push(baker::Node(baker::Symbol(to_std(name))));
    for (int64_t i = 0; i < args.size(); ++i) {
        baker::ValueList value;
        if (!to_values(args[i], value) || value.nodes().empty()) {
            last_error_ = "Baker: argument " + String::num_int64(i + 1) + " of " + name + ", a " +
                          Variant::get_type_name(args[i].get_type()) + ", is not a value";
            godot::UtilityFunctions::push_error(last_error_);
            return Variant();
        }
        if (value.nodes().size() == 1)
            command.push(value.nodes()[0]);
        else
            command.push(baker::Node(baker::Array::make_copy(value.nodes())));
    }
    return evaluated(
        ctx->eval(baker::Node(baker::Array::make_copy(command.nodes()), baker::Type::Command)));
}

Variant BakerRuntime::eval(const String& text) {
    baker::Context* ctx = script_context();
    if (!ctx) {
        last_error_ = "Baker: no project is open";
        godot::UtilityFunctions::push_error(last_error_);
        return Variant();
    }
    return evaluated(ctx->eval(to_std(text), "<game>"));
}

Variant BakerRuntime::evaluated(const baker::Expected<baker::Node>& result) {
    if (!result) {
        (void)fail(result.error());
        return Variant();
    }
    const Variant v = to_variant(*result);
    if (!tool_context_) changed_since();  // a command may have changed the data
    return v;
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
    if (hold([this] { (void)reload(); })) return false;
    const bool reloaded = live_->reload_now();
    changed_since();
    return reloaded;
}

bool BakerRuntime::hold(std::function<void()> change) {
    if (!locked_) return false;
    held_.push_back(std::move(change));
    session_dirty_ = true;
    return true;
}

String BakerRuntime::fingerprint() const {
    if (!config()) return String();
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(config()->digest()));
    return String(hex);
}

String BakerRuntime::lock() {
    locked_ = true;
    session_dirty_ = true;
    return fingerprint();
}

String BakerRuntime::apply_held() {
    std::vector<std::function<void()>> held;
    held.swap(held_);
    const bool locked = locked_;
    locked_ = false;
    for (const std::function<void()>& change : held) change();
    if constexpr (!baker::kShipping) {
        // Every file saved before the boundary, whether or not the watcher has reported it
        // yet: a boundary that depended on when the watcher noticed would not be the same
        // for every peer. Then the console, and #autorun after the reload.
        if (live_) {
            (void)live_->reload_now();
            live_->begin_frame();
        }
    }
    locked_ = locked;
    session_dirty_ = true;
    return fingerprint();
}

void BakerRuntime::unlock() {
    (void)apply_held();
    locked_ = false;
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

namespace {

// A layer's options from GDScript: {optional, enabled, below, above}. `names` holds the
// strings the options' views point at, for the push.
baker::LayerOptions layer_options(const Dictionary& d, std::string (&names)[2]) {
    baker::LayerOptions o;
    o.optional = bool(d.get("optional", false));
    o.enabled = bool(d.get("enabled", true));
    names[0] = to_std(String(d.get("below", "")));
    names[1] = to_std(String(d.get("above", "")));
    o.below = names[0];
    o.above = names[1];
    return o;
}

}  // namespace

godot::Error BakerRuntime::push_layer(const String& name, const String& path,
                                      const Dictionary& options) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (hold([=, this] { (void)push_layer(name, path, options); })) return godot::OK;
    std::string names[2];
    if (auto r = config()->push_layer(to_std(name), to_std(path), layer_options(options, names));
        !r)
        return fail(r.error());
    changed();
    return godot::OK;
}

godot::Error BakerRuntime::push_layer_text(const String& name, const String& text,
                                           const Dictionary& options) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (hold([=, this] { (void)push_layer_text(name, text, options); })) return godot::OK;
    std::string names[2];
    if (auto r =
            config()->push_layer_text(to_std(name), to_std(text), layer_options(options, names));
        !r)
        return fail(r.error());
    changed();
    return godot::OK;
}

std::vector<std::string> BakerRuntime::feature_defines() {
    static constexpr const char* kGodot[] = {
        "debug", "release", "editor", "template", "pc",  "mobile",           "web",
        "linux", "windows", "macos",  "android",  "ios", "dedicated_server", "x86_64",
        "arm64"};
    godot::OS* os = godot::OS::get_singleton();
    std::vector<std::string> out;
    for (const char* f : kGodot)
        if (os->has_feature(f)) out.push_back(f);
    const godot::PackedStringArray custom = godot::ProjectSettings::get_singleton()->get_setting(
        "baker/feature_defines", godot::PackedStringArray());
    for (const String& f : custom) {
        const std::string name = to_std(f);
        if (os->has_feature(f) && std::find(out.begin(), out.end(), name) == out.end())
            out.push_back(name);
    }
    return out;
}

godot::Error BakerRuntime::push_layer_dir(const String& name, const String& dir,
                                          const Dictionary& options) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (hold([=, this] { (void)push_layer_dir(name, dir, options); })) return godot::OK;
    std::string names[2];
    if (auto r = config()->push_layer_dir(to_std(name), to_std(dir), layer_options(options, names));
        !r)
        return fail(r.error());
    changed();
    return godot::OK;
}

String BakerRuntime::layer_text(const String& name) const {
    if (!config()) return {};
    std::string text;
    if (auto r = config()->layer_text(to_std(name), baker::string_out(text)); !r) {
        godot::UtilityFunctions::push_error("Baker: ", to_godot(baker::to_string(r.error())));
        return {};
    }
    return to_godot(text);
}

godot::Error BakerRuntime::set_layer_text(const String& name, const String& text) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (hold([=, this] { (void)set_layer_text(name, text); })) return godot::OK;
    if (auto r = config()->set_layer_text(to_std(name), to_std(text)); !r) return fail(r.error());
    changed_since();
    return godot::OK;
}

godot::Error BakerRuntime::set_defines(const godot::PackedStringArray& defines) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (hold([=, this] { (void)set_defines(defines); })) return godot::OK;
    std::vector<std::string> names;
    for (const String& d : defines) names.push_back(to_std(d));
    const std::vector<std::string_view> views(names.begin(), names.end());
    if (auto r = config()->set_defines(views); !r) return fail(r.error());
    defines_ = std::move(names);
    changed_since();
    return godot::OK;
}

godot::PackedStringArray BakerRuntime::get_defines() const {
    godot::PackedStringArray out;
    for (const std::string& d : defines_) out.push_back(to_godot(d));
    return out;
}

godot::Error BakerRuntime::remove_layer(const String& name) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (hold([=, this] { (void)remove_layer(name); })) return godot::OK;
    if (auto r = config()->remove_layer(to_std(name)); !r) return fail(r.error());
    changed();
    return godot::OK;
}

BakerBinding* BakerRuntime::bind_resource(const godot::Ref<godot::Resource>& resource,
                                          const Dictionary& bindings) {
    if (resource.is_null()) return nullptr;
    unbind_resource(resource);
    auto* binding = memnew(BakerBinding);
    binding->set_name("Resource" + String::num_int64(int64_t(resource->get_instance_id())));
    binding->set_target(resource.ptr());
    binding->set_bindings(bindings);
    add_child(binding);  // binds as it enters, as a node's binding does
    return binding;
}

void BakerRuntime::unbind_resource(const godot::Ref<godot::Resource>& resource) {
    if (resource.is_null()) return;
    const String name = "Resource" + String::num_int64(int64_t(resource->get_instance_id()));
    if (godot::Node* old = get_node_or_null(godot::NodePath(name))) {
        remove_child(old);
        old->queue_free();
    }
}

namespace {

// The properties under a script's @export_group `group`, which a record's binding binds.
godot::PackedStringArray grouped(const godot::Object& o, const String& group) {
    godot::PackedStringArray out;
    bool in = false;
    for (const Variant& p : o.get_property_list()) {
        const Dictionary info = p;
        const int64_t usage = info.get("usage", 0);
        if (usage & godot::PROPERTY_USAGE_CATEGORY) {
            in = false;
        } else if (usage & godot::PROPERTY_USAGE_GROUP) {
            in = String(info["name"]) == group;
        } else if (in && !(usage & godot::PROPERTY_USAGE_SUBGROUP) &&
                   (usage & godot::PROPERTY_USAGE_STORAGE) &&
                   (usage & godot::PROPERTY_USAGE_EDITOR)) {
            out.push_back(info["name"]);
        }
    }
    return out;
}

}  // namespace

int64_t BakerRuntime::bind_records(const String& prefix, const String& dir,
                                   const Dictionary& options) {
    unbind_records(prefix);
    if (!godot::DirAccess::dir_exists_absolute(dir)) {
        last_error_ = "Baker: no directory " + dir;
        godot::UtilityFunctions::push_error(last_error_);
        return 0;
    }
    const String key_field = options.get("key", "");
    const String group = options.get("group", "Tuning");
    // An export lists a resource it converted as NAME.remap; the loader follows it.
    godot::PackedStringArray files = godot::DirAccess::get_files_at(dir);
    files.sort();
    Dictionary table;
    for (String file : files) {
        if (file.ends_with(".remap")) file = file.trim_suffix(".remap");
        if (!file.ends_with(".tres") && !file.ends_with(".res")) continue;
        const godot::Ref<godot::Resource> r =
            godot::ResourceLoader::get_singleton()->load(dir.path_join(file));
        if (r.is_null()) continue;
        const String key = key_field.is_empty() ? file.get_basename() : String(r->get(key_field));
        if (key.is_empty()) {
            godot::UtilityFunctions::push_warning("Baker: ", dir.path_join(file), " has no ",
                                                  key_field, " to key it by");
            continue;
        }
        table[key] = r;
        const godot::Ref<godot::Script> script = r->get_script();
        if (script.is_valid() && !script->get_global_name().is_empty())
            record_tables_[String(script->get_global_name())] = prefix;
    }
    // Every record is known before any binds, so a reference to one of its own table binds.
    records_[prefix] = table;
    const Array keys = table.keys();
    for (int64_t i = 0; i < keys.size(); ++i) {
        const godot::Ref<godot::Resource> r = table[keys[i]];
        Dictionary bindings;
        for (const String& property : grouped(*r.ptr(), group))
            bindings[property] = prefix + String(".") + String(keys[i]) + String(".") + property;
        (void)bind_resource(r, bindings);
    }
    return keys.size();
}

void BakerRuntime::unbind_records(const String& prefix) {
    if (!records_.has(prefix)) return;
    const Dictionary table = records_[prefix];
    records_.erase(prefix);
    const Array values = table.values();
    for (int64_t i = 0; i < values.size(); ++i) unbind_resource(values[i]);
    const Array classes = record_tables_.keys();
    for (int64_t i = 0; i < classes.size(); ++i)
        if (String(record_tables_[classes[i]]) == prefix) record_tables_.erase(classes[i]);
}

godot::Ref<godot::Resource> BakerRuntime::record(const String& prefix, const String& key) const {
    if (!records_.has(prefix)) return {};
    return Dictionary(records_[prefix]).get(key, Variant());
}

Dictionary BakerRuntime::records(const String& prefix) const {
    return records_.has(prefix) ? Dictionary(records_[prefix]).duplicate() : Dictionary();
}

String BakerRuntime::record_table(const String& class_name) const {
    return record_tables_.get(class_name, String());
}

String BakerRuntime::record_key(const String& prefix, const godot::Object* resource) const {
    if (!resource || !records_.has(prefix)) return {};
    const Dictionary table = records_[prefix];
    const Array keys = table.keys();
    for (int64_t i = 0; i < keys.size(); ++i)
        if (const godot::Object* o = table[keys[i]]; o && o == resource) return keys[i];
    return {};
}

godot::Error BakerRuntime::set_layer_enabled(const String& layer, bool enabled) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (hold([=, this] { (void)set_layer_enabled(layer, enabled); })) return godot::OK;
    const std::string name = to_std(layer);
    if (config()->layer_enabled(name) == enabled) return godot::OK;
    if (auto r = config()->set_layer_enabled(name, enabled); !r) return fail(r.error());
    changed();
    return godot::OK;
}

bool BakerRuntime::toggle_layer(const String& layer) {
    if (!config()) return false;
    if (hold([=, this] { (void)toggle_layer(layer); }))
        return !config()->layer_enabled(to_std(layer));
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
    const Variant v = r->nodes.empty() ? Variant() : to_variant(r->values(), fallback.get_type());
    return v.get_type() == Variant::NIL ? fallback : v;
}

Variant BakerRuntime::tweak(const String& path, const Variant& fallback) {
    if (cache_.has(path)) return cache_[path];
    const Variant v = get_value(path, fallback);
    cache_[path] = v;
    return v;
}

namespace {

// The symbol a node's array is tagged by, if it is a tagged array.
const baker::Array* tagged(const baker::Node& n) {
    const baker::Array* a = n.array();
    return a && a->size() > 0 && (*a)[0].try_sym() ? a : nullptr;
}

// Each value under `table`'s tagged children: a child whose values are all tagged arrays,
// each tag once, is a table of its own; any other is a value at its path.
void walk(std::span<const baker::Node> nodes, const String& prefix, Array& out) {
    for (const baker::Node& n : nodes) {
        const baker::Array* a = tagged(n);
        if (!a) continue;
        const String path = (prefix.is_empty() ? String() : prefix + String(".")) +
                            to_godot((*a)[0].try_sym()->str());
        const baker::Values values = a->values();
        bool table = !values.empty();
        std::vector<baker::Symbol> tags;
        for (const baker::Node& v : values) {
            const baker::Array* t = tagged(v);
            if (!t) {
                table = false;
                break;
            }
            const baker::Symbol tag = *(*t)[0].try_sym();
            if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
                table = false;  // a list of records, (wave ...) (wave ...): one value
                break;
            }
            tags.push_back(tag);
        }
        if (table) {
            walk(std::span<const baker::Node>(values.begin(), values.end()), path, out);
        } else {
            Dictionary d;
            d["path"] = path;
            d["value"] = to_table(values);
            out.push_back(d);
        }
    }
}

}  // namespace

Array BakerRuntime::data_paths() const {
    Array out;
    if (!config()) return out;
    walk(config()->root()->nodes(), String(), out);
    return out;
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
    auto entries = baker::tools::explain(*config(), baker::Path(to_std(path)));
    if (!entries) return out;
    for (const baker::tools::ExplainEntry& e : *entries) {
        Dictionary d;
        put_source(d, e.source);
        d["layer"] = name_of(e.layer);
        d["value"] = to_variant(e.value);
        d["standing"] = standing_name(e.standing);
        // The code's value of a record's property is its resource's.
        if (e.layer.str() == "code")
            if (const String resource = BakerBinding::resource_of(path); !resource.is_empty())
                d["resource"] = resource;
        out.push_back(d);
    }
    return out;
}

godot::Error BakerRuntime::set_value(const String& path, const Variant& value) {
    if (!config()) return godot::ERR_UNCONFIGURED;
    if (hold([=, this] { (void)set_value(path, value); })) return godot::OK;
    if (value.get_type() == Variant::DICTIONARY) {
        // Each field it names, as its own live edit: the record's other fields stay.
        const Dictionary fields = value;
        const Array keys = fields.keys();
        for (int64_t i = 0; i < keys.size(); ++i)
            if (const godot::Error e =
                    set_value(path + String(".") + String(keys[i]), fields[keys[i]]);
                e != godot::OK)
                return e;
        return keys.is_empty() ? godot::ERR_INVALID_PARAMETER : godot::OK;
    }
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
    if (hold([=, this] { (void)clear_scratch(path); })) return godot::OK;
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
    auto entries = baker::tools::layer_effect(*config(), to_std(layer));
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
