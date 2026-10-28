/*
 * ct_project_cli.h
 * Local project subcommands. No change to CherryTree's existing CLI options.
 */
#pragma once

#include <climits>
#include <cctype>
#include <ostream>
#include <string>
#include <vector>

namespace CtProjectCli {

struct Result {
    bool recognized{false};
    bool stop{false};
    int status{0};
    std::vector<std::string> argv;
};

inline std::string usage()
{
    return "Usage:\n"
           "  ct [FILE.ctb] project create NAME\n"
           "  ct [FILE.ctb] project list [-N | --years N | -a | --all]\n"
           "  ct [FILE.ctb] project rename PNR NAME [--all-occurrences]\n"
           "  ct [FILE.ctb] project delete PNR [-r|--recursive] [-y|--yes]\n"
           "  ct project help [create|list|rename|delete]\n";
}

inline std::string help(const std::string& action = "")
{
    if (action == "create") return usage() + "\ncreate: Create a project from the current placeholder.\n";
    if (action == "list") return usage() + "\nlist: Show the latest project year (default), last N years (-N or --years N), or all (--all).\n"
                                           "-N counts calendar years including the newest project year. -a is an alias for --all.\n";
    if (action == "rename") return usage() + "\nrename: Rename a project selected by its complete project number (PNR).\n"
        "Default: update only project links. --all-occurrences also replaces visible plain text and other link captions.\n";
    if (action == "delete") return usage() + "\ndelete: Delete a project selected by its complete PNR.\n"
                                         "Interactive confirmation is required unless -y / --yes is given.\n"
                                         "-r / --recursive also deletes child nodes; without it, child nodes prevent deletion.\n"
                                         "Shared nodes are not supported; other external links may become invalid.\n";
    return usage() + "\n"
        "The file can be omitted only when exactly one CherryTree document is open.\n"
        "Full command names are stable; unique prefixes are accepted interactively.\n"
        "The current local implementation supports unencrypted .ctb documents.\n";
}

inline std::string resolve(const std::string& token, const std::vector<std::string>& names)
{
    for (const auto& name : names) if (name == token) return name;
    std::string result;
    for (const auto& name : names) {
        if (name.compare(0, token.size(), token) == 0) {
            if (not result.empty()) return ""; // ambiguous
            result = name;
        }
    }
    return result;
}

inline bool parse_years(const std::string& token, int& years)
{
    if (token.empty()) return false;
    long long value = 0;
    for (char ch : token) {
        if (ch < '0' or ch > '9') return false;
        value = value * 10 + ch - '0';
        if (value > 100) return false;
    }
    if (value < 1) return false;
    years = static_cast<int>(value);
    return true;
}

inline Result parse(int argc, char* argv[], std::ostream& out, std::ostream& err)
{
    Result result;
    int pos = 1;
    std::vector<std::string> prefix{argc > 0 ? argv[0] : "cherrytree"};
    if (pos < argc and (std::string{argv[pos]} == "-S" or
                        std::string{argv[pos]} == "--secondary_session")) {
        prefix.push_back(argv[pos++]);
    }
    std::string file;
    if (pos < argc) {
        const std::string first = argv[pos];
        if (first.size() >= 4 and first.substr(first.size() - 4) == ".ctb") {
            file = first;
            ++pos;
        }
    }
    // Support the CS-style reversed help forms without intercepting CherryTree's
    // ordinary global -h/--help when no project scope follows.
    if (pos + 1 < argc and (std::string{argv[pos]} == "help" or
        std::string{argv[pos]} == "-h" or std::string{argv[pos]} == "--help") and
        resolve(argv[pos+1], {"project"}) == "project") {
        result.recognized = true;
        result.stop = true;
        pos += 2;
        std::string selected;
        bool has_topic = pos < argc;
        if (has_topic) selected = resolve(argv[pos++], {"create", "list", "rename", "delete"});
        if (pos != argc or (has_topic and selected.empty())) {
            err << "ct: unknown project help topic\n" << usage();
            result.status = 2;
            return result;
        }
        out << help(selected);
        return result;
    }
    if (pos >= argc) return result;
    std::string scope = resolve(argv[pos], {"project"});
    if (scope != "project") return result;
    result.recognized = true;
    ++pos;

    auto error = [&](const std::string& message) {
        err << "ct: " << message << "\n" << usage() << "Try 'ct project --help' for details.\n";
        result.stop = true;
        result.status = 2;
    };
    if (pos >= argc) { error("missing project action"); return result; }

    std::string action = resolve(argv[pos], {"create", "list", "rename", "delete", "help"});
    if (std::string{argv[pos]} == "--help" or std::string{argv[pos]} == "-h") action = "help";
    if (action.empty()) { error(std::string{"unknown or ambiguous action: "} + argv[pos]); return result; }
    ++pos;
    if (action == "help") {
        std::string selected;
        if (pos < argc) {
            selected = resolve(argv[pos++], {"create", "list", "rename", "delete"});
            if (selected.empty()) { error("unknown help topic"); return result; }
        }
        if (pos != argc) { error("too many help arguments"); return result; }
        out << help(selected);
        result.stop = true;
        return result;
    }
    if (pos < argc and (std::string{argv[pos]} == "--help" or std::string{argv[pos]} == "-h")) {
        if (pos + 1 != argc) { error("unexpected arguments after --help"); return result; }
        out << help(action);
        result.stop = true;
        return result;
    }

    std::string name, pnr;
    int years{1};
    bool all{false};
    bool yes{false}, recursive{false};
    bool all_occurrences{false};
    if (action == "create") {
        if (pos + 1 != argc) { error("create requires exactly one project name"); return result; }
        name = argv[pos];
    }
    else if (action == "rename") {
        if (pos + 2 > argc or pos + 3 < argc) {
            error("rename requires PNR, name and optional --all-occurrences"); return result;
        }
        pnr = argv[pos++];
        name = argv[pos++];
        if (pos < argc) {
            if (std::string{argv[pos]} != "--all-occurrences") {
                error(std::string{"unknown rename option: "} + argv[pos]); return result;
            }
            all_occurrences = true;
        }
    }
    else if (action == "delete") {
        if (pos >= argc) { error("delete requires a project number (PNR)"); return result; }
        pnr = argv[pos++];
        for (; pos < argc; ++pos) {
            const std::string arg = argv[pos];
            if (arg == "--yes" or arg == "-y") {
                if (yes) { error("--yes given more than once"); return result; }
                yes = true;
            }
            else if (arg == "--recursive" or arg == "-r") {
                if (recursive) { error("--recursive given more than once"); return result; }
                recursive = true;
            }
            else { error("unknown delete option: " + arg); return result; }
        }
    }
    else if (action == "list") {
        if (pos < argc) {
            std::string arg = argv[pos++];
            if (arg == "--all" or arg == "-a") all = true;
            else if (arg == "--years") {
                if (pos >= argc or not parse_years(argv[pos++], years)) {
                    error("--years requires a positive number"); return result;
                }
            }
            else if (arg.compare(0, 8, "--years=") == 0) {
                if (not parse_years(arg.substr(8), years)) {
                    error("invalid value for --years"); return result;
                }
            }
            else if (arg.size() > 1 and arg[0] == '-') {
                if (not parse_years(arg.substr(1), years)) {
                    error("invalid -N year count"); return result;
                }
            }
            else { error("unknown list argument: " + arg); return result; }
        }
        if (pos != argc) { error("list takes only one filter (-N or --all)"); return result; }
    }
    if (name.empty() and action != "list" and action != "delete") {
        error("project name must not be empty"); return result;
    }

    result.argv = prefix;
    result.argv.push_back("--local-project-action=" + action);
    if (yes) result.argv.push_back("--local-project-yes");
    if (recursive) result.argv.push_back("--local-project-recursive");
    if (all_occurrences) result.argv.push_back("--local-project-all-occurrences");
    if (not name.empty()) result.argv.push_back("--local-project-name=" + name);
    if (not pnr.empty()) result.argv.push_back("--local-project-pnr=" + pnr);
    if (action == "list") {
        result.argv.push_back("--local-project-years=" + std::to_string(years));
        if (all) result.argv.push_back("--local-project-all");
    }
    if (not file.empty()) result.argv.push_back(file);
    return result;
}

} // namespace CtProjectCli
