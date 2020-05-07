#pragma once

#include <liim/pointers.h>
#include <liim/vector.h>

#include "line.h"

class Command;
struct KeyPress;
class Panel;

enum class UpdateMaxCursorCol { No, Yes };

enum class DeleteCharMode { Backspace, Delete };

enum class LineMode { Single, Multiple };

class Document {
public:
    static UniquePtr<Document> create_from_file(const String& path, Panel& panel);
    static UniquePtr<Document> create_empty(Panel& panel);
    static UniquePtr<Document> create_single_line(Panel& panel);

    struct Snapshot {
        Vector<Line> lines;
        int row_offset { 0 };
        int col_offset { 0 };
        int cursor_row { 0 };
        int cursor_col { 0 };
        int max_cursor_col { 0 };
        bool document_was_modified { false };
    };

    Document(Vector<Line> lines, String name, Panel& panel, LineMode mode);
    ~Document();

    void display() const;

    Panel& panel() { return m_panel; }
    const Panel& panel() const { return m_panel; }

    void notify_key_pressed(KeyPress press);
    void notify_panel_size_changed();

    void save();
    void quit();

    bool single_line_mode() const { return m_line_mode == LineMode::Single; }

    String content_string() const;

    bool convert_tabs_to_spaces() const { return m_convert_tabs_to_spaces; }
    void set_convert_tabs_to_spaces(bool b) { m_convert_tabs_to_spaces = b; }

    bool needs_display() const { return m_needs_display; }
    void set_needs_display() { m_needs_display = true; }

    int cursor_col_position() const;
    int cursor_row_position() const;

    bool modified() const { return m_document_was_modified; }

    const String& name() const { return m_name; }

    const String& search_text() const { return m_search_text; }
    void set_search_text(String text);

    void move_cursor_left();
    void move_cursor_right();
    void move_cursor_down();
    void move_cursor_up();
    void move_cursor_to_line_start();
    void move_cursor_to_line_end(UpdateMaxCursorCol update = UpdateMaxCursorCol::Yes);

    Line& line_at_cursor();
    const Line& line_at_cursor() const { return const_cast<Document&>(*this).line_at_cursor(); }
    int line_index_at_cursor() const;
    int num_lines() const { return m_lines.size(); }
    void remove_line(int index) { m_lines.remove(index); }
    void insert_line(Line&& line, int index) { m_lines.insert(move(line), index); }

    void merge_lines(int l1, int l2);

    void set_was_modified(bool b) { m_document_was_modified = b; }

    Snapshot snapshot() const;
    void restore(Snapshot snapshot);

private:
    void clamp_cursor_to_line_end();

    void update_search_results();
    void clear_search_results();
    void enter_interactive_search();

    void split_line_at_cursor();
    void insert_char(char c);
    void delete_char(DeleteCharMode mode);

    void redo();
    void undo();

    template<typename C, typename... Args>
    void push_command(Args... args) {
        // This means some undo's have taken place, and the user started typing
        // something else, so the redo stack will be discarded.
        if (m_command_stack_index != m_command_stack.size()) {
            m_command_stack.resize(m_command_stack_index);
        }

        if (m_command_stack.size() >= m_max_undo_stack) {
            // FIXME: this makes the Vector data structure very inefficent
            //        a doubly-linked list would be much nicer.
            m_command_stack.remove(0);
        }

        auto command = make_unique<C>(*this, forward<Args>(args)...);
        command->execute();
        m_command_stack.add(move(command));
        m_command_stack_index++;
    }

    Vector<Line> m_lines;
    String m_name;
    Panel& m_panel;
    LineMode m_line_mode { LineMode::Multiple };

    Vector<UniquePtr<Command>> m_command_stack;
    int m_command_stack_index { 0 };

    String m_search_text;
    int m_search_result_count { 0 };

    int m_row_offset { 0 };
    int m_col_offset { 0 };
    int m_max_cursor_col { 0 };
    bool m_document_was_modified { false };

    int m_max_undo_stack { 30 };
    bool m_convert_tabs_to_spaces { true };
    mutable bool m_needs_display { false };
};