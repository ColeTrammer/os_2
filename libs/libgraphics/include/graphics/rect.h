#pragma once

#include <graphics/point.h>

class Rect {
public:
    Rect(int x, int y, int width, int height)
        : m_x(x)
        , m_y(y)
        , m_width(width)
        , m_height(height)
    {
    }

    int x() const { return m_x; }
    int y() const { return m_y; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    void set_x(int x) { m_x = x; }
    void set_y(int y) { m_y = y; }
    void set_width(int width) { m_width = width; }
    void set_height(int height) { m_height = height; }

    Point center() const
    {
        return Point(x() + width() / 2, y() + height() / 2);
    }

    ~Rect()
    {
    }

private:
    int m_x;
    int m_y;
    int m_width;
    int m_height;
};