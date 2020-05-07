#pragma once

#include <liim/maybe.h>
#include <liim/vector.h>
#include <time.h>

#include "panel.h"

class TerminalPanel final : public Panel {
public:
    TerminalPanel();
    virtual ~TerminalPanel() override;

    virtual int rows() const override { return m_rows; }
    virtual int cols() const override { return m_cols; }

    virtual void clear() override;
    virtual void set_text_at(int row, int col, char c, CharacterMetadata metadata) override;
    virtual void flush() override;
    virtual void enter() override;
    virtual void send_status_message(String message) override;
    virtual String prompt(const String& message) override;
    virtual void enter_search(String starting_text) override;

    virtual void set_cursor(int row, int col) override;

    virtual int cursor_col() const { return m_cursor_col; }
    virtual int cursor_row() const { return m_cursor_row; }

    void set_stop_on_enter(bool b) { m_stop_on_enter = b; }

    void set_coordinates(int row_offset, int col_offset, int rows, int cols);

    int col_offset() const { return m_col_offset; }
    int row_offset() const { return m_row_offset; }

private:
    struct Info {
        char ch;
        CharacterMetadata metadata;
    };

    TerminalPanel(int rows, int cols, int row_off, int col_off);

    Maybe<KeyPress> read_key();

    void draw_cursor();
    void draw_status_message();

    const String& string_for_metadata(CharacterMetadata metadata) const;

    void print_char(char c, CharacterMetadata metadata);
    void flush_row(int line);

    String enter_prompt(const String& message);

    int index(int row, int col) const { return row * m_cols + col; }

    Vector<Info> m_screen_info;
    String m_status_message;
    time_t m_status_message_time { 0 };
    String m_prompt_buffer;
    CharacterMetadata m_last_metadata_rendered;
    int m_rows { 0 };
    int m_cols { 0 };
    int m_cursor_row { 0 };
    int m_cursor_col { 0 };
    int m_row_offset { 0 };
    int m_col_offset { 0 };
    bool m_stop_on_enter { false };
};