# litehtml_host — using litehtml from pure C with TCC

Integrates the **litehtml** HTML/CSS engine (C++17) into a **C** application
compiled with **Tiny C Compiler**.

litehtml is written in C++17 (STL, `std::string`, `std::shared_ptr`, virtual
classes, exceptions): **TCC cannot compile it**. The solution is to separate
the languages:

```
litehtml/*.cpp + gumbo/*.c ──(MinGW-w64 g++/gcc)──► litehtml_c.dll
                                                        │  C API (extern "C")
                                   bridge/litehtml_c.h  │  + litehtml_c.def
                                                        ▼
                      demo.c / smoke.c ──(tcc.exe)──► demo.exe / smoke.exe
```

The bridge implements `litehtml::document_container` and forwards every
drawing operation to C callbacks defined by the application. Everything is
64-bit; HTML is UTF-8.

## Requirements

- MinGW-w64 x86_64 (`gcc` + `g++` 11+) — e.g. `C:\Users\pr30565\Nim_code\mingw64\bin`
- Tiny C Compiler 0.9.27 x86_64 — `C:\Program Files (x86)\tcc64\tcc.exe`

The required litehtml sources are **bundled** in `.\litehtml\`: the project is
self-contained and does not need the external repository.

## Build

```bat
build_dll.bat      &rem  builds litehtml_c.dll (+ liblitehtml_c.a)
build_app.bat      &rem  compiles demo.exe with tcc
build_app.bat run  &rem  compiles and runs
```

`build_dll.bat` compiles gumbo as C (with `gcc`) and litehtml + bridge as C++17
(with `g++`): g++ would treat the `.c` files as C++, so they must be kept
separate. It builds from `.\litehtml\` (local copy), not from the external
repository.

### Updating the litehtml sources (optional)

To re-sync the local copy from the original repository:

```bat
vendorize.bat      &rem  with ..\litehtml present, re-copies the subset
```

It copies into `.\litehtml\` only what is needed (~2.7 MB): `include\`
(70 headers), `src\*.cpp` (59) + `src\internal.h`, `src\gumbo\src\` (32 files),
plus the licenses (`LICENSE` BSD, `src\gumbo\LICENSE` Apache).

## Running

```bat
demo.exe            &rem  opens test.html in the current folder
demo.exe page.html
```

Mouse: hover and click on links (shows the URL). Wheel / arrow keys: scroll.
Resizing the window makes the layout fit the width.

Headless test (no window) that checks create + render + measurement:

```bat
"C:\Program Files (x86)\tcc64\tcc.exe" smoke.c bridge\litehtml_c.def -o smoke.exe -I. -Wall
smoke.exe
```

## Files

| File | Role |
|------|------|
| `bridge/litehtml_c.h` | public C API: `lh_callbacks` struct + `lh_*` functions |
| `bridge/litehtml_c.cpp` | C++ wrapper implementing `document_container` |
| `bridge/litehtml_c.def` | EXPORTS list for importing from TCC |
| `build_dll.bat` | builds `litehtml_c.dll` with MinGW (from `.\litehtml`) |
| `vendorize.bat` | re-copies the litehtml subset from `..\litehtml` (one-time) |
| `demo.c` | Win32/GDI app: window, GDI callbacks, mouse, scroll |
| `build_app.bat` | compiles `demo.c` with tcc |
| `smoke.c` | headless create/render/measure test |
| `test.html` | sample page (UTF-8) |
| `litehtml/` | vendored litehtml + gumbo sources (self-containment) |

## C API in short

```c
typedef struct lh_callbacks {
    void* (*create_font)(const char* family, int size, int bold, int italic,
                         int underline, int strike, lh_font_metrics* fm);
    void  (*delete_font)(void* font);
    int   (*text_width)(const char* text, void* font);
    void  (*draw_text)(void* hdc, const char* text, void* font, unsigned rgb,
                       int x, int y, int w, int h);
    void  (*draw_solid_fill)(void* hdc, int x, int y, int w, int h, unsigned rgb);
    void  (*draw_borders)(void* hdc, int x, int y, int w, int h,
                          int lw, int tw, int rw, int bw,
                          unsigned lc, unsigned tc, unsigned rc, unsigned bc);
    void  (*set_caption)(const char* caption);
    void  (*on_anchor_click)(const char* url);
    void  (*set_cursor)(const char* cursor);
    void  (*get_viewport)(int* x, int* y, int* w, int* h);
    void  (*get_media_features)(lh_media_features* media);
    void  (*redraw_box)(int x, int y, int w, int h);
} lh_callbacks;

void* lh_create(const char* html_utf8, const lh_callbacks* cb, int vw, int vh);
int   lh_render(void* doc, int max_width);          /* returns best_width */
void  lh_draw(void* doc, void* hdc, int x, int y, int cx, int cy, int cw, int ch);
int   lh_width(void* doc);   int   lh_height(void* doc);
int   lh_mouse_over(void* doc, int x, int y, int cx, int cy);
int   lh_lbutton_down(void* doc, int x, int y, int cx, int cy);
int   lh_lbutton_up(void* doc, int x, int y, int cx, int cy);
int   lh_mouse_leave(void* doc);
void  lh_destroy(void* doc);
```

All callbacks are optional. `hdc` is the opaque value passed to `lh_draw`
(an HDC); `font` is the opaque value returned by `create_font`. Colors are
`COLORREF` (`r | g<<8 | b<<16`), ready for `SetTextColor`/`CreateSolidBrush`.
Coordinates are in pixels, document-relative (use `-scroll_x/-scroll_y` as the
offset in `lh_draw`).

## Notes and limitations (v1)

- Rendering: text, blocks, background colors, borders, list markers.
- Stubs (no-op): images, linear/radial/conic gradients, CSS `@import`,
  `text-transform`, advanced `position:fixed`.
- **Unicode**: litehtml delivers UTF-8; the GDI callbacks must convert to
  UTF-16 and use `DrawTextW`/`GetTextExtentPoint32W` (see `u8_to_w` in
  `demo.c`). With `DrawTextA` accented characters appear corrupted.
- **DLL/C boundary**: do not `free` memory allocated by the bridge (use the
  opaque handles). The bridge catches C++ exceptions and does not propagate
  them.
- Static C++ runtime (`-static-libstdc++ -static-libgcc`): the DLL does not
  depend on MinGW runtime DLLs.

## License

The bridge, demo and build scripts are provided as-is. Bundled third-party
sources keep their own licenses: litehtml (New BSD, see `litehtml/LICENSE`)
and gumbo-parser (Apache-2.0, see `litehtml/src/gumbo/LICENSE`).
