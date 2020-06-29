#include <app/event.h>
#include <app/window.h>
#include <graphics/renderer.h>
#include <unistd.h>

#include "terminal_widget.h"

constexpr int cell_width = 8;
constexpr int cell_height = 16;

TerminalWidget::TerminalWidget() {
    m_pseudo_terminal_wrapper = make_unique<App::FdWrapper>(m_pseudo_terminal.master_fd());
    m_pseudo_terminal_wrapper->set_selected_events(App::NotifyWhen::Readable);
    m_pseudo_terminal_wrapper->enable_notifications();
    m_pseudo_terminal_wrapper->on_readable = [this] {
        char buf[BUFSIZ];
        for (;;) {
            ssize_t ret = read(m_pseudo_terminal.master_fd(), buf, sizeof(buf));
            if (ret < 0) {
                perror("terminal: read");
                exit(1);
            } else if (ret == 0) {
                break;
            }

            for (ssize_t i = 0; i < ret; i++) {
                m_tty.on_char(buf[i]);
            }
        }
        window()->draw();
    };
}

void TerminalWidget::render() {
    Renderer renderer(*window()->pixels());
    auto x_offset = rect().x();
    auto y_offset = rect().y();

    auto& rows = m_tty.rows();
    for (auto r = m_tty.row_offset(); r < rows.size() && r < m_tty.row_offset() + m_tty.row_count(); r++) {
        auto& row = rows[r];
        auto y = y_offset + (r - m_tty.row_offset()) * cell_height;
        for (auto c = 0; c < row.size(); c++) {
            auto& cell = row[c];
            auto x = x_offset + c * cell_width;

            bool at_cursor =
                r - (rows.size() - m_tty.row_count()) == m_tty.cursor_row() && c == m_tty.cursor_col() && !m_tty.cursor_hidden();
            if (!cell.dirty && !at_cursor) {
                continue;
            }

            cell.dirty = at_cursor;

            auto fg = cell.fg;
            auto bg = cell.bg;
            if (at_cursor) {
                swap(fg, bg);
            }

            if (cell.inverted) {
                swap(fg, bg);
            }

            renderer.fill_rect({ x, y, cell_width, cell_height }, bg);
            renderer.render_text(x, y, String(cell.ch), fg, cell.bold ? Font::bold_font() : Font::default_font());
        }
    }
}

void TerminalWidget::on_resize() {
    int rows = rect().height() / cell_height;
    int cols = rect().width() / cell_width;
    m_tty.resize(rows, cols);
    m_pseudo_terminal.set_size(rows, cols);
}

void TerminalWidget::on_key_event(App::KeyEvent& event) {
    m_pseudo_terminal.handle_key_event(event.key(), event.flags(), event.ascii());
}

void TerminalWidget::on_mouse_event(App::MouseEvent& event) {
    if (event.scroll() == SCROLL_DOWN) {
        m_tty.scroll_down();
        window()->draw();
    } else if (event.scroll() == SCROLL_UP) {
        m_tty.scroll_up();
        window()->draw();
    }
}