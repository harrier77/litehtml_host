# How it works — the DLL vs. `demo.c`

You understood it half right: **`demo.c` paints the pixels, but it does not
decide what to paint.** That decision belongs entirely to the DLL. Here is the
split in plain words.

## The analogy

- The **litehtml DLL** is the **architect**: it reads HTML/CSS and produces a
  *blueprint* — where everything goes, how big it is, what color, which font.
  It cannot draw anything.
- **`demo.c`** is the **builder/painter**: it takes the blueprint and actually
  "builds" it on screen with GDI.

The architect never touches the brush; the builder never designs the floor plan.

## What kind of object does the DLL return?

`lh_create` (`bridge/litehtml_c.cpp:281`) **does not return an image or a pixel
buffer**. It returns an **opaque handle**, i.e. a `void*`.

Inside that handle there is a small struct (`lh_doc`,
`bridge/litehtml_c.cpp:262`) holding:

```cpp
litehtml::document::ptr doc;   // the "blueprint"
c_container*            container;
```

`litehtml::document` is an **in-memory model**: a tree of boxes (elements),
each with style, position, width, height and the text to show. It is **data,
not a drawing**. To C it is "opaque" because C does not know C++ types: you
just hand it back to the other `lh_*` functions.

## The two key functions

**1. `lh_render` (`:314`)** — computes the layout (CSS parsing, units, wrapping,
positions) and returns the *best width*. After this call the `document` holds
all final coordinates.

**2. `lh_draw` (`:331`)** — walks the blueprint and, for every box, **calls back
into `demo.c`**:

- `draw_text` → "write *this* word, *here*, with *this* font, *this* color"
- `draw_solid_fill` → "fill *this* rectangle with *this* color"
- `draw_borders` → "draw the borders *like this*"

So `demo.c` performs the drawing, but **text, position, size, color and style
are handed to it by the DLL**. The functions in `demo.c:189–232` are the
"brushes"; the *what/when/where* comes from the DLL.

## The dialogue is two-way

There is a back-and-forth, not a one-way output:

```
demo.c ──lh_create(html, callbacks)──► DLL   (registers the brushes)
DLL    ──create_font/text_width─────► demo.c ("how wide is this text?")
demo.c ──answer in pixels───────────► DLL     (uses the measurements for layout)
demo.c ──lh_draw(hdc)───────────────► DLL
DLL    ──draw_text/draw_fill────────► demo.c  (does the actual painting)
```

The DLL needs the text measurements (`text_width`, font metrics) that only
`demo.c` can provide through GDI; and `demo.c` needs the DLL to tell it where
to put things. Neither is self-sufficient.

## Why the DLL exists (summary)

All the *intellectual* work — parsing HTML, applying CSS, computing layout — is
C++ and **TCC cannot do it**. The DLL does that work and exposes it through a
handful of C functions. `demo.c` stays pure C and does only what TCC does well:
**talk to Windows/GDI**.

In one sentence: **the DLL produces the "map", `demo.c` "paints" it** — and the
map is not an image, it is a data structure of positions and instructions.
