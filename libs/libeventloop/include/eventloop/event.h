#pragma once

#include <liim/function.h>
#include <kernel/hal/input.h>

namespace App {

class Event {
public:
    enum class Type {
        Invalid,
        Key,
        Mouse,
        Resize,
        Window,
        WindowState,
        Callback,
        Timer,
        ThemeChange,
    };

    Event(Type type) : m_type(type) {};
    virtual ~Event() {}

    Type type() const { return m_type; }

private:
    Type m_type { Type::Invalid };
};

class WindowEvent final : public Event {
public:
    enum class Type {
        Invalid,
        Close,
        DidResize,
    };

    WindowEvent(Type type) : Event(Event::Type::Window), m_type(type) {}

    Type window_event_type() const { return m_type; }

private:
    Type m_type { Type::Invalid };
};

class WindowStateEvent final : public Event {
public:
    WindowStateEvent(bool active) : Event(Event::Type::WindowState), m_active(active) {}

    bool active() const { return m_active; }
    void set_active(bool b) { m_active = b; }

private:
    bool m_active;
};

enum class MouseEventType {
    Down,
    Double,
    Triple,
    Move,
    Up,
};

namespace MouseButton {
    enum {
        Left = 1,
        Middle = 2,
        Right = 4,
    };
}

class MouseEvent final : public Event {
public:
    MouseEvent(MouseEventType mouse_event_type, int buttons_down, int x, int y, scroll_state scroll, mouse_button_state left,
               mouse_button_state right)
        : Event(Event::Type::Mouse)
        , m_x(x)
        , m_y(y)
        , m_buttons_down(buttons_down)
        , m_scroll(scroll)
        , m_left(left)
        , m_right(right)
        , m_mouse_event_type(mouse_event_type) {}

    int x() const { return m_x; }
    int y() const { return m_y; }
    scroll_state scroll() const { return m_scroll; }
    mouse_button_state left() const { return m_left; }
    mouse_button_state right() const { return m_right; }

    void set_x(int x) { m_x = x; }
    void set_y(int y) { m_y = y; }

    MouseEventType mouse_event_type() const { return m_mouse_event_type; }
    int buttons_down() const { return m_buttons_down; };

private:
    int m_x { 0 };
    int m_y { 0 };
    int m_buttons_down { 0 };
    scroll_state m_scroll { SCROLL_NONE };
    mouse_button_state m_left { MOUSE_NO_CHANGE };
    mouse_button_state m_right { MOUSE_NO_CHANGE };
    MouseEventType m_mouse_event_type { MouseEventType::Move };
};

class KeyEvent final : public Event {
public:
    KeyEvent(char ascii, key k, int flags) : Event(Event::Type::Key), m_ascii(ascii), m_key(k), m_flags(flags) {}

    char ascii() const { return m_ascii; }
    enum key key() const { return m_key; }
    int flags() const { return m_flags; }

    bool key_up() const { return !!(m_flags & KEY_UP); }
    bool key_down() const { return !!(m_flags & KEY_DOWN); }
    bool control_down() const { return !!(m_flags & KEY_CONTROL_ON); }
    bool shift_down() const { return !!(m_flags & KEY_SHIFT_ON); }
    bool alt_down() const { return !!(m_flags & KEY_ALT_ON); }

private:
    char m_ascii;
    enum key m_key;
    int m_flags;
};

class CallbackEvent final : public Event {
public:
    CallbackEvent(Function<void()> callback) : Event(Event::Type::Callback), m_callback(move(callback)) {}

    void invoke() { m_callback(); }

private:
    Function<void()> m_callback;
};

class TimerEvent final : public Event {
public:
    TimerEvent(int times_expired) : Event(Event::Type::Timer), m_times_expired(times_expired) {}

    int times_expired() const { return m_times_expired; }

private:
    int m_times_expired;
};

class ThemeChangeEvent final : public Event {
public:
    ThemeChangeEvent() : Event(Event::Type::ThemeChange) {}
};

}
