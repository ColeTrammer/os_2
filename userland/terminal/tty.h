#pragma once

#include <liim/vector.h>

#include "vga_buffer.h"

class TTY {
public:
    TTY(VgaBuffer&, int fd);
    ~TTY() {}

    void scroll_up();
    void scroll_down();

    void scroll_to_bottom();
    void scroll_to_top();

    void on_char(char);

    void refresh() { m_buffer.refresh(); }

private:
    void save_pos() {
        m_saved_row = m_row;
        m_saved_col = m_col;
    }
    void restore_pos() {
        m_row = m_saved_row;
        m_col = m_saved_col;
    }

    void write(const char* buffer, size_t length);

    void draw(char);
    void update_cursor();

    void handle_escape_sequence();
    void on_next_escape_char(char);

    void clamp_cursor();

    int m_row { 0 };
    int m_col { 0 };

    int m_saved_row { 0 };
    int m_saved_col { 0 };

    bool m_cursor_hidden { false };
    bool m_in_escape { false };
    int m_escape_index { 0 };
    char m_escape_buffer[50] { 0 };

    VgaBuffer& m_buffer;
    Vector<VgaBuffer::Row> m_above_rows;
    Vector<VgaBuffer::Row> m_below_rows;
    int m_fd;
};