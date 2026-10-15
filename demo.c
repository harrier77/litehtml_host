/*
 * demo.c - minimal Win32/GDI host for litehtml_c.dll, built with tcc.
 *
 * Loads an UTF-8 HTML file, renders it with litehtml and paints it with GDI.
 * Supports mouse hover/clicks on links and wheel/keyboard scrolling.
 *
 * Build:  build_app.bat   (tcc demo.c litehtml_c.def ...)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge/litehtml_c.h"

static void*   g_doc      = NULL;
static HDC     g_tmp_dc   = NULL;
static int     g_scroll_x = 0;
static int     g_scroll_y = 0;
static int     g_client_w = 0;
static int     g_client_h = 0;

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

static char* read_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    char* buf;
    long  size;

    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0)
    {
        fclose(f);
        return NULL;
    }

    buf = (char*)malloc((size_t)size + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size)
    {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[size] = 0;
    fclose(f);
    return buf;
}

/* ------------------------------------------------------------------ */
/* litehtml callbacks                                                 */
/* ------------------------------------------------------------------ */

/* litehtml gives us UTF-8; GDI wide functions need UTF-16.
 * Self-contained decoder (TCC's headers do not expose MultiByteToWideChar). */
static WCHAR* u8_to_w(const char* s)
{
    const unsigned char* p = (const unsigned char*)(s ? s : "");
    size_t               len = strlen((const char*)p);
    WCHAR*               out = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
    size_t               i = 0, o = 0;

    if (!out)
        return NULL;

    while (i < len)
    {
        unsigned c = p[i++];
        unsigned cp;

        if (c < 0x80)
        {
            cp = c;
        }
        else if ((c & 0xE0) == 0xC0 && i < len)
        {
            cp = ((c & 0x1F) << 6) | (p[i++] & 0x3F);
        }
        else if ((c & 0xF0) == 0xE0 && i + 1 < len)
        {
            cp = ((c & 0x0F) << 12) | ((p[i] & 0x3F) << 6) | (p[i + 1] & 0x3F);
            i += 2;
        }
        else if ((c & 0xF8) == 0xF0 && i + 2 < len)
        {
            cp = ((c & 0x07) << 18) | ((p[i] & 0x3F) << 12) | ((p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F);
            i += 3;
        }
        else
        {
            cp = '?';
        }

        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            out[o++] = (WCHAR)(0xD800 + (cp >> 10));
            out[o++] = (WCHAR)(0xDC00 + (cp & 0x3FF));
        }
        else
        {
            out[o++] = (WCHAR)cp;
        }
    }

    out[o] = 0;
    return out;
}

static void* cb_create_font(const char* family, int size, int bold, int italic, int underline, int strike,
                            lh_font_metrics* fm)
{
    LOGFONTW lf;
    HFONT    font;
    TEXTMETRICW tm;
    WCHAR*   wfam = u8_to_w(family && family[0] ? family : "Segoe UI");

    ZeroMemory(&lf, sizeof(lf));
    if (wfam)
    {
        lstrcpynW(lf.lfFaceName, wfam, LF_FACESIZE);
        free(wfam);
    }
    lf.lfHeight    = -size;
    lf.lfWeight    = bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic    = (BYTE)(italic ? TRUE : FALSE);
    lf.lfUnderline = (BYTE)(underline ? TRUE : FALSE);
    lf.lfStrikeOut = (BYTE)(strike ? TRUE : FALSE);
    lf.lfCharSet   = DEFAULT_CHARSET;
    lf.lfQuality   = DEFAULT_QUALITY;
    font           = CreateFontIndirectW(&lf);

    if (fm)
    {
        HGDIOBJ old = SelectObject(g_tmp_dc, font);
        ZeroMemory(&tm, sizeof(tm));
        GetTextMetricsW(g_tmp_dc, &tm);
        SelectObject(g_tmp_dc, old);

        fm->font_size   = size;
        fm->height      = tm.tmHeight;
        fm->ascent      = tm.tmAscent;
        fm->descent     = tm.tmDescent;
        fm->x_height    = tm.tmHeight / 2;
        fm->ch_width    = tm.tmAveCharWidth;
        fm->draw_spaces = (italic || underline || strike) ? 1 : 0;
        fm->sub_shift   = size / 5;
        fm->super_shift = size / 3;
    }
    return font;
}

static void cb_delete_font(void* font)
{
    if (font)
        DeleteObject((HFONT)font);
}

static int cb_text_width(const char* text, void* font)
{
    SIZE    sz;
    HGDIOBJ old;
    WCHAR*  wtext = u8_to_w(text);

    ZeroMemory(&sz, sizeof(sz));
    old = SelectObject(g_tmp_dc, (HFONT)font);
    if (wtext)
    {
        GetTextExtentPoint32W(g_tmp_dc, wtext, lstrlenW(wtext), &sz);
        free(wtext);
    }
    SelectObject(g_tmp_dc, old);
    return sz.cx;
}

static void cb_draw_text(void* hdc, const char* text, void* font, unsigned rgb, int x, int y, int w, int h)
{
    HDC     dc = (HDC)hdc;
    RECT    rc;
    HGDIOBJ oldfont;
    WCHAR*  wtext = u8_to_w(text);

    rc.left   = x;
    rc.top    = y;
    rc.right  = x + w;
    rc.bottom = y + h;

    oldfont = SelectObject(dc, (HFONT)font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, (COLORREF)rgb);
    if (wtext)
    {
        DrawTextW(dc, wtext, -1, &rc, DT_SINGLELINE | DT_NOPREFIX | DT_BOTTOM | DT_NOCLIP);
        free(wtext);
    }
    SelectObject(dc, oldfont);
}

static void cb_draw_solid_fill(void* hdc, int x, int y, int w, int h, unsigned rgb)
{
    RECT        rc;
    HBRUSH      br;
    HGDIOBJ     old;

    if (w <= 0 || h <= 0)
        return;
    rc.left   = x;
    rc.top    = y;
    rc.right  = x + w;
    rc.bottom = y + h;

    br  = CreateSolidBrush((COLORREF)rgb);
    old = SelectObject((HDC)hdc, br);
    FillRect((HDC)hdc, &rc, br);
    SelectObject((HDC)hdc, old);
    DeleteObject(br);
}

static void cb_draw_borders(void* hdc, int x, int y, int w, int h, int lw, int tw, int rw, int bw, unsigned lc,
                            unsigned tc, unsigned rc, unsigned bc)
{
    HDC     dc = (HDC)hdc;
    HBRUSH  br;
    HGDIOBJ old;
    RECT    r;

    if (tw > 0 && tc != (unsigned)-1)
    {
        br  = CreateSolidBrush((COLORREF)tc);
        old = SelectObject(dc, br);
        r.left = x; r.top = y; r.right = x + w; r.bottom = y + tw;
        FillRect(dc, &r, br);
        SelectObject(dc, old);
        DeleteObject(br);
    }
    if (bw > 0)
    {
        br  = CreateSolidBrush((COLORREF)bc);
        old = SelectObject(dc, br);
        r.left = x; r.top = y + h - bw; r.right = x + w; r.bottom = y + h;
        FillRect(dc, &r, br);
        SelectObject(dc, old);
        DeleteObject(br);
    }
    if (lw > 0)
    {
        br  = CreateSolidBrush((COLORREF)lc);
        old = SelectObject(dc, br);
        r.left = x; r.top = y; r.right = x + lw; r.bottom = y + h;
        FillRect(dc, &r, br);
        SelectObject(dc, old);
        DeleteObject(br);
    }
    if (rw > 0)
    {
        br  = CreateSolidBrush((COLORREF)rc);
        old = SelectObject(dc, br);
        r.left = x + w - rw; r.top = y; r.right = x + w; r.bottom = y + h;
        FillRect(dc, &r, br);
        SelectObject(dc, old);
        DeleteObject(br);
    }
    (void)bc;
}

static void cb_set_caption(const char* caption)
{
    HWND   hwnd = GetActiveWindow();
    WCHAR* w    = u8_to_w(caption ? caption : "litehtml");

    if (hwnd && w)
        SetWindowTextW(hwnd, w);
    free(w);
}

static void cb_on_anchor_click(const char* url)
{
    WCHAR* w = u8_to_w(url ? url : "");
    if (w)
    {
        MessageBoxW(NULL, w, L"Link", MB_OK | MB_ICONINFORMATION);
        free(w);
    }
}

static void cb_set_cursor(const char* cursor)
{
    (void)cursor;
}

static void cb_get_viewport(int* x, int* y, int* w, int* h)
{
    if (x) *x = 0;
    if (y) *y = 0;
    if (w) *w = g_client_w;
    if (h) *h = g_client_h;
}

static void cb_get_media_features(lh_media_features* m)
{
    if (!m)
        return;
    m->type          = 3; /* media_type_screen */
    m->width         = g_client_w;
    m->height        = g_client_h;
    m->device_width  = g_client_w;
    m->device_height = g_client_h;
    m->color         = 8;
    m->color_index   = 256;
    m->monochrome    = 0;
    m->resolution    = GetDeviceCaps(g_tmp_dc, LOGPIXELSX);
}

static void cb_redraw_box(int x, int y, int w, int h)
{
    HWND hwnd = GetActiveWindow();
    RECT r;
    if (!hwnd)
        return;
    r.left   = x - g_scroll_x;
    r.top    = y - g_scroll_y;
    r.right  = r.left + w;
    r.bottom = r.top + h;
    InvalidateRect(hwnd, &r, FALSE);
}

/* ------------------------------------------------------------------ */
/* window                                                             */
/* ------------------------------------------------------------------ */

static void layout_and_render(HWND hwnd)
{
    if (!g_doc)
        return;
    lh_render(g_doc, g_client_w);
    InvalidateRect(hwnd, NULL, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SIZE:
        g_client_w = LOWORD(lp);
        g_client_h = HIWORD(lp);
        layout_and_render(hwnd);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(hwnd, &ps);
            if (g_doc)
            {
                lh_draw(g_doc, hdc, -g_scroll_x, -g_scroll_y, g_scroll_x, g_scroll_y, g_client_w, g_client_h);
            }
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (g_doc)
        {
            int x = (short)LOWORD(lp) + g_scroll_x;
            int y = (short)HIWORD(lp) + g_scroll_y;
            lh_mouse_over(g_doc, x, y, (short)LOWORD(lp), (short)HIWORD(lp));
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (g_doc)
        {
            int x = (short)LOWORD(lp) + g_scroll_x;
            int y = (short)HIWORD(lp) + g_scroll_y;
            lh_lbutton_down(g_doc, x, y, (short)LOWORD(lp), (short)HIWORD(lp));
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_doc)
        {
            int x = (short)LOWORD(lp) + g_scroll_x;
            int y = (short)HIWORD(lp) + g_scroll_y;
            lh_lbutton_up(g_doc, x, y, (short)LOWORD(lp), (short)HIWORD(lp));
        }
        return 0;

    case WM_MOUSEWHEEL:
        {
            int delta = (short)HIWORD(wp);
            g_scroll_y -= delta / WHEEL_DELTA * 48;
            if (g_scroll_y < 0)
                g_scroll_y = 0;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_KEYDOWN:
        if (msg == WM_KEYDOWN)
        {
            if (wp == VK_DOWN)  g_scroll_y += 32;
            if (wp == VK_UP)    g_scroll_y -= 32;
            if (wp == VK_RIGHT) g_scroll_x += 32;
            if (wp == VK_LEFT)  g_scroll_x -= 32;
            if (g_scroll_y < 0) g_scroll_y = 0;
            if (g_scroll_x < 0) g_scroll_x = 0;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        break;

    case WM_DESTROY:
        if (g_doc)
        {
            lh_destroy(g_doc);
            g_doc = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int show)
{
    WNDCLASSA wc;
    HWND      hwnd;
    MSG       msg;
    char*     html;
    char      path[MAX_PATH];
    lh_callbacks cb;
    const char* arg = cmdline;

    (void)hPrev;

    if (arg && arg[0])
    {
        lstrcpynA(path, arg, MAX_PATH);
    }
    else
    {
        lstrcpynA(path, "test.html", MAX_PATH);
    }

    html = read_file(path);
    if (!html)
    {
        char err[MAX_PATH + 64];
        wsprintfA(err, "Impossibile leggere il file HTML:\n%s", path);
        MessageBoxA(NULL, err, "litehtml demo", MB_OK | MB_ICONERROR);
        return 1;
    }

    g_tmp_dc = GetDC(NULL);

    ZeroMemory(&cb, sizeof(cb));
    cb.create_font       = cb_create_font;
    cb.delete_font       = cb_delete_font;
    cb.text_width        = cb_text_width;
    cb.draw_text         = cb_draw_text;
    cb.draw_solid_fill   = cb_draw_solid_fill;
    cb.draw_borders      = cb_draw_borders;
    cb.set_caption       = cb_set_caption;
    cb.on_anchor_click   = cb_on_anchor_click;
    cb.set_cursor        = cb_set_cursor;
    cb.get_viewport      = cb_get_viewport;
    cb.get_media_features = cb_get_media_features;
    cb.redraw_box        = cb_redraw_box;

    g_client_w = 800;
    g_client_h = 600;

    g_doc = lh_create(html, &cb, g_client_w, g_client_h);
    free(html);
    if (!g_doc)
    {
        MessageBoxA(NULL, "lh_create() ha restituito NULL.", "litehtml demo", MB_OK | MB_ICONERROR);
        return 1;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "litehtml_host_demo";
    RegisterClassA(&wc);

    hwnd = CreateWindowA("litehtml_host_demo", "litehtml demo", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                         800, 600, NULL, NULL, hInst, NULL);
    if (!hwnd)
    {
        lh_destroy(g_doc);
        g_doc = NULL;
        return 1;
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (g_tmp_dc)
    {
        ReleaseDC(NULL, g_tmp_dc);
        g_tmp_dc = NULL;
    }
    return (int)msg.wParam;
}
