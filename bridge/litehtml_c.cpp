/*
 * litehtml_c.cpp - C++ side of the C bridge.
 *
 * Implements litehtml::document_container and forwards everything to the
 * lh_callbacks supplied from C. Build this together with the rest of
 * litehtml into litehtml_c.dll.
 */

#include <litehtml.h>

#include <functional>
#include <new>
#include <string>

#include "litehtml_c.h"

namespace
{
    unsigned to_cref(const litehtml::web_color& c)
    {
        return static_cast<unsigned>(c.red) | (static_cast<unsigned>(c.green) << 8) |
               (static_cast<unsigned>(c.blue) << 16);
    }

    class c_container : public litehtml::document_container
    {
      public:
        lh_callbacks cb{};
        int          viewport_w = 800;
        int          viewport_h = 600;

        litehtml::uint_ptr create_font(const litehtml::font_description& descr, const litehtml::document*,
                                       litehtml::font_metrics* fm) override
        {
            if(!cb.create_font)
            {
                return 0;
            }

            const int bold      = descr.weight >= 700 ? 1 : 0;
            const int italic    = descr.style == litehtml::font_style_italic ? 1 : 0;
            const int underline = (descr.decoration_line & litehtml::text_decoration_line_underline) ? 1 : 0;
            const int strike    = (descr.decoration_line & litehtml::text_decoration_line_line_through) ? 1 : 0;

            lh_font_metrics m = {};
            void*           f = cb.create_font(descr.family.c_str(), static_cast<int>(descr.size), bold, italic,
                                               underline, strike, &m);

            if(fm)
            {
                fm->font_size   = m.font_size ? m.font_size : static_cast<int>(descr.size);
                fm->height      = m.height;
                fm->ascent      = m.ascent;
                fm->descent     = m.descent;
                fm->x_height    = m.x_height;
                fm->ch_width    = m.ch_width;
                fm->draw_spaces = m.draw_spaces != 0;
                fm->sub_shift   = m.sub_shift;
                fm->super_shift = m.super_shift;
            }

            return reinterpret_cast<litehtml::uint_ptr>(f);
        }

        void delete_font(litehtml::uint_ptr hFont) override
        {
            if(cb.delete_font && hFont)
            {
                cb.delete_font(reinterpret_cast<void*>(hFont));
            }
        }

        litehtml::pixel_t text_width(const char* text, litehtml::uint_ptr hFont) override
        {
            if(cb.text_width)
            {
                return cb.text_width(text, reinterpret_cast<void*>(hFont));
            }
            return 0;
        }

        void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color,
                       const litehtml::position& pos) override
        {
            if(cb.draw_text)
            {
                cb.draw_text(reinterpret_cast<void*>(hdc), text, reinterpret_cast<void*>(hFont), to_cref(color),
                             static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.width),
                             static_cast<int>(pos.height));
            }
        }

        litehtml::pixel_t pt_to_px(float pt) const override
        {
            return pt * 96.0f / 72.0f;
        }

        litehtml::pixel_t get_default_font_size() const override
        {
            return 16;
        }

        const char* get_default_font_name() const override
        {
            return "Segoe UI";
        }

        void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override
        {
            if(cb.draw_solid_fill)
            {
                cb.draw_solid_fill(reinterpret_cast<void*>(hdc), static_cast<int>(marker.pos.x),
                                   static_cast<int>(marker.pos.y), static_cast<int>(marker.pos.width),
                                   static_cast<int>(marker.pos.height), to_cref(marker.color));
            }
        }

        void load_image(const char*, const char*, bool) override {}
        void get_image_size(const char*, const char*, litehtml::size& sz) override
        {
            sz.width  = 0;
            sz.height = 0;
        }
        void draw_image(litehtml::uint_ptr, const litehtml::background_layer&, const std::string&,
                        const std::string&) override
        {
        }

        void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer& layer,
                             const litehtml::web_color& color) override
        {
            if(cb.draw_solid_fill)
            {
                cb.draw_solid_fill(reinterpret_cast<void*>(hdc), static_cast<int>(layer.border_box.x),
                                   static_cast<int>(layer.border_box.y), static_cast<int>(layer.border_box.width),
                                   static_cast<int>(layer.border_box.height), to_cref(color));
            }
        }

        void draw_linear_gradient(litehtml::uint_ptr, const litehtml::background_layer&,
                                  const litehtml::background_layer::linear_gradient&) override
        {
        }
        void draw_radial_gradient(litehtml::uint_ptr, const litehtml::background_layer&,
                                  const litehtml::background_layer::radial_gradient&) override
        {
        }
        void draw_conic_gradient(litehtml::uint_ptr, const litehtml::background_layer&,
                                 const litehtml::background_layer::conic_gradient&) override
        {
        }

        void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& pos,
                          bool /*root*/) override
        {
            if(cb.draw_borders)
            {
                cb.draw_borders(reinterpret_cast<void*>(hdc), static_cast<int>(pos.x), static_cast<int>(pos.y),
                                static_cast<int>(pos.width), static_cast<int>(pos.height),
                                static_cast<int>(borders.left.width), static_cast<int>(borders.top.width),
                                static_cast<int>(borders.right.width), static_cast<int>(borders.bottom.width),
                                to_cref(borders.left.color), to_cref(borders.top.color), to_cref(borders.right.color),
                                to_cref(borders.bottom.color));
            }
        }

        void set_caption(const char* caption) override
        {
            if(cb.set_caption)
            {
                cb.set_caption(caption ? caption : "");
            }
        }

        void set_base_url(const char*) override {}
        void link(const std::shared_ptr<litehtml::document>&, const litehtml::element::ptr&) override {}

        void on_anchor_click(const char* url, const litehtml::element::ptr&) override
        {
            if(cb.on_anchor_click)
            {
                cb.on_anchor_click(url ? url : "");
            }
        }

        void on_mouse_event(const litehtml::element::ptr&, litehtml::mouse_event) override {}

        void set_cursor(const char* cursor) override
        {
            if(cb.set_cursor)
            {
                cb.set_cursor(cursor ? cursor : "");
            }
        }

        void transform_text(std::string&, litehtml::text_transform) override {}
        void import_css(std::string&, const std::string&, std::string&) override {}
        void set_clip(const litehtml::position&, const litehtml::border_radiuses&) override {}
        void del_clip() override {}

        void get_viewport(litehtml::position& viewport) const override
        {
            if(cb.get_viewport)
            {
                int x = 0, y = 0, w = 0, h = 0;
                cb.get_viewport(&x, &y, &w, &h);
                viewport.x      = x;
                viewport.y      = y;
                viewport.width  = w;
                viewport.height = h;
            } else
            {
                viewport.x      = 0;
                viewport.y      = 0;
                viewport.width  = viewport_w;
                viewport.height = viewport_h;
            }
        }

        litehtml::element::ptr create_element(const char*, const litehtml::string_map&,
                                              const std::shared_ptr<litehtml::document>&) override
        {
            return nullptr;
        }

        void get_media_features(litehtml::media_features& media) const override
        {
            if(cb.get_media_features)
            {
                lh_media_features m = {};
                cb.get_media_features(&m);
                media.type          = static_cast<litehtml::media_type>(m.type);
                media.width         = m.width;
                media.height        = m.height;
                media.device_width  = m.device_width;
                media.device_height = m.device_height;
                media.color         = m.color;
                media.color_index   = m.color_index;
                media.monochrome    = m.monochrome;
                media.resolution    = m.resolution;
            } else
            {
                media.type          = litehtml::media_type_screen;
                media.width         = viewport_w;
                media.height        = viewport_h;
                media.device_width  = viewport_w;
                media.device_height = viewport_h;
                media.color         = 8;
                media.color_index   = 256;
                media.monochrome    = 0;
                media.resolution    = 96;
            }
        }

        void get_language(std::string& language, std::string& culture) const override
        {
            language = "en";
            culture  = "";
        }
    };

    struct lh_doc
    {
        litehtml::document::ptr doc;
        c_container*            container = nullptr;
    };

    void fire_redraw(void* user, const litehtml::position& pos)
    {
        auto* h = static_cast<lh_doc*>(user);
        if(h && h->container->cb.redraw_box)
        {
            h->container->cb.redraw_box(static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.width),
                                        static_cast<int>(pos.height));
        }
    }
} // namespace

extern "C" {

void* lh_create(const char* html, const lh_callbacks* cb, int viewport_width, int viewport_height)
{
    try
    {
        auto* container         = new c_container();
        container->viewport_w   = viewport_width > 0 ? viewport_width : 800;
        container->viewport_h   = viewport_height > 0 ? viewport_height : 600;
        if(cb)
        {
            container->cb = *cb;
        }

        auto doc = litehtml::document::createFromString(litehtml::estring(html ? html : ""), container);

        auto* h      = new lh_doc();
        h->doc       = doc;
        h->container = container;
        return h;
    } catch(...)
    {
        return nullptr;
    }
}

int lh_render(void* doc, int max_width)
{
    auto* h = static_cast<lh_doc*>(doc);
    if(!h || !h->doc)
    {
        return 0;
    }
    try
    {
        return static_cast<int>(h->doc->render(max_width));
    } catch(...)
    {
        return 0;
    }
}

void lh_draw(void* doc, void* hdc, int x, int y, int clip_x, int clip_y, int clip_w, int clip_h)
{
    auto* h = static_cast<lh_doc*>(doc);
    if(!h || !h->doc)
    {
        return;
    }
    litehtml::position clip(clip_x, clip_y, clip_w, clip_h);
    try
    {
        h->doc->draw(reinterpret_cast<litehtml::uint_ptr>(hdc), x, y, &clip);
    } catch(...)
    {
    }
}

int lh_width(void* doc)
{
    auto* h = static_cast<lh_doc*>(doc);
    return h && h->doc ? static_cast<int>(h->doc->width()) : 0;
}

int lh_height(void* doc)
{
    auto* h = static_cast<lh_doc*>(doc);
    return h && h->doc ? static_cast<int>(h->doc->height()) : 0;
}

int lh_mouse_over(void* doc, int x, int y, int client_x, int client_y)
{
    auto* h = static_cast<lh_doc*>(doc);
    if(!h || !h->doc)
    {
        return 0;
    }
    try
    {
        return h->doc->on_mouse_over(x, y, client_x, client_y,
                                     [h](const litehtml::position& p) { fire_redraw(h, p); })
                   ? 1
                   : 0;
    } catch(...)
    {
        return 0;
    }
}

int lh_lbutton_down(void* doc, int x, int y, int client_x, int client_y)
{
    auto* h = static_cast<lh_doc*>(doc);
    if(!h || !h->doc)
    {
        return 0;
    }
    try
    {
        return h->doc->on_lbutton_down(x, y, client_x, client_y,
                                       [h](const litehtml::position& p) { fire_redraw(h, p); })
                   ? 1
                   : 0;
    } catch(...)
    {
        return 0;
    }
}

int lh_lbutton_up(void* doc, int x, int y, int client_x, int client_y)
{
    auto* h = static_cast<lh_doc*>(doc);
    if(!h || !h->doc)
    {
        return 0;
    }
    try
    {
        return h->doc->on_lbutton_up(x, y, client_x, client_y,
                                     [h](const litehtml::position& p) { fire_redraw(h, p); })
                   ? 1
                   : 0;
    } catch(...)
    {
        return 0;
    }
}

int lh_mouse_leave(void* doc)
{
    auto* h = static_cast<lh_doc*>(doc);
    if(!h || !h->doc)
    {
        return 0;
    }
    try
    {
        return h->doc->on_mouse_leave([h](const litehtml::position& p) { fire_redraw(h, p); }) ? 1 : 0;
    } catch(...)
    {
        return 0;
    }
}

int lh_find_box(void* doc, const char* field_id, int* px, int* py, int* pw, int* ph)
{
    auto* hd = static_cast<lh_doc*>(doc);
    if(!hd || !hd->doc || !field_id || !field_id[0])
    {
        return 0;
    }
    try
    {
        auto list = hd->doc->root()->select_all("[data-field]");
        for(auto& el : list)
        {
            if(!el) continue;
            const char* a = el->get_attr("data-field");
            if(a && std::string(a) == field_id)
            {
                litehtml::position pos = el->get_placement();
                if(px) *px = static_cast<int>(pos.x);
                if(py) *py = static_cast<int>(pos.y);
                if(pw) *pw = static_cast<int>(pos.width);
                if(ph) *ph = static_cast<int>(pos.height);
                return 1;
            }
        }
    } catch(...)
    {
    }
    return 0;
}

void lh_destroy(void* doc)
{
    auto* h = static_cast<lh_doc*>(doc);
    if(!h)
    {
        return;
    }
    h->doc.reset();
    delete h->container;
    delete h;
}

const char* lh_version(void)
{
    return "litehtml-c bridge 1.0";
}

} // extern "C"
