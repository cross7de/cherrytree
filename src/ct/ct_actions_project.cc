/*
 * ct_actions_project.cc
 * Personal CherryTree project operations. Not part of the upstream PR.
 */
#include "ct_actions.h"
#include "ct_treestore.h"
#include "ct_storage_control.h"

#include <regex>
#include <sstream>
#include <iomanip>
#include <vector>
#include <functional>
#include <algorithm>

namespace {

struct Project {
    CtTreeIter node;
    std::string pnr;
    std::string title;
    int year{0};
    int year_width{0};
};

bool split_project_name(const std::string& input, Project& p)
{
    static const std::regex pattern{R"(^([0-9]{1,2})P([0-9]{2}) - (.+)$)"};
    std::smatch m;
    if (not std::regex_match(input, m, pattern)) return false;
    p.pnr = m[1].str() + "P" + m[2].str();
    p.title = m[3].str();
    p.year = std::stoi(m[1].str());
    p.year_width = static_cast<int>(m[1].str().size());
    return true;
}

bool valid_title(const Glib::ustring& title)
{
    if (title.empty() or title == "PLATZHALTER" or
        title.find('\n') != Glib::ustring::npos or
        title.find('\r') != Glib::ustring::npos) return false;
    for (const auto ch : title) if (not g_unichar_isspace(ch)) return true;
    return false;
}

CtTreeIter find_overview(CtTreeStore& store, Glib::ustring& error)
{
    CtTreeIter found;
    int matches{0};
    std::function<void(CtTreeIter)> scan = [&](CtTreeIter node) {
        while (node) {
            if (node.get_node_name() == "Projekt") { found = node; ++matches; }
            scan(node.first_child());
            ++node;
        }
    };
    scan(store.get_ct_iter_first());
    if (matches != 1) {
        error = "Expected exactly one overview node named 'Projekt'";
        return CtTreeIter{};
    }
    return found;
}

// The overview's linked captions form the project register, regardless of
// the target nodes' folders or any status characters preceding the links.
std::vector<Project> collect_projects(CtTreeStore& store, CtTreeIter overview,
                                      Glib::ustring& error)
{
    std::vector<Project> result;
    const auto buffer = overview.get_node_text_buffer();
    const Glib::ustring text = buffer->get_slice(buffer->begin(), buffer->end());
    const std::string link_prefix = CtConst::TAG_LINK_PREFIX.raw() + std::string{"node "};
    bool below_template{false};

    auto node_link_at = [&](int offset, gint64& id, std::string& tag_name) -> bool {
        for (const auto& tag : buffer->get_iter_at_offset(offset).get_tags()) {
            const std::string name = tag->property_name().get_value().raw();
            if (name.compare(0, link_prefix.size(), link_prefix) != 0) continue;
            const std::string id_text = name.substr(link_prefix.size());
            try {
                std::size_t consumed{0};
                const auto parsed = std::stoll(id_text, &consumed, 10);
                if (parsed <= 0 or consumed != id_text.size()) continue;
                id = parsed;
                tag_name = name;
                return true;
            }
            catch (const std::exception&) {
                // Ignore links that do not directly target a numeric node ID.
            }
        }
        return false;
    };

    for (Glib::ustring::size_type line_start = 0; line_start < text.size();) {
        const auto newline = text.find('\n', line_start);
        const auto line_end = newline == Glib::ustring::npos ? text.size() : newline;
        for (int offset = static_cast<int>(line_start); offset < static_cast<int>(line_end); ++offset) {
            gint64 node_id{0};
            std::string tag_name;
            if (not node_link_at(offset, node_id, tag_name)) continue;

            int link_end = offset + 1;
            for (; link_end < static_cast<int>(line_end); ++link_end) {
                gint64 next_id{0};
                std::string next_tag;
                if (not node_link_at(link_end, next_id, next_tag) or next_tag != tag_name) break;
            }
            Project p;
            if (split_project_name(text.substr(offset, link_end - offset).raw(), p)) {
                CtTreeIter target = store.get_node_from_node_id(node_id);
                if (not below_template) {
                    if (p.title == "PLATZHALTER" and target and
                        target.get_node_name() == text.substr(offset, link_end - offset) and
                        text.substr(line_start, offset - static_cast<int>(line_start)) == "# ") {
                        below_template = true;
                    }
                }
                else if (p.title != "PLATZHALTER") {
                    p.node = target;
                    result.push_back(p);
                }
            }
            offset = link_end - 1;
        }
        if (newline == Glib::ustring::npos) break;
        line_start = newline + 1;
    }
    if (not below_template) error = "Project overview has no linked placeholder heading";
    return result;
}

bool has_link(const Glib::RefPtr<Gtk::TextBuffer>& buffer, int offset, const std::string& link_tag)
{
    for (const auto& tag : buffer->get_iter_at_offset(offset).get_tags()) {
        if (tag->property_name() == link_tag) return true;
    }
    return false;
}

// Locate one uninterrupted internal link to the given node ID.
// A slice includes positions occupied by embedded objects.
bool find_link(const Glib::RefPtr<Gtk::TextBuffer>& buffer,
               const gint64 node_id, int& start, int& end, Glib::ustring& error)
{
    const Glib::ustring text = buffer->get_slice(buffer->begin(), buffer->end());
    if (buffer->get_char_count() != static_cast<int>(text.size())) {
        error = "Could not determine overview character offsets";
        return false;
    }
    const std::string tag = std::string{CtConst::TAG_LINK_PREFIX.c_str()} + "node " + std::to_string(node_id);
    start = -1;
    end = -1;
    bool other = false;
    bool in_link = false;
    for (int i = 0; i < buffer->get_char_count(); ++i) {
        const bool linked = has_link(buffer, i, tag);
        if (linked and not in_link) {
            if (start >= 0) other = true;
            else start = i;
        }
        if (not linked and in_link and end < 0) end = i;
        in_link = linked;
    }
    if (start < 0 or other) {
        error = Glib::ustring{std::string{"Overview must contain exactly one link to node "} + std::to_string(node_id)};
        return false;
    }
    if (end < 0) end = buffer->get_char_count();
    return true;
}

void replace_span(const Glib::RefPtr<Gtk::TextBuffer>& buffer,
                  const int start, const int length, const Glib::ustring& replacement)
{
    std::vector<Glib::ustring> tags;
    for (const auto& tag : buffer->get_iter_at_offset(start).get_tags()) {
        tags.push_back(tag->property_name());
    }
    auto from = buffer->get_iter_at_offset(start);
    auto to = buffer->get_iter_at_offset(start + length);
    buffer->erase(from, to);
    auto at = buffer->get_iter_at_offset(start);
    buffer->insert_with_tags_by_name(at, replacement, tags);
}

bool validate_headline(const Glib::RefPtr<Gtk::TextBuffer>& buffer, const Glib::ustring& expected)
{
    const auto text = buffer->get_text();
    return (text == expected or
            (text.size() > expected.size() and text.substr(0, expected.size()+1) == expected + "\n"));
}

bool validate_reference(const Glib::RefPtr<Gtk::TextBuffer>& buffer,
                        const gint64 id, const Glib::ustring& name,
                        const std::string& prefix, int& start, int& end,
                        Glib::ustring& error)
{
    if (not find_link(buffer, id, start, end, error)) return false;
    const auto text = buffer->get_slice(buffer->begin(), buffer->end());
    if (text.substr(start, end-start) != name) {
        error = Glib::ustring{std::string{"Reference caption does not match node name "} + std::to_string(id)};
        return false;
    }
    auto linebreak = text.rfind('\n', start);
    const auto line_start = linebreak == Glib::ustring::npos ? 0 : linebreak + 1;
    if (not prefix.empty() and text.substr(line_start, start-line_start).raw() != prefix) {
        error = Glib::ustring{std::string{"Unexpected reference prefix for node "} + std::to_string(id)};
        return false;
    }
    if (end < static_cast<int>(text.size()) and text[end] != '\n') {
        error = Glib::ustring{std::string{"Unexpected text following reference for node "} + std::to_string(id)};
        return false;
    }
    return true;
}

} // namespace

bool CtActions::project_cli_list(const int years, const bool all,
                                 Glib::ustring& output, Glib::ustring& error)
{
    CtTreeIter overview = find_overview(_pCtMainWin->get_tree_store(), error);
    if (not overview) return false;
    auto projects = collect_projects(_pCtMainWin->get_tree_store(), overview, error);
    if (not error.empty()) return false;
    if (not all and years > 10) {
        for (const auto& p : projects) {
            if (p.year_width == 1) {
                error = "Only up to 10 years can be selected with the current one-digit project year";
                return false;
            }
        }
    }
    // Resolve the filter first, then align the CT IDs against the longest
    // value in the actual output. The project caption stays unmodified.
    std::vector<const Project*> selected;
    int age{0};
    int previous_year{-1};
    int width{0};
    std::size_t id_width{1};
    for (const auto& p : projects) {
        if (width == 0) width = p.year_width;
        if (not all and width != p.year_width) {
            error = "Mixed one- and two-digit project years require --all until the migration is complete";
            return false;
        }
        if (previous_year >= 0) {
            const int modulus = width == 1 ? 10 : 100;
            age += (previous_year - p.year + modulus) % modulus;
        }
        previous_year = p.year;
        if (all or age < years) {
            selected.push_back(&p);
            const std::string id = p.node ? std::to_string(p.node.get_node_id()) : "-";
            id_width = std::max(id_width, id.size());
        }
    }
    std::ostringstream report;
    for (const Project* p : selected) {
        const std::string id = p->node ? std::to_string(p->node.get_node_id()) : "-";
        report << '[' << std::right << std::setw(static_cast<int>(id_width)) << id << "]  "
               << p->pnr << " - " << p->title << '\n';
    }
    output = Glib::ustring{report.str()};
    return true;
}

bool CtActions::project_cli_create(const Glib::ustring& title,
                                   gint64& new_id, Glib::ustring& error)
{
    if (not valid_title(title)) { error = "Invalid project name"; return false; }
    CtTreeStore& store = _pCtMainWin->get_tree_store();
    CtTreeIter overview = find_overview(store, error);
    if (not overview) return false;
    if (overview.get_node_read_only() or
        overview.get_node_syntax_highlighting() != CtConst::RICH_TEXT_ID) {
        error = "Project overview must be writable rich text";
        return false;
    }
    CtTreeIter placeholder;
    Project placeholder_info;
    int placeholders{0};
    for (CtTreeIter node = overview.first_child(); node; ++node) {
        Project p;
        if (not split_project_name(node.get_node_name().raw(), p)) continue;
        if (p.title == "PLATZHALTER") {
            ++placeholders;
            placeholder = node;
            placeholder_info = p;
        }
    }
    if (placeholders != 1 or not placeholder) {
        error = "Expected exactly one project placeholder";
        return false;
    }
    if (placeholder.get_node_shared_master_id() > 0 or placeholder.get_node_read_only() or
        placeholder.get_node_syntax_highlighting() != CtConst::RICH_TEXT_ID) {
        error = "Project placeholder is shared, read-only or not rich text";
        return false;
    }
    auto source_buffer = placeholder.get_node_text_buffer();
    const Glib::ustring marker = "# <PNR> - <PROJEKTNAME>";
    if (not validate_headline(source_buffer, marker)) {
        error = "The template must start with '# <PNR> - <PROJEKTNAME>'";
        return false;
    }
    if (source_buffer->get_text().find("<PNR>", marker.size()) != Glib::ustring::npos or
        source_buffer->get_text().find("<PROJEKTNAME>", marker.size()) != Glib::ustring::npos) {
        error = "Template contains additional placeholders; this prototype only replaces the first line";
        return false;
    }
    const int number = std::stoi(placeholder_info.pnr.substr(placeholder_info.pnr.find('P')+1));
    if (number >= 99) { error = "Reached P99; next numbering rule is not defined"; return false; }
    const auto next_id = placeholder_info.pnr.substr(0, placeholder_info.pnr.find('P')+1)
                        + (number+1 < 10 ? "0" : "") + std::to_string(number+1);
    const Glib::ustring old_name = placeholder.get_node_name();
    const Glib::ustring next_placeholder = Glib::ustring{next_id} + " - PLATZHALTER";
    const Glib::ustring project_name = Glib::ustring{placeholder_info.pnr} + " - " + title;
    for (CtTreeIter node = overview.first_child(); node; ++node) {
        if (node.get_node_name() == next_placeholder or node.get_node_name() == project_name) {
            error = "Project or next placeholder already exists";
            return false;
        }
    }

    auto overview_buffer = overview.get_node_text_buffer();
    int start{-1}, end{-1};
    if (not validate_reference(overview_buffer, placeholder.get_node_id(), old_name,
                               "# ", start, end, error)) return false;
    auto overview_text = overview_buffer->get_slice(overview_buffer->begin(), overview_buffer->end());
    if (end >= static_cast<int>(overview_text.size()) or overview_text[end] != '\n' or
        end + 1 >= static_cast<int>(overview_text.size())) {
        error = "Expected the project register below the placeholder reference";
        return false;
    }

    // All checks above are read-only. No automatic rollback is provided below.
    Gtk::TreeModel::iterator copy = _node_add(CtDuplicateShared::Duplicate,
        false/*add_as_child*/, &placeholder, _pCtMainWin, &project_name, false/*select_new*/);
    if (not copy) { error = "Could not duplicate project template"; return false; }
    CtTreeIter project = store.to_ct_tree_iter(copy);
    new_id = project.get_node_id();
    auto project_buffer = project.get_node_text_buffer();
    replace_span(project_buffer, 2, static_cast<int>(marker.size())-2, project_name);
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::nbuf, true, &project);
    placeholder.set_node_name(next_placeholder);
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::npro, false, &placeholder);
    replace_span(overview_buffer, start, end-start, next_placeholder);
    const int insertion = end + static_cast<int>(next_placeholder.size()) - (end-start) + 1;
    const Glib::ustring new_line = "- " + project_name + "\n";
    auto insert_at = overview_buffer->get_iter_at_offset(insertion);
    overview_buffer->insert(insert_at, new_line);
    auto new_from = overview_buffer->get_iter_at_offset(insertion);
    auto new_to = overview_buffer->get_iter_at_offset(insertion + static_cast<int>(new_line.size()));
    overview_buffer->remove_all_tags(new_from, new_to);
    auto tag = _pCtMainWin->get_text_tag_name_exist_or_create(CtConst::TAG_LINK,
                                                              "node " + std::to_string(new_id));
    auto link_from = overview_buffer->get_iter_at_offset(insertion + 2);
    auto link_to = overview_buffer->get_iter_at_offset(insertion + 2 + static_cast<int>(project_name.size()));
    overview_buffer->apply_tag_by_name(tag, link_from, link_to);
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::nbuf, true, &overview);
    return true;
}

bool CtActions::project_cli_rename(const Glib::ustring& pnr, const Glib::ustring& new_title,
                                   Glib::ustring& error)
{
    if (not valid_title(new_title)) { error = "Invalid new project name"; return false; }
    CtTreeStore& store = _pCtMainWin->get_tree_store();
    CtTreeIter overview = find_overview(store, error);
    if (not overview) return false;
    auto projects = collect_projects(store, overview, error);
    if (not error.empty()) return false;
    Project chosen;
    int matches{0};
    for (const auto& p : projects) {
        if (p.pnr == pnr.raw()) { chosen = p; ++matches; }
    }
    if (matches != 1) { error = "Project number not found or ambiguous"; return false; }
    if (not chosen.node) { error = "Project reference is missing or invalid"; return false; }
    if (chosen.node.get_node_read_only() or overview.get_node_read_only() or
        chosen.node.get_node_syntax_highlighting() != CtConst::RICH_TEXT_ID or
        overview.get_node_syntax_highlighting() != CtConst::RICH_TEXT_ID) {
        error = "Project and overview must be writable rich-text nodes";
        return false;
    }
    const Glib::ustring old_name = chosen.node.get_node_name();
    const Glib::ustring new_name = pnr + " - " + new_title;
    if (old_name == new_name) return true;
    for (const auto& p : projects) {
        if (p.node and p.node.get_node_id() != chosen.node.get_node_id() and
            p.node.get_node_name() == new_name) {
            error = "New project title already exists";
            return false;
        }
    }
    auto project_buffer = chosen.node.get_node_text_buffer();
    const Glib::ustring old_headline = "# " + old_name;
    if (not validate_headline(project_buffer, old_headline)) {
        error = "Unexpected first line; expected '# PNR - old name'";
        return false;
    }
    auto overview_buffer = overview.get_node_text_buffer();
    int start{-1}, end{-1};
    if (not validate_reference(overview_buffer, chosen.node.get_node_id(), old_name,
                               "", start, end, error)) return false;

    replace_span(project_buffer, 2, static_cast<int>(old_name.size()), new_name);
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::nbuf, true, &chosen.node);
    chosen.node.set_node_name(new_name);
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::npro, false, &chosen.node);
    replace_span(overview_buffer, start, end-start, new_name);
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::nbuf, true, &overview);
    return true;
}

// Validate first; perform=false changes nothing. All validation precedes the
// interactive confirmation, so invalid PNRs can never trigger a prompt.
bool CtActions::project_cli_delete(const Glib::ustring& pnr, bool recursive, bool perform,
                                   Glib::ustring& display_name, Glib::ustring& error)
{
    CtTreeStore& store = _pCtMainWin->get_tree_store();
    CtTreeIter overview = find_overview(store, error);
    if (not overview) return false;
    const auto projects = collect_projects(store, overview, error);
    if (not error.empty()) return false;

    Project chosen;
    int matches{0};
    for (const auto& project : projects) {
        if (project.pnr == pnr.raw()) { chosen = project; ++matches; }
    }
    if (matches != 1) { error = "Project number not found or ambiguous"; return false; }
    if (not chosen.node) { error = "Project reference is missing or invalid"; return false; }
    display_name = chosen.node.get_node_name();
    if (display_name.raw() != chosen.pnr + " - " + chosen.title) {
        error = "Project node name differs from its register entry";
        return false;
    }
    if (overview.get_node_read_only() or
        overview.get_node_syntax_highlighting() != CtConst::RICH_TEXT_ID or
        chosen.node.get_node_read_only()) {
        error = "Project and overview must be writable";
        return false;
    }

    const std::vector<gint64> descendants = chosen.node.get_children_node_ids();
    if (not recursive and not descendants.empty()) {
        error = "Project has child nodes; use --recursive to delete them too";
        return false;
    }
    // Shared nodes are deliberately excluded in this initial implementation,
    // including any in a recursive subtree. CherryTree's GUI has more
    // elaborate master reassignment logic; do not reimplement that here.
    CtSharedNodesMap shared_nodes;
    store.populate_shared_nodes_map(shared_nodes);
    auto check_shared = [&](gint64 id) {
        const CtTreeIter node = store.get_node_from_node_id(id);
        return (node and node.get_node_shared_master_id() > 0) or
               shared_nodes.find(id) != shared_nodes.end();
    };
    if (check_shared(chosen.node.get_node_id())) {
        error = "Project uses shared nodes; delete it manually";
        return false;
    }
    for (gint64 id : descendants) {
        if (check_shared(id)) {
            error = "Project subtree uses shared nodes; delete it manually";
            return false;
        }
    }

    auto buffer = overview.get_node_text_buffer();
    int start{-1}, end{-1};
    if (not validate_reference(buffer, chosen.node.get_node_id(), display_name,
                               "", start, end, error)) return false;

    const Glib::ustring text = buffer->get_slice(buffer->begin(), buffer->end());
    const auto prev_newline = text.rfind('\n', static_cast<Glib::ustring::size_type>(start));
    const int line_start = prev_newline == Glib::ustring::npos ? 0 : static_cast<int>(prev_newline) + 1;
    const Glib::ustring prefix = text.substr(line_start, start - line_start);
    // Status indicators belong to the register entry, not to the project ID.
    if (prefix.size() != 2 or prefix[1] != ' ' or
        Glib::ustring{"-vxrwi"}.find(prefix[0]) == Glib::ustring::npos) {
        error = "Unexpected project register line prefix; no changes made";
        return false;
    }
    int erase_start = line_start;
    int erase_end = end;
    if (erase_end < static_cast<int>(text.size()) and text[erase_end] == '\n') {
        ++erase_end;
    }
    else if (erase_start > 0) {
        --erase_start;
    }

    // When deleting the first project directly below the template, the
    // template is restored to that project's PNR. Validate BEFORE prompting.
    CtTreeIter placeholder;
    Project placeholder_info;
    int placeholders{0};
    for (CtTreeIter node = overview.first_child(); node; ++node) {
        Project candidate;
        if (split_project_name(node.get_node_name().raw(), candidate) and
            candidate.title == "PLATZHALTER") {
            placeholder = node;
            placeholder_info = candidate;
            ++placeholders;
        }
    }
    if (placeholders != 1 or not placeholder) {
        error = "Expected exactly one project placeholder";
        return false;
    }
    int template_start{-1}, template_end{-1};
    if (not validate_reference(buffer, placeholder.get_node_id(),
                               placeholder.get_node_name(), "# ",
                               template_start, template_end, error)) return false;
    if (template_end >= static_cast<int>(text.size()) or text[template_end] != '\n') {
        error = "Expected a complete placeholder heading in the register";
        return false;
    }
    const bool reset_placeholder = erase_start == template_end + 1;
    if (reset_placeholder) {
        const auto sep = chosen.pnr.find('P');
        const auto template_sep = placeholder_info.pnr.find('P');
        const int project_num = std::stoi(chosen.pnr.substr(sep + 1));
        const int template_num = std::stoi(placeholder_info.pnr.substr(template_sep + 1));
        if (chosen.pnr.substr(0, sep) != placeholder_info.pnr.substr(0, template_sep) or
            template_num != project_num + 1 or
            placeholder.get_node_read_only() or
            placeholder.get_node_shared_master_id() > 0 or
            placeholder.get_node_syntax_highlighting() != CtConst::RICH_TEXT_ID or
            not validate_headline(placeholder.get_node_text_buffer(),
                                  "# <PNR> - <PROJEKTNAME>")) {
            error = "Cannot safely reset the placeholder after deleting the newest project";
            return false;
        }
    }

    if (not perform) return true; // the caller can now ask for confirmation

    // No rollback in this prototype. The caller saves only after success.
    buffer->erase(buffer->get_iter_at_offset(erase_start),
                  buffer->get_iter_at_offset(erase_end));
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::nbuf, true, &overview);

    if (_pCtMainWin->curr_tree_iter()) {
        const gint64 selected_id = _pCtMainWin->curr_tree_iter().get_node_id();
        if (selected_id == chosen.node.get_node_id() or
            std::find(descendants.begin(), descendants.end(), selected_id) != descendants.end()) {
            _pCtMainWin->get_tree_view().set_cursor_safe(overview);
        }
    }
    CtTreeIter parent = chosen.node.parent();
    const gint64 node_id = chosen.node.get_node_id();
    _pCtMainWin->resetPrevTreeIter();
    _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::ndel, false, &chosen.node);
    store.get_store()->erase(chosen.node);
    store.nodes_sequences_fix(parent, false);

    bool removed_bookmark = store.bookmarks_remove(node_id);
    for (gint64 id : descendants) removed_bookmark = store.bookmarks_remove(id) or removed_bookmark;
    if (removed_bookmark) {
        _pCtMainWin->menu_set_bookmark_menu_items();
        _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::book);
    }
    if (reset_placeholder) {
        const Glib::ustring restored = pnr + " - PLATZHALTER";
        placeholder.set_node_name(restored);
        _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::npro, false, &placeholder);
        replace_span(buffer, template_start, template_end - template_start, restored);
        _pCtMainWin->update_window_save_needed(CtSaveNeededUpdType::nbuf, true, &overview);
    }
    return true;
}
