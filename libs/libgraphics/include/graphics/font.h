#pragma once

#include <fcntl.h>
#include <liim/bitset.h>
#include <liim/hash_map.h>
#include <liim/string.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

class FontImpl {
public:
    FontImpl(const char* path);
    FontImpl(int num_chars);
    ~FontImpl();

    const Bitset<uint8_t>* get_for_character(int c) const;
    bool save_to_file(const String& path) const;

private:
    HashMap<int, Bitset<uint8_t>> m_font_map;
};

class Font {
public:
    static const Font& default_font() {
        static Font* s_default;
        if (!s_default) {
            s_default = new Font("/usr/share/font.psf");
        }
        return *s_default;
    }

    static const Font& bold_font() {
        static Font* s_bold;
        if (!s_bold) {
            s_bold = new Font("/usr/share/bold.psf");
        }
        return *s_bold;
    }

    static Font create_blank();

    Font(const char* path) : m_impl(make_shared<FontImpl>(path)) {}

    const Bitset<uint8_t>* get_for_character(int c) const { return m_impl->get_for_character(c); }
    bool save_to_file(const String& path) const { return m_impl->save_to_file(path); }

private:
    Font(int num_chars) : m_impl(make_shared<FontImpl>(num_chars)) {}

    SharedPtr<FontImpl> m_impl;
};
