#pragma once

#include <stdint.h>

#include <kernel/hal/x86_64/drivers/vga.h>

class Color {
public:
    Color() : Color(0xFF, 0xFF, 0xFF) {}

    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0x00) : m_color(a << 24 | r << 16 | g << 8 | b) {}
    Color(enum vga_color color);
    Color(uint32_t value) : m_color(value) {}

    Color invert() const { return Color(255 - r(), 255 - g(), 255 - b(), a()); }

    uint32_t color() const { return m_color; }

    void set(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0x00) { m_color = a << 24 | r << 16 | g << 8 | b; }

    uint8_t a() const { return m_color & 0xFF000000U; }
    uint8_t r() const { return m_color & 0x00FF0000U; }
    uint8_t g() const { return m_color & 0x0000FF00U; }
    uint8_t b() const { return m_color & 0x000000FFU; }

    bool operator==(const Color& other) const { return this->color() == other.color(); }
    bool operator!=(const Color& other) const { return *this != other; }

private:
    uint32_t m_color;
};
