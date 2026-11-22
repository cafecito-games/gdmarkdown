# Architecture

`gdmarkdown` v2 is a native C++ GDExtension (`godot-cpp` + vendored `md4c`
v0.6.0) that renders Markdown directly onto Godot's `Control`, `TextParagraph`,
`TextServer`, and `StyleBox` pipeline.

```text
Markdown UTF-8 string
    │
    ├─ 1. md4c callback parser ────→ Pure C++ Document IR (Block / InlineNode)
    │
    ├─ 2. Document post-pass ──────→ Callout promotion, image-block promotion,
    │                                depth-first task indexing
    │
    ├─ 3. Layout engine ───────────→ Two-pass table column sizing,
    │                                TextParagraph / TextServer shaping,
    │                                inline objects, decoration rects & lines,
    │                                flat hit-target lists, cumulative char ranges
    │
    └─ 4. MarkdownView (_draw) ────→ Vertical clip culling, StyleBox & panel
                                     painting, per-glyph span colors & typewriter
                                     cutoff, child-RID clipped code scrolling
```

The front end (parser, IR, and table sizing math) has zero Godot header
dependencies so core parsing and layout invariants can be tested in milliseconds
with a standard C++17 compiler (`task test:native`).

---

## 1. Pure C++ front end (`native/src/parser/`)

### `md4c` parsing

`native/src/parser/markdown_parser.cpp` feeds UTF-8 input to vendored `md4c`
v0.6.0 with:

- `MD_FLAG_TABLES`
- `MD_FLAG_STRIKETHROUGH`
- `MD_FLAG_TASKLISTS`
- `MD_FLAG_PERMISSIVEAUTOLINKS`
- `MD_FLAG_COLLAPSEWHITESPACE` (automatically suspended inside code spans and
  code blocks by `md4c`)

Raw HTML parsing stays enabled so `md4c` emits `MD_TEXT_HTML` tokens:
case-insensitive `<br>`, `<br/>`, and `<br />` convert to
`InlineKind::HARD_BREAK`, while all other `MD_TEXT_HTML` tokens and
`MD_BLOCK_HTML` blocks are safely discarded.

### Document IR (`native/src/parser/document.hpp`)

The parser produces a value-type `gdmarkdown::Document` containing:

- `blocks`: recursive tree of `Block` nodes (`PARAGRAPH`, `HEADING`,
  `BLOCK_QUOTE`, `CALLOUT`, `LIST`, `CODE_BLOCK`, `TABLE`, `THEMATIC_BREAK`,
  `IMAGE_BLOCK`).
- `initial_task_states`: initial checked state for every task item in
  depth-first document order.

Inline content is stored as flat `InlineNode` runs (`TEXT`, `IMAGE`,
`SOFT_BREAK`, `HARD_BREAK`) carrying bitmask `InlineStyleFlag` bits (`BOLD`,
`ITALIC`, `STRIKETHROUGH`, `CODE`) and an optional `link_url`.

### Post-pass: callout promotion, block images, and task indexing

After `md4c` finishes building the raw block tree, a single recursive post-pass
normalizes three structures:

1. **Callout promotion.** Every `BLOCK_QUOTE` whose first child is a `PARAGRAPH`
   starting with plain text matching `[!KIND](+|-)? Title` (scanned directly
   with a hand-written zero-regex scanner up to the first line break or end of
   paragraph) is promoted to a `CALLOUT` block:
   - `callout_kind` is uppercased (e.g., `"NOTE"`, `"QUEST"`).
   - Optional `+`/`-` flags are preserved on `callout_collapsible` and
     `callout_initially_collapsed` as additive IR fields.
   - Optional custom title text is trimmed into `callout_custom_title`.
   - The directive header and its trailing break are stripped from the leading
     paragraph (removing the paragraph if nothing remains), and the post-pass
     recurses into child blocks so nested callouts work automatically.
2. **Block image promotion.** Any paragraph containing only a single `IMAGE`
   inline (optionally surrounded by whitespace) promotes to an `IMAGE_BLOCK` so
   standalone banners frame and scale independently of inline icons.
3. **Task indexing.** Every task `ListItem` receives a sequential 0-based
   `task_index` in depth-first document order, recording its initial checked
   boolean in `Document::initial_task_states`.

`MarkdownView` copies `initial_task_states` into its own `task_states` vector
whenever `markdown` is assigned or cleared. Toggling a checkbox mutates
`task_states` and queues a redraw without re-parsing Markdown, while replacing
`markdown` completely resets `task_states` so prior document state can never
leak.

---

## 2. Two-pass table auto-sizing (`native/src/layout/table_sizing.*`)

Markdown pipe tables are sized in two passes so columns fit narrow containers
gracefully without wasting space in wide ones:

1. **Measurement pass.** Each cell's content is measured unwrapped to obtain
   `maximum_width` and broken at word boundaries (with a practical floor) to
   obtain `minimum_width`. Columns aggregate the max of both bounds across all
   header and body rows.
2. **Allocation pass (`compute_table_column_widths`).**
   - When `sum(maximum_width) <= available_width`, columns use their natural
     widths (optionally expanding when filling available width).
   - When constrained, each column receives `minimum_width` plus a proportional
     share of remaining space weighted by `maximum_width - minimum_width`. If
     even `sum(minimum_width) > available_width`, columns scale down
     proportionally so total table width never exceeds `available_width`.
   - Horizontal cell padding is clamped per column (`effective_padding_horizontal`)
     so every column's inner `text_width` stays strictly positive (`>= 1.0f`),
     preventing NaN heights or unconstrained wrapping inside ultra-narrow
     containers.

Each cell's `TextParagraph` is then shaped at `text_width` with its column
`HorizontalAlignment` (`LEFT`, `CENTER`, `RIGHT`) and
`BREAK_MANDATORY | BREAK_WORD_BOUND | BREAK_GRAPHEME_BOUND`. Row height is the
tallest shaped cell plus vertical cell padding.

---

## 3. Text shaping, decorations, and images (`native/src/layout/layout_engine.*`)

`build_layout()` walks `Document` top-to-bottom at a target `available_width`,
emitting a flat `LayoutDocument` of `LaidOutParagraph` records, ordered
`PaintPrimitive` draw commands, and hit-target lists:

- **Font selection.** Inline runs resolve `regular_font`, `bold_font`,
  `italic_font`, `bold_italic_font`, or `monospace_font` from `MarkdownTheme`,
  falling back cleanly to the `Control`'s default theme font when any variant is
  unset.
- **Inline code pills.** Zero-draw spacer objects (`TextParagraph::add_object`)
  bookend each inline code span so adjacent prose never crowds the pill border.
  Logical character ranges are queried via
  `TextServer::shaped_text_get_selection()` per shaped line to materialize exact
  pill rectangles painted behind the glyphs.
- **Link underlines and strikethroughs.** Span ranges are converted into
  per-line horizontal segments positioned from line ascent/descent metrics, and
  link fragments simultaneously register `LinkHitBox` rectangles so hit-testing
  never false-triggers on trailing line whitespace.
- **Inline and block images.** URIs resolve once through `image_resolver` (if
  valid) and otherwise through `ResourceLoader` for `res://` paths only, cached
  per `MarkdownView` in `image_cache` (invalidated via `clear_image_cache()`).
  - Inline images insert into `TextParagraph` via `add_object()` height-scaled
    to `font_size * inline_image_scale` preserving aspect ratio, then read back
    their line-relative placement rects via `get_line_object_rect()`.
  - Block images scale to fit `min(available_width, texture_width)` and
    `max_image_height` preserving aspect ratio.
  - Any unresolved image degrades cleanly to its `alt` text.
- **Cumulative character accounting.** Every `LaidOutParagraph` records its
  document-ordered `[document_char_start, document_char_end)` range, and every
  `PaintPrimitive` stores its `document_char_threshold`, enabling constant-cost
  typewriter visibility checks during drawing.

---

## 4. Drawing, typewriter reveal, and code scrolling (`native/src/nodes/markdown_view.*`)

### Per-glyph span rendering & typewriter reveal

Godot's `TextParagraph::draw()` paints an entire paragraph in one color. When a
paragraph contains rich spans (links, hover highlight, inline code, muted text)
or intersects an active typewriter cutoff (`0 <= visible_characters <
total_character_count`), `draw_laid_out_paragraph()` iterates shaped lines via
`TextServer::shaped_text_get_glyphs()` and paints each glyph with
`TextServer::font_draw_glyph()` using the color of the `ColorSpan` covering
`glyph.start`.

For typewriter reveal:

- Items with `document_char_end <= visible_characters` draw normally (using the
  fast `TextParagraph::draw()` path when single-colored).
- Items with `document_char_start >= visible_characters` (and trailing panels /
  rules past the frontier) are skipped entirely.
- Only the single frontier paragraph masks glyphs, inline objects, pills, and
  decorations past `local_cutoff = visible_characters - document_char_start`
  while preserving each glyph's advance.

Words never jump lines mid-word as characters appear, and advancing
`visible_characters` or `visible_ratio` triggers zero text re-shaping.

### Code blocks and child-RID clipping

- When `code_block_wrap == true`, code blocks shape with normal word/grapheme
  wrapping at the inner panel width.
- When `code_block_wrap == false`, code blocks shape with `BREAK_MANDATORY`
  only. If the natural `content_width` exceeds `inner_rectangle.size.x`,
  `MarkdownView` allocates a lightweight child `RID` canvas item via
  `RenderingServer` with both `canvas_item_set_custom_rect(rid, true, ...)` and
  `canvas_item_set_clip(rid, true)`, paints the code paragraph offset by
  `-scroll_x`, renders a bottom scrollbar thumb, and handles horizontal wheel
  (`MOUSE_BUTTON_WHEEL_LEFT` / `RIGHT`, `Shift` + wheel) and horizontal pointer
  drag inside the block. All child `RID`s are deterministically freed on
  re-layout, `NOTIFICATION_EXIT_TREE`, and destructor.

### Vertical clip culling

Before iterating `LayoutDocument::primitives`, `_draw_document()` computes the
visible vertical band from the nearest ancestor `ScrollContainer` (or canvas
visible rect) and skips primitives completely above or below the viewport band.

---

## 5. `MarkdownView` control lifecycle and live theming

### Sizing contract

`MarkdownView::_get_minimum_size()` always returns `Vector2(0.0,
layout.total_height)` — never the last laid-out width. Inside a
`ScrollContainer`, `VBoxContainer`, or split pane, the control can therefore
shrink and expand freely without locking horizontally to its widest prior width.
On `NOTIFICATION_RESIZED`, width changes exceeding `0.5px` trigger a relayout,
`update_minimum_size()`, `queue_redraw()`, and the `layout_updated` signal.
Before the control has a positive layout width (for example, in headless unit
tests before tree layout), it falls back to `custom_minimum_size.x` or `400.0`.

### Live theme propagation

`MarkdownTheme` connects to `changed` on every registered `MarkdownCalloutStyle`
and forwards emissions through its own `changed` signal. `MarkdownView` connects
to its active `MarkdownTheme::changed` (disconnecting cleanly on theme swap or
destruction), so editing a color, font size, metric, or callout style in code or
the Inspector immediately updates all attached views.

### Hit-testing & pointer exit

Layout materializes three flat control-local lists — `link_hit_boxes`,
`task_hit_boxes`, and `code_scroll_regions` — queried directly in
`_gui_input()`. When `NOTIFICATION_MOUSE_EXIT` fires, `MarkdownView` clears
hovered link state, restores the default cursor, and emits `link_hovered("")`
only if a link was actively hovered.

---

## 6. Build system, `gpm` sliced packaging, and verification

### Native builds (`native/SConstruct`, `scripts/build-native`)

One C++17 codebase builds across all targets via SCons against `godot-cpp`:

| Target | Command | Output under `addons/gdmarkdown/bin/` |
| --- | --- | --- |
| macOS (`arm64`) | `task macos` | `macos_arm64/gdmarkdown.framework` |
| iOS (device + simulator `arm64`) | `task ios` | `ios/gdmarkdown.xcframework` (dynamic Mach-O dylibs, signed via `scripts/sign-ios-frameworks`) |
| Android (`arm64`, `arm32`, `x86_64`) | `task android` | `android/libgdmarkdown.android.release.<arch>.so` |
| Linux (`x86_64`, `arm64`) | `task linux` | `linux_<arch>/libgdmarkdown.so` |

`addons/gdmarkdown/gdmarkdown.gdextension` declares `compatibility_minimum =
"4.6"` (matching Godot 4.6+ dynamic iOS `.xcframework` loading) and maps every
platform library path explicitly.

### `gpm` slice packaging

- `packaging/gpm-package.toml` is kept deliberately outside the repository root
  so a bare `gpm package` cannot accidentally publish `native/`, `demo/`, or
  `tests/`.
- `scripts/stage-addon-tree` is the single source of truth for what ships in a
  release or `task vendor`.
- `scripts/package-addon` stages the addon tree, injects the release version
  into `plugin.cfg`, and invokes `gpm package`. Because every native binary is
  declared in `[libraries]` inside `gdmarkdown.gdextension`, `gpm` automatically
  partitions the release into 8 slices: `core`, `ios`, `macos.arm64`,
  `android.arm64`, `android.arm32`, `android.x86_64`, `linux.x86_64`, and
  `linux.arm64`.
- `scripts/package-addon` asserts that `core` carries zero native binaries or
  `native/` sources, and `scripts/verify-package` verifies the full 8-slice
  contract and byte-for-byte archive reproducibility (synthesizing placeholder
  binaries on single-OS hosts when cross-OS artifacts are absent).

### GDScript strictness and verification gates

`project.godot` enables all Godot 4.7 GDScript warnings as **errors** (including
`inferred_declaration` and `return_value_discarded`), and `.gdkit/lint.json`
enforces strict explicit types across `addons/gdmarkdown`, `demo`, and `tests`.

```sh
task lint                # gdkit format, lint, license, and uid checks
task test:native         # C++ unit tests (parser, callouts, HTML stripping, table sizing)
task test:extension-load # throwaway Godot project GDExtension load verification
task test:gdscript       # gdtest integration suite
task verify:package      # 8-slice packaging contract and reproducibility check
```

The interactive showcase in `demo/` (`demo/demo.tscn`, `demo/demo.gd`,
`demo/demo_presets.gd`, `demo/demo_icons.gd`) exercises every native capability,
and `demo/capture.gd` renders the README screenshots into `docs/images/` inside
fixed-size `SubViewport`s via `scripts/capture-screenshots`.
