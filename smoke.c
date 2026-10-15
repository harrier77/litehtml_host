/*
 * smoke.c - headless test for litehtml_c.dll (no window, no GDI painting).
 * Verifies create + render + measure. Built with tcc.
 *
 *   tcc.exe smoke.c bridge\litehtml_c.def -o smoke.exe -I. -Wall
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge/litehtml_c.h"

static void* cf(const char* family, int size, int bold, int italic, int underline, int strike, lh_font_metrics* m)
{
    (void)family; (void)bold; (void)italic; (void)underline; (void)strike;
    memset(m, 0, sizeof(*m));
    m->font_size   = size;
    m->height      = size + 3;
    m->ascent      = size;
    m->descent     = 3;
    m->x_height    = size / 2;
    m->ch_width    = size / 2;
    m->draw_spaces = 1;
    m->sub_shift   = size / 5;
    m->super_shift = size / 3;
    return (void*)1;
}

static int tw(const char* text, void* font)
{
    (void)font;
    return (int)strlen(text) * 8;
}

static void dt(void* hdc, const char* t, void* f, unsigned c, int x, int y, int w, int h)
{
    (void)hdc; (void)t; (void)f; (void)c; (void)x; (void)y; (void)w; (void)h;
}

static void sf(void* hdc, int x, int y, int w, int h, unsigned c)
{
    (void)hdc; (void)x; (void)y; (void)w; (void)h; (void)c;
}

static void db(void* hdc, int x, int y, int w, int h, int lw, int tw2, int rw, int bw, unsigned lc, unsigned tc,
               unsigned rc, unsigned bc)
{
    (void)hdc; (void)x; (void)y; (void)w; (void)h; (void)lw; (void)tw2; (void)rw; (void)bw; (void)lc; (void)tc;
    (void)rc; (void)bc;
}

static void gv(int* x, int* y, int* w, int* h)
{
    *x = 0; *y = 0; *w = 800; *h = 600;
}

static void gm(lh_media_features* m)
{
    memset(m, 0, sizeof(*m));
    m->type = 3; m->width = 800; m->height = 600;
    m->device_width = 800; m->device_height = 600;
    m->color = 8; m->color_index = 256; m->resolution = 96;
}

int main(int argc, char** argv)
{
    FILE*        fp;
    long         sz;
    char*        buf;
    void*        doc;
    lh_callbacks cb;
    int          w, h;
    const char*  path = argc > 1 ? argv[1] : "test.html";

    fp = fopen(path, "rb");
    if(!fp)
    {
        printf("smoke: cannot open %s\n", path);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (char*)malloc((size_t)sz + 1);
    if(fread(buf, 1, (size_t)sz, fp) != (size_t)sz)
    {
        fclose(fp);
        free(buf);
        return 1;
    }
    buf[sz] = 0;
    fclose(fp);

    memset(&cb, 0, sizeof(cb));
    cb.create_font        = cf;
    cb.text_width         = tw;
    cb.draw_text          = dt;
    cb.draw_solid_fill    = sf;
    cb.draw_borders       = db;
    cb.get_viewport       = gv;
    cb.get_media_features = gm;

    doc = lh_create(buf, &cb, 800, 600);
    printf("bridge: %s\n", lh_version());
    printf("doc:    %p\n", doc);
    if(!doc)
    {
        free(buf);
        return 2;
    }

    printf("best:   %d\n", lh_render(doc, 800));
    w = lh_width(doc);
    h = lh_height(doc);
    printf("size:   %dx%d\n", w, h);
    lh_destroy(doc);
    free(buf);
    printf("ok\n");
    return (w > 0 && h > 0) ? 0 : 3;
}
