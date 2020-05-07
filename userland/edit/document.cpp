#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "command.h"
#include "document.h"
#include "key_press.h"
#include "panel.h"

UniquePtr<Document> Document::create_from_file(const String& path, Panel& panel) {
    FILE* file = fopen(path.string(), "r");
    if (!file) {
        panel.send_status_message(String::format("new file: `%s'", path.string()));
        return make_unique<Document>(Vector<Line>(), path, panel, LineMode::Multiple);
    }

    Vector<Line> lines;
    char* line = nullptr;
    size_t line_max = 0;
    ssize_t line_length;
    while ((line_length = getline(&line, &line_max, file)) != -1) {
        char* trailing_newline = strchr(line, '\n');
        if (trailing_newline) {
            *trailing_newline = '\0';
        }

        lines.add(Line(String(line)));
    }

    UniquePtr<Document> ret;

    if (ferror(file)) {
        panel.send_status_message(String::format("error reading file: `%s'", path.string()));
        ret = Document::create_empty(panel);
    } else {
        ret = make_unique<Document>(move(lines), path, panel, LineMode::Multiple);
    }

    if (fclose(file)) {
        panel.send_status_message(String::format("error closing file: `%s'", path.string()));
    }

    return ret;
}

UniquePtr<Document> Document::create_empty(Panel& panel) {
    return make_unique<Document>(Vector<Line>(), "", panel, LineMode::Multiple);
}

UniquePtr<Document> Document::create_single_line(Panel& panel) {
    return make_unique<Document>(Vector<Line>(), "", panel, LineMode::Single);
}

Document::Document(Vector<Line> lines, String name, Panel& panel, LineMode mode)
    : m_lines(move(lines)), m_name(move(name)), m_panel(panel), m_line_mode(mode) {
    if (m_lines.empty()) {
        m_lines.add(Line(""));
    }
}

Document::~Document() {}

String Document::content_string() const {
    if (single_line_mode()) {
        return m_lines.first().contents();
    }

    String ret;
    for (auto& line : m_lines) {
        ret += line.contents();
        ret += "\n";
    }
    return ret;
}

void Document::display() const {
    m_panel.clear();
    for (int line_num = m_row_offset; line_num < m_lines.size() && line_num - m_row_offset < m_panel.rows(); line_num++) {
        m_lines[line_num].render(const_cast<Document&>(*this).panel(), m_col_offset, line_num - m_row_offset);
    }
    m_panel.flush();
    m_needs_display = false;
}

Line& Document::line_at_cursor() {
    return m_lines[m_panel.cursor_row() + m_row_offset];
}

int Document::line_index_at_cursor() const {
    return line_at_cursor().index_of_col_position(cursor_col_position());
}

int Document::cursor_col_position() const {
    return m_panel.cursor_col() + m_col_offset;
}

int Document::cursor_row_position() const {
    return m_panel.cursor_row() + m_row_offset;
}

void Document::move_cursor_right() {
    auto& line = line_at_cursor();
    int index_into_line = line_index_at_cursor();
    if (index_into_line == line.length()) {
        if (&line == &m_lines.last()) {
            return;
        }

        move_cursor_down();
        move_cursor_to_line_start();
        return;
    }

    int new_col_position = line.col_position_of_index(index_into_line + 1);
    int current_col_position = cursor_col_position();
    int cols_to_advance = new_col_position - current_col_position;

    m_max_cursor_col = new_col_position;

    int cursor_col = m_panel.cursor_col();
    if (cursor_col + cols_to_advance >= m_panel.cols()) {
        m_col_offset += cursor_col + cols_to_advance - m_panel.cols() + 1;
        m_panel.set_cursor_col(m_panel.cols() - 1);
        set_needs_display();
        return;
    }

    m_panel.set_cursor_col(m_panel.cursor_col() + cols_to_advance);
}

void Document::move_cursor_left() {
    auto& line = line_at_cursor();
    int index_into_line = line_index_at_cursor();
    if (index_into_line == 0) {
        if (&line == &m_lines.first()) {
            return;
        }

        move_cursor_up();
        move_cursor_to_line_end();
        return;
    }

    int new_col_position = line.col_position_of_index(index_into_line - 1);
    int current_col_position = cursor_col_position();
    int cols_to_recede = current_col_position - new_col_position;

    m_max_cursor_col = new_col_position;

    int cursor_col = m_panel.cursor_col();
    if (cursor_col - cols_to_recede < 0) {
        m_col_offset += cursor_col - cols_to_recede;
        m_panel.set_cursor_col(0);
        set_needs_display();
        return;
    }

    m_panel.set_cursor_col(m_panel.cursor_col() - cols_to_recede);
}

void Document::move_cursor_down() {
    int cursor_row = m_panel.cursor_row();
    if (cursor_row + m_row_offset == m_lines.size() - 1) {
        move_cursor_to_line_end();
        return;
    }

    if (cursor_row == m_panel.rows() - 1) {
        m_row_offset++;
        set_needs_display();
    } else {
        m_panel.set_cursor_row(cursor_row + 1);
    }

    clamp_cursor_to_line_end();
}

void Document::move_cursor_up() {
    int cursor_row = m_panel.cursor_row();
    if (cursor_row == 0 && m_row_offset == 0) {
        m_panel.set_cursor_col(0);
        m_max_cursor_col = 0;
        return;
    }

    if (cursor_row == 0) {
        m_row_offset--;
        set_needs_display();
    } else {
        m_panel.set_cursor_row(cursor_row - 1);
    }

    clamp_cursor_to_line_end();
}

void Document::clamp_cursor_to_line_end() {
    auto& line = line_at_cursor();
    int current_col = cursor_col_position();
    int max_col = line.col_position_of_index(line.length());
    if (current_col == max_col) {
        return;
    }

    if (current_col > max_col) {
        move_cursor_to_line_end(UpdateMaxCursorCol::No);
        return;
    }

    if (m_max_cursor_col > current_col) {
        int new_line_index = line.index_of_col_position(m_max_cursor_col);
        int new_cursor_col = line.col_position_of_index(new_line_index);
        if (new_cursor_col >= m_panel.cols()) {
            m_col_offset = new_cursor_col - m_panel.cols() + 1;
            m_panel.set_cursor_col(m_panel.cols() - 1);
            set_needs_display();
            return;
        }

        m_panel.set_cursor_col(new_cursor_col);
    }
}

void Document::move_cursor_to_line_start() {
    m_panel.set_cursor_col(0);
    if (m_col_offset != 0) {
        m_col_offset = 0;
        set_needs_display();
    }
    m_max_cursor_col = 0;
}

void Document::move_cursor_to_line_end(UpdateMaxCursorCol should_update_max_cursor_col) {
    auto& line = line_at_cursor();
    int new_col_position = line.col_position_of_index(line.length());

    if (should_update_max_cursor_col == UpdateMaxCursorCol::Yes) {
        m_max_cursor_col = new_col_position;
    }

    if (new_col_position >= m_panel.cols()) {
        m_col_offset = new_col_position - m_panel.cols() + 1;
        m_panel.set_cursor_col(m_panel.cols() - 1);
        set_needs_display();
        return;
    }

    m_panel.set_cursor_col(new_col_position);

    if (m_col_offset != 0) {
        m_col_offset = 0;
        set_needs_display();
    }
}

void Document::merge_lines(int l1i, int l2i) {
    auto& l1 = m_lines[l1i];
    auto& l2 = m_lines[l2i];

    l1.combine_line(l2);
    m_lines.remove(l2i);
}

void Document::insert_char(char c) {
    push_command<InsertCommand>(c);
}

void Document::delete_char(DeleteCharMode mode) {
    push_command<DeleteCommand>(mode);
}

void Document::split_line_at_cursor() {
    push_command<SplitLineCommand>();
}

void Document::redo() {
    if (m_command_stack_index == m_command_stack.size()) {
        return;
    }

    auto& command = *m_command_stack[m_command_stack_index++];
    command.execute();
}

void Document::undo() {
    if (m_command_stack_index == 0) {
        return;
    }

    auto& command = *m_command_stack[--m_command_stack_index];
    command.undo();
}

Document::Snapshot Document::snapshot() const {
    return { Vector<Line>(m_lines), m_row_offset,     m_col_offset,           m_panel.cursor_row(),
             m_panel.cursor_col(),  m_max_cursor_col, m_document_was_modified };
}

void Document::restore(Snapshot s) {
    m_lines = move(s.lines);
    m_row_offset = s.row_offset;
    m_col_offset = s.col_offset;
    m_max_cursor_col = s.max_cursor_col;
    m_document_was_modified = s.document_was_modified;

    update_search_results();
    m_panel.set_cursor(s.cursor_row, s.cursor_col);
    set_needs_display();
}

void Document::notify_panel_size_changed() {
    while (m_panel.cursor_row() >= m_panel.rows()) {
        move_cursor_up();
    }

    while (m_panel.cursor_col() >= m_panel.cols()) {
        move_cursor_left();
    }

    display();
}

void Document::save() {
    struct stat st;
    if (m_name.is_empty()) {
        String result = m_panel.prompt("Save as: ");
        if (access(result.string(), F_OK) == 0) {
            String ok = m_panel.prompt(String::format("Are you sure you want to overwrite file `%s'? ", result.string()));
            if (ok != "y" && ok != "yes") {
                return;
            }
        }

        m_name = move(result);
        st.st_mode = 0644;
    } else {
        if (stat(m_name.string(), &st)) {
            m_panel.send_status_message(String::format("Error looking up file - `%s'", strerror(errno)));
            return;
        }

        if (access(m_name.string(), W_OK)) {
            m_panel.send_status_message(String::format("Permission to write file `%s' denied", m_name.string()));
            return;
        }
    }

    auto temp_path_template = String::format("%sXXXXXX", m_name.string());
    char* temp_path = temp_path_template.string();
    int fd = mkstemp(temp_path);
    if (fd < 0) {
        m_panel.send_status_message(String::format("Failed to create a temp file - `%s'", strerror(errno)));
        return;
    }

    FILE* file = fdopen(fd, "w");
    if (!file) {
        m_panel.send_status_message(String::format("Failed to save - `%s'", strerror(errno)));
        return;
    }

    if (m_lines.size() == 1 && m_lines.first().empty()) {
        if (ftruncate(fileno(file), 0)) {
            m_panel.send_status_message(String::format("Failed to sync to disk - `%s'", strerror(errno)));
            fclose(file);
            return;
        }
    } else {
        for (auto& line : m_lines) {
            fprintf(file, "%s\n", line.contents().string());
        }
    }

    if (ferror(file)) {
        m_panel.send_status_message(String::format("Failed to write to disk - `%s'", strerror(errno)));
        fclose(file);
        return;
    }

    if (fchmod(fileno(file), st.st_mode)) {
        m_panel.send_status_message(String::format("Faild to sync file metadata - `%s'", strerror(errno)));
        fclose(file);
        return;
    }

    if (fclose(file)) {
        m_panel.send_status_message(String::format("Failed to sync to disk - `%s'", strerror(errno)));
        return;
    }

    if (rename(temp_path, m_name.string())) {
        m_panel.send_status_message(String::format("Failed to overwrite file - `%s'", strerror(errno)));
        return;
    }

    m_panel.send_status_message(String::format("Successfully saved file: `%s'", m_name.string()));
    m_document_was_modified = false;
}

void Document::quit() {
    if (m_document_was_modified) {
        auto result = m_panel.prompt("Quit without saving? ");
        if (result != "y" && result != "yes") {
            return;
        }
    }

    exit(0);
}

void Document::update_search_results() {
    m_search_result_count = 0;
    if (m_search_text.is_empty()) {
        return;
    }

    for (auto& line : m_lines) {
        int num = line.search(m_search_text);
        m_search_result_count += num;
        if (num > 0) {
            set_needs_display();
        }
    }
}

void Document::clear_search_results() {
    if (m_search_result_count == 0) {
        return;
    }

    for (auto& line : m_lines) {
        line.clear_search();
    }
    set_needs_display();
}

void Document::set_search_text(String text) {
    if (m_search_text == text) {
        return;
    }

    m_search_text = move(text);
    update_search_results();
}

void Document::enter_interactive_search() {
    m_panel.enter_search(m_search_text);
    m_panel.send_status_message(String::format("Found %d result(s)", m_search_result_count));
}

void Document::notify_key_pressed(KeyPress press) {
    if (press.modifiers & KeyPress::Modifier::Control) {
        switch (press.key) {
            case 'F':
                enter_interactive_search();
                break;
            case 'Q':
            case 'W':
                quit();
                break;
            case 'S':
                if (!single_line_mode()) {
                    save();
                }
                break;
            case 'Y':
                redo();
                break;
            case 'Z':
                undo();
                break;
            default:
                break;
        }

        if (needs_display()) {
            display();
        }
        return;
    }

    switch (press.key) {
        case KeyPress::Key::LeftArrow:
            move_cursor_left();
            break;
        case KeyPress::Key::RightArrow:
            move_cursor_right();
            break;
        case KeyPress::Key::DownArrow:
            move_cursor_down();
            break;
        case KeyPress::Key::UpArrow:
            move_cursor_up();
            break;
        case KeyPress::Key::Home:
            move_cursor_to_line_start();
            break;
        case KeyPress::Key::End:
            move_cursor_to_line_end();
            break;
        case KeyPress::Key::Backspace:
            delete_char(DeleteCharMode::Backspace);
            break;
        case KeyPress::Key::Delete:
            delete_char(DeleteCharMode::Delete);
            break;
        case KeyPress::Key::Enter:
            if (!single_line_mode()) {
                split_line_at_cursor();
            }
            break;
        case KeyPress::Key::Escape:
            clear_search_results();
            break;
        default:
            if (isascii(press.key)) {
                insert_char(press.key);
            }
            break;
    }

    if (needs_display()) {
        display();
    }
}