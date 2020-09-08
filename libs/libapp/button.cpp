#include <app/button.h>
#include <app/event.h>
#include <app/window.h>
#include <graphics/renderer.h>

namespace App {

void Button::render() {
    Renderer renderer(*window()->pixels());

    renderer.fill_rect(rect(), ColorValue::Black);
    renderer.draw_rect(rect(), ColorValue::White);
    renderer.render_text(label(), rect().adjusted(-2), ColorValue::White, TextAlign::CenterLeft, font());
}

void Button::on_mouse_event(MouseEvent& mouse_event) {
    if (mouse_event.left() == MOUSE_DOWN) {
        m_did_mousedown = true;
        return;
    }

    if (m_did_mousedown && mouse_event.left() == MOUSE_UP) {
        m_did_mousedown = false;
        if (on_click) {
            on_click();
        }
    }
}

}
