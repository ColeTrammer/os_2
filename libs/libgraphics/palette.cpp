#include <ext/json.h>
#include <graphics/palette.h>

SharedPtr<Palette> Palette::create_from_json(const String& path) {
    auto maybe_object = Ext::Json::parse_file(path);
    if (!maybe_object.has_value()) {
        return nullptr;
    }

    Vector<uint32_t> colors(ColorType::Count);

    auto& object = maybe_object.value();
#define __ENUMERATE_COLOR_TYPE(t, l, d)                   \
    {                                                     \
        auto s = object.get_as<Ext::Json::String>("" #l); \
        Color color = d;                                  \
        if (s) {                                          \
            auto maybe_color = Color::parse(s->view());   \
            if (maybe_color.has_value()) {                \
                color = maybe_color.value();              \
            }                                             \
        }                                                 \
        colors.add(color.color());                        \
    }
    ENUMERATE_COLOR_TYPES
#undef __ENUMERATE_COLOR_TYPE

    return make_shared<Palette>(move(colors));
}

SharedPtr<Palette> Palette::create_from_shared_memory(const String& path, int prot) {
    auto file = Ext::MappedFile::create_with_shared_memory(path, prot);
    if (!file || file->size() != byte_size()) {
        return nullptr;
    }
    return make_shared<Palette>(move(file));
}

void Palette::copy_from(const Palette& other) {
    memcpy(this->m_colors, other.m_colors, byte_size());
}
