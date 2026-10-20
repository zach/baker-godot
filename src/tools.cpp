#include "tools.hpp"

#include "godot_files.hpp"

#include <baker/shape.hpp>
#include <baker/std_interop.hpp>
#include <baker/std_script.hpp>

#include <algorithm>
#include <cstdio>
#include <map>

namespace baker_godot {

namespace {

constexpr int kFailed = 1;
constexpr int kError = 2;

ToolResult usage(const std::string& message) {
    return {kError,
            {},
            message +
                "\nusage: --baker check|reference [--json] | eval [--json] \"{...}\"... | diff | "
                "promote [--to LAYER] --patch FILE\n"};
}

bool has(const std::vector<std::string>& args, const char* flag) {
    return std::find(args.begin() + 1, args.end(), flag) != args.end();
}

std::string value_of(const std::vector<std::string>& args, const char* flag, std::string fallback) {
    for (size_t i = 1; i + 1 < args.size(); ++i)
        if (args[i] == flag) return args[i + 1];
    return fallback;
}

ToolResult check(const std::vector<std::string>& args, baker::Config& cfg, baker::Context& ctx) {
    baker::CheckReport report;
    baker::check(cfg, ctx, report, {.unresolved_fails = true});
    ToolResult r;
    if (has(args, "--json"))
        baker::write_json(report, baker::string_out(r.out), &cfg.read_session());
    else
        baker::write(report, baker::string_out(r.out));
    r.status = report.ok() ? 0 : kFailed;
    return r;
}

ToolResult reference(const std::vector<std::string>& args, baker::Config& cfg,
                     baker::Context& ctx) {
    ToolResult r;
    if (!has(args, "--json")) {
        baker::reference(ctx, baker::string_out(r.out), &cfg.read_session());
        return r;
    }
    // The vocabulary, the tweaks (the bound properties among them), and the shapes the
    // enabled layers declare, in one object.
    std::string ref, tweaks, shapes;
    baker::write_reference_json(ctx, baker::string_out(ref), &cfg.read_session());
    baker::write_json(baker::TweakRegistry::global(), baker::string_out(tweaks));
    baker::shape::Set set;
    baker::shape::Report report;
    for (const baker::Symbol layer : cfg.layers())
        if (cfg.layer_enabled(layer.str()))
            if (const baker::Ref<baker::Array> root = cfg.layer_root(layer.str()))
                set.add(*root, report, &cfg.read_session());
    baker::shape::write_json(set, baker::string_out(shapes));
    r.out =
        "{\"reference\": " + ref + ", \"tweaks\": " + tweaks + ", \"shapes\": " + shapes + "}\n";
    return r;
}

ToolResult eval(const std::vector<std::string>& args, baker::Context& ctx) {
    ToolResult r;
    const bool json = has(args, "--json");
    int id = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& command = args[i];
        if (command == "--json") continue;
        if (command == "--out") {
            ++i;
            continue;
        }
        ++id;
        if (json) {
            // One baker.reply/1.1 line a command, as the running game's console answers.
            std::string reply;
            baker::eval_request(ctx, "(" + std::to_string(id) + " " + command + ")",
                                baker::string_out(reply));
            if (reply.find(R"(,"ok":false,)") != std::string::npos) r.status = kFailed;
            r.out += reply;
            continue;
        }
        auto v = ctx.eval(command, "<eval>");
        if (!v) {
            r.err += std::string(baker::describe(v.error())) + "\n";
            r.status = kFailed;
        } else if (auto text = v->as_str()) {
            r.out += std::string(*text) + "\n";
        } else {
            r.out += baker::to_string(*v) + "\n";
        }
    }
    if (id == 0) return usage("eval needs a command: eval \"{...}\"");
    return r;
}

ToolResult diff(baker::Config& cfg) {
    ToolResult r;
    auto effect = cfg.layer_effect("agent");
    if (!effect) return {kError, {}, std::string(baker::to_string(effect.error())) + "\n"};
    if (effect->empty()) {
        r.out = "agent.dta changes nothing\n";
        return r;
    }
    r.out = "agent.dta changes:\n";
    for (const baker::LayerEffectEntry& e : *effect) {
        const std::string below = e.below ? baker::to_string(*e.below) : std::string("(new)");
        r.out += "  " + std::string(e.path.str()) + "  " + below + " -> " +
                 baker::to_string(e.value) + "  (" + std::string(e.source.file.str()) + ":" +
                 std::to_string(e.source.line) + ")" + (e.shadowed ? "  shadowed" : "") + "\n";
    }
    return r;
}

ToolResult promote(const std::vector<std::string>& args, baker::Config& cfg) {
    const std::string patch = value_of(args, "--patch", "");
    const std::string to = value_of(args, "--to", "default");
    if (patch.empty()) return usage("promote needs --patch FILE: it never writes the data");
    // The real promotion, with the writer capturing each file instead of writing it.
    std::map<std::string, std::string> captured;
    cfg.set_writer([&captured](std::string_view path, std::string_view text) {
        captured[std::string(path)] = std::string(text);
        return true;
    });
    auto report = cfg.promote("agent", to);
    if (!report) return {kError, {}, std::string(baker::describe(report.error())) + "\n"};
    ToolResult r;
    for (const baker::Error& e : report->refused)
        r.err += "refused: " + std::string(baker::to_string(e)) + "\n";
    std::string text;
    if (report->ok()) {  // all or nothing
        const baker::LoadSession::Reader read = reader();
        for (const auto& [path, after] : captured) {
            std::string before;
            (void)read(path, baker::string_out(before));
            text += unified_diff(before, after, path);
        }
    }
    if (!write_output(patch, text)) return {kError, {}, "cannot write " + patch + "\n"};
    r.out = "wrote " + patch + ": " + std::to_string(report->ok() ? captured.size() : 0) +
            " file(s) would change; apply it from the project's root with git apply -p1\n";
    r.status = report->ok() ? 0 : kFailed;
    return r;
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        const size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start + 1));
        start = end + 1;
    }
    return lines;
}

}  // namespace

bool write_output(const std::string& path, const std::string& text) {
    if (path.find("://") != std::string::npos) return write_file(path, text, true);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    return std::fclose(f) == 0 && ok;
}

ToolResult run_tool(const std::vector<std::string>& args, baker::Config& cfg, baker::Context& ctx) {
    if (args.empty()) return usage("--baker needs a command");
    const std::string& command = args[0];
    if (command == "check") return check(args, cfg, ctx);
    if (command == "reference") return reference(args, cfg, ctx);
    if (command == "eval") return eval(args, ctx);
    if (command == "diff") return diff(cfg);
    if (command == "promote") return promote(args, cfg);
    return usage("unknown command " + command);
}

std::string unified_diff(const std::string& before, const std::string& after,
                         const std::string& name) {
    const std::vector<std::string> a = lines_of(before), b = lines_of(after);
    if (a == b) return {};
    // The longest common subsequence of lines, then the edit script it gives.
    const size_t n = a.size(), m = b.size();
    std::vector<std::vector<uint32_t>> lcs(n + 1, std::vector<uint32_t>(m + 1, 0));
    for (size_t i = n; i-- > 0;)
        for (size_t j = m; j-- > 0;)
            lcs[i][j] =
                a[i] == b[j] ? lcs[i + 1][j + 1] + 1 : std::max(lcs[i + 1][j], lcs[i][j + 1]);
    struct Op {
        char kind;      // ' ', '-', '+'
        size_t ai, bi;  // the line's index in a and in b
    };
    std::vector<Op> ops;
    for (size_t i = 0, j = 0; i < n || j < m;) {
        if (i < n && j < m && a[i] == b[j])
            ops.push_back({' ', i++, j++});
        else if (i < n && (j == m || lcs[i + 1][j] >= lcs[i][j + 1]))
            ops.push_back({'-', i++, j});
        else
            ops.push_back({'+', i, j++});
    }
    std::string out = "--- a/" + name + "\n+++ b/" + name + "\n";
    constexpr size_t kContext = 3;
    std::vector<size_t> changes;
    for (size_t k = 0; k < ops.size(); ++k)
        if (ops[k].kind != ' ') changes.push_back(k);
    for (size_t c = 0; c < changes.size();) {
        size_t last = c;
        while (last + 1 < changes.size() && changes[last + 1] - changes[last] <= 2 * kContext + 1)
            ++last;
        const size_t start = changes[c] > kContext ? changes[c] - kContext : 0;
        const size_t end = std::min(ops.size(), changes[last] + kContext + 1);
        size_t a_count = 0, b_count = 0;
        for (size_t x = start; x < end; ++x) {
            a_count += ops[x].kind != '+';
            b_count += ops[x].kind != '-';
        }
        // A side with no lines in the hunk names the line before it, as diff -u does.
        out += "@@ -" + std::to_string(ops[start].ai + (a_count ? 1 : 0)) + "," +
               std::to_string(a_count) + " +" + std::to_string(ops[start].bi + (b_count ? 1 : 0)) +
               "," + std::to_string(b_count) + " @@\n";
        for (size_t x = start; x < end; ++x) {
            const std::string& line = ops[x].kind == '+' ? b[ops[x].bi] : a[ops[x].ai];
            out += ops[x].kind + line;
            if (line.empty() || line.back() != '\n') out += "\n\\ No newline at end of file\n";
        }
        c = last + 1;
    }
    return out;
}

}  // namespace baker_godot
