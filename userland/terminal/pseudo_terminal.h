#pragma once

#include <liim/string.h>
#include <sys/types.h>
#include <kernel/hal/input.h>

enum class MouseTrackingMode {
    None,
    X10,
    X11,
    Hilite,
    Cell,
    All,
};

enum class MouseReportingMode {
    None,
    X10,
    X11,
    Hilite,
    UTF8,
    SGR,
    URXVT,
};

class PsuedoTerminal {
public:
    PsuedoTerminal();
    ~PsuedoTerminal();

    int master_fd() const { return m_master_fd; }
    pid_t child_pid() const { return m_child_pid; }

    void send_clipboard_contents(const String& contents);
    void handle_key_event(key key, int flags, char ascii);
    bool handle_mouse_event(mouse_button_state left, mouse_button_state right, int row, int col, scroll_state scroll_state);
    void set_size(int rows, int cols);

    void write(const String& message);

    void set_application_cursor_keys(bool b) { m_application_cursor_keys = b; }
    void set_bracketed_paste(bool b) { m_bracketed_paste = b; }

    MouseReportingMode mouse_reporting_mode() const { return m_mouse_reporting_mode; }
    MouseTrackingMode mouse_tracking_mode() const { return m_mouse_tracking_mode; }

    void set_mouse_tracking_mode(MouseTrackingMode mode) { m_mouse_tracking_mode = mode; }
    void set_mouse_reporting_mode(MouseReportingMode mode) { m_mouse_reporting_mode = mode; }

    void reset_mouse_tracking_mode(MouseTrackingMode tracking_mode);
    void reset_mouse_reporting_mode();

private:
    int m_master_fd { -1 };
    int m_rows { 25 };
    int m_cols { 80 };
    pid_t m_child_pid { -1 };
    int m_mouse_last_row { -1 };
    int m_mouse_last_col { -1 };
    MouseTrackingMode m_mouse_tracking_mode { MouseTrackingMode::None };
    MouseReportingMode m_mouse_reporting_mode { MouseReportingMode::None };
    bool m_application_cursor_keys { false };
    bool m_bracketed_paste { false };
    bool m_mouse_left_down { false };
    bool m_mouse_right_down { false };
};
