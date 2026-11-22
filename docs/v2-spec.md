# gdmarkdown v2 — Native C++ Markdown Renderer Specification

## 1. Overview

`gdmarkdown` v2 replaces the pure-GDScript BBCode/`RichTextLabel` translation layer with a native C++ GDExtension (`godot-cpp` + `md4c`) that renders Markdown directly onto Godot's `Control` / `TextParagraph` / `TextServer` / `StyleBox` pipeline.

The design is inspired by [gonzalezreal/textual](https://github.com/gonzalezreal/textual) — preserving the host engine's native text shaping, DPI scaling, theming, and input pipeline while delivering a real block box model, extensible callouts/alerts, auto-sized tables, inline code pills, inline images/icons, task lists, and RPG-friendly typewriter reveal.

### Goals
- **In-game Markdown rendering** with clean, crisp, game-ready visuals out of the box and full Inspector theming (`MarkdownTheme`, `MarkdownCalloutStyle`).
- **Full CommonMark + GFM core** via `md4c`: headings (H1–H6), paragraphs, emphasis, strong, strikethrough, inline code, fenced & indented code blocks, links, autolinks, blockquotes, ordered/unordered/nested lists, task lists, tables (with column alignment), thematic breaks, hard/soft breaks, and block/inline images.
- **GitHub Alerts & Extensible Callouts**: built-in `NOTE`, `TIP`, `IMPORTANT`, `WARNING`, `CAUTION` plus custom titles (`> [!TIP] Pro Tip`) and game-registered callout kinds (`> [!QUEST]`, `> [!LORE]`).
- **One portable C++ codebase** targeting **macOS (`arm64`)**, **iOS (`arm64` device + simulator dynamic `.xcframework`, Godot 4.6+)**, **Android (`arm64-v8a`, `armeabi-v7a`, `x86_64` `.so`)**, and **Linux (`x86_64`, `arm64` `.so`)** — with **no** `SwiftGodot` dependency, **no** Java/Gradle AARs, and **no** desktop stubs.
- **Cafecito packaging & quality conventions** mirroring `AuthenticationKit` and `PurchaseKit`: `Taskfile.yml`, `packaging/gpm-package.toml` (out of repo root), `scripts/stage-addon-tree`, `scripts/package-addon`, `scripts/sign-ios-frameworks`, `scripts/test-gdextension-load`, `gdkit`, and `gdtest`.

### Explicit Non-Goals (v2.0)
- Syntax highlighting inside code blocks.
- LaTeX / math rendering.
- Animated GIF / APNG decoding.
- Arbitrary raw HTML/CSS layout (only `<br>` / `<br/>` is interpreted as a hard line break; all other raw HTML is safely omitted).
- Web / WASM export.
- Network I/O or `OS.shell_open` (the addon never performs network requests or launches external apps; links emit signals and images resolve locally or via an explicit host callback).

### Modeled in IR for v2.1 (Additive, Non-Breaking)
- Cross-block text selection & copy (`selection_enabled` property reserved; layout stores document-ordered `TextParagraph` rects and logical offsets).
- Collapsible callouts (`> [!NOTE]- Title` and `> [!NOTE]+ Title` parsed into `collapsible` / `initially_collapsed` fields on the IR).

---

## 2. Markdown Dialect & Parsing (`md4c`)

### Upstream Dependencies
- **`godot-cpp`**: pinned to `godot-4.5-stable` with `compatibility_minimum = "4.6"` in `.gdextension` (requires Godot 4.6+, verified on Godot 4.7.2, matching `PurchaseKit`'s Godot 4.6+ dynamic iOS `.xcframework` support).
- **`md4c`**: vendored MIT-licensed C99 source (`md4c.c`, `md4c.h`, `LICENSE.md`) at `v0.6.0` under `native/third_party/md4c/`.

### Parser Flags
`md4c` is invoked with:
- `MD_FLAG_TABLES`
- `MD_FLAG_STRIKETHROUGH`
- `MD_FLAG_TASKLISTS`
- `MD_FLAG_PERMISSIVEAUTOLINKS`
- `MD_FLAG_COLLAPSEWHITESPACE` (disabled inside code blocks/spans by `md4c` automatically)

Raw HTML flags are **not** disabled so `md4c` emits `MD_TEXT_HTML` tokens:
- Case-insensitive `<br>`, `<br/>`, `<br />` emit `InlineKind::HardBreak`.
- All other `MD_TEXT_HTML` tokens and `MD_BLOCK_HTML` blocks are discarded cleanly.

### Document IR (`native/src/parser/document.hpp`)
Pure C++ structs (no Godot headers required, enabling fast headless C++ unit testing):

- **`Document`**: `std::vector<Block> blocks`, `int task_count`.
- **`Block`** (tagged variant / struct):
  - `Paragraph { std::vector<InlineNode> inlines }`
  - `Heading { int level (1..6); std::vector<InlineNode> inlines }`
  - `BlockQuote { std::vector<Block> children }`
  - `Callout { std::string kind; std::string custom_title; bool collapsible; bool initially_collapsed; std::vector<Block> children }`
  - `List { bool ordered; int start; bool tight; std::vector<ListItem> items }`
    - `ListItem { bool is_task; bool checked; int task_index; std::vector<Block> children }`
  - `CodeBlock { std::string info_language; std::string code }`
  - `Table { std::vector<TableAlign> alignments; std::vector<TableRow> header_rows; std::vector<TableRow> body_rows }`
    - `TableCell { TableAlign align; std::vector<InlineNode> inlines }`
  - `ThematicBreak {}`
  - `ImageBlock { std::string src; std::string alt; std::string title }` (promoted when a paragraph contains exclusively a single image inline, optionally with whitespace).
- **`InlineNode`**:
  - `Text { std::string text; uint8_t style_flags (Bold, Italic, Strikethrough, Code); std::string link_url }`
  - `Image { std::string src; std::string alt; std::string title; std::string link_url }`
  - `SoftBreak {}` / `HardBreak {}`

### Callout Detection Post-Pass
After `md4c` builds the block tree, a recursive post-pass inspects every `BlockQuote`:
1. Check if `children[0]` is a `Paragraph` whose leading plain text (not inside code/link/emphasis) matches the directive syntax `^\[!([A-Za-z][A-Za-z0-9_-]*)\](\+|-)?(?:[ \t]+([^\n]*))?` (implemented with a direct, zero-regex hand-written scanner) up to the first `SoftBreak`, `HardBreak`, or end of paragraph.
2. If matched:
   - Promote `BlockQuote` → `Callout` with `kind` uppercased (e.g., `"NOTE"`, `"QUEST"`).
   - Capture optional `collapsible` / `initially_collapsed` (`+` or `-`) and `custom_title` (trimmed).
   - Strip the directive header (and its trailing break if present) from `children[0]`; if `children[0]` is now empty, erase it from `children`.
3. Recurse into all container blocks (`BlockQuote`, `Callout`, `ListItem`) so nested callouts work cleanly.

### Task Indexing & Lifecycle
During parsing/post-pass, assign a sequential 0-based `task_index` to every `ListItem` where `is_task == true` in depth-first document order. Runtime checkbox states are stored in `std::vector<bool> task_states` on `MarkdownView` so toggling a checkbox updates visually without re-parsing Markdown. Whenever `markdown` is reassigned (or cleared), `task_states` is **cleared and repopulated from the newly parsed document's initial task states**, so toggled states from a prior document can never leak onto a replacement document.

---

## 3. Public Godot API

All classes are registered by the GDExtension under `MODULE_INITIALIZATION_LEVEL_SCENE`.

### `MarkdownCalloutStyle` (`Resource`)
Defines the visual presentation of one callout kind.

| Property | Type | Default | Notes |
| --- | --- | --- | --- |
| `title` | `String` | `""` | Default header title when Markdown omits a custom title. |
| `accent_color` | `Color` | Slate blue | Left accent bar, default header tint, and fallback vector icon color. |
| `title_color` | `Color` | `Color(0,0,0,0)` | When alpha == 0, uses `accent_color`. |
| `icon` | `Texture2D` | `null` | Optional icon texture; when `null`, draws a crisp built-in procedural vector icon for `NOTE`/`TIP`/`IMPORTANT`/`WARNING`/`CAUTION` or a clean badge glyph for custom kinds. |
| `panel_style` | `StyleBox` | `null` | Optional background card; when `null`, renders a subtle rounded tinted box (`accent_color` at low alpha) with a solid left accent bar. |

### `MarkdownTheme` (`Resource`)
Central styling resource editable in the Godot Inspector. Modifying any property emits `changed`, automatically re-laying out any attached `MarkdownView`.

- **Fonts** (all fall back to the `Control`'s theme default font when `null`):
  - `regular_font: Font`
  - `bold_font: Font`
  - `italic_font: Font`
  - `bold_italic_font: Font`
  - `monospace_font: Font`
- **Typography & Font-Relative (`em`) Rhythm**:
  - `base_font_size: int = 16`
  - `code_font_scale: float = 0.9`
  - `line_spacing: float = 4.0`
  - `paragraph_spacing_em: float = 0.75`
  - `heading_scales: PackedFloat32Array = [1.85, 1.5, 1.25, 1.1, 1.0, 0.9]`
  - `heading_top_spacing_em: float = 1.1`
  - `heading_bottom_spacing_em: float = 0.4`
  - `list_indent_em: float = 1.5`
  - `list_item_spacing_em: float = 0.3`
  - `blockquote_indent_em: float = 1.0`
- **Colors**:
  - `text_color`, `heading_color`, `muted_color`, `link_color`, `link_hover_color`, `code_text_color`, `code_bg_color`, `inline_code_bg_color`, `strikethrough_color`, `rule_color`, `blockquote_accent_color`, `blockquote_bg_color`, `table_border_color`, `table_header_bg_color`, `table_alt_row_bg_color`, `task_checked_color`, `task_unchecked_color`.
- **StyleBoxes** *(all optional — clean procedural defaults drawn when `null` so zero-config rendering looks great)*:
  - `code_block_style: StyleBox`
  - `inline_code_style: StyleBox`
  - `blockquote_style: StyleBox`
  - `table_style: StyleBox`
  - `table_header_style: StyleBox`
- **Box Metrics**:
  - `inline_code_padding_h: float = 4.0`
  - `inline_code_padding_v: float = 1.5`
  - `code_block_padding: float = 12.0`
  - `blockquote_padding: float = 10.0`
  - `blockquote_accent_width: float = 3.0`
  - `callout_padding: float = 12.0`
  - `callout_accent_width: float = 4.0`
  - `callout_icon_size: float = 16.0`
  - `table_cell_padding_h: float = 10.0`
  - `table_cell_padding_v: float = 6.0`
  - `rule_thickness: float = 1.0`
  - `corner_radius: float = 6.0`
  - `max_image_height: float = 420.0`
  - `inline_image_scale: float = 1.15`
- **Callout Registry**:
  - `callouts: Dictionary` (`String -> MarkdownCalloutStyle`), pre-seeded with `NOTE`, `TIP`, `IMPORTANT`, `WARNING`, `CAUTION`.
  - `set_callout_style(kind: String, style: Ref<MarkdownCalloutStyle>)` (normalizes `kind` to uppercase).
  - `get_callout_style(kind: String) -> Ref<MarkdownCalloutStyle>` (returns registered style or a synthesized fallback).
- **Built-in Factory Presets**:
  - `MarkdownTheme.create_dark() -> Ref<MarkdownTheme>`
  - `MarkdownTheme.create_light() -> Ref<MarkdownTheme>`

### `MarkdownView` (`Control`)
The primary scene node.

- **Properties**:
  - `markdown: String` (`PROPERTY_HINT_MULTILINE_TEXT`)
  - `markdown_theme: Ref<MarkdownTheme>` (auto-uses a default dark theme if unset)
  - `interactive_tasks: bool = false`
  - `code_block_wrap: bool = true`
  - `visible_characters: int = -1` (`-1` shows full document)
  - `visible_ratio: float = 1.0` (`0.0..1.0`, synchronized with `visible_characters` just like `Label`/`RichTextLabel`)
  - `image_resolver: Callable` (optional `Callable(uri: String) -> Texture2D`)
- **Signals**:
  - `link_clicked(url: String)`
  - `link_hovered(url: String)`
  - `task_toggled(task_index: int, checked: bool)`
  - `layout_updated()`
- **Methods**:
  - `get_total_character_count() -> int`
  - `get_task_count() -> int`
  - `is_task_checked(task_index: int) -> bool`
  - `set_task_checked(task_index: int, checked: bool) -> void`
  - `get_plain_text() -> String`
  - `clear_image_cache() -> void`

---

## 4. Layout, Rendering & Interaction Architecture

### Control Sizing Contract (Preventing `ScrollContainer` Lockups)
- `MarkdownView::_get_minimum_size()` returns `Vector2(0.0, layout_height)` — **never** the current layout width — so the control can freely shrink and grow horizontally inside `ScrollContainer` / `VBoxContainer` / split panes without locking to its widest prior width.
- On `NOTIFICATION_RESIZED`, if `std::abs(new_width - last_layout_width) > 0.5f`, re-run layout for `new_width`, update `layout_height`, call `update_minimum_size()`, and `queue_redraw()`.
- Before first layout with a positive width (e.g. headless unit tests or freshly added nodes), fall back to `custom_minimum_size.x > 0 ? custom_minimum_size.x : 400.0f`.

### Text Shaping & Per-Span Glyph Rendering (`TextParagraph` + `TextServer`)
- Every text leaf (paragraph, heading, list item text, code block text, table cell, callout title) shapes into a `Ref<TextParagraph>`.
- Inline font switching selects `regular_font`, `bold_font`, `italic_font`, `bold_italic_font`, or `monospace_font` (with graceful fallback if a specialized variant isn't provided).
- **Per-span colors (links, link hover, inline code, muted text)**:
  - Because `TextParagraph::draw()` paints an entire paragraph with one color, rich paragraphs render line-by-line by querying `TextServer::shaped_text_get_glyphs(paragraph->get_line_rid(line))` and drawing each glyph with `TextServer::font_draw_glyph()` using the color of the inline span containing `glyph.start` (Godot logical character index).
  - Plain single-color blocks (e.g., code blocks and callout titles) may draw directly via `TextParagraph::draw()` when no typewriter cutoff is active inside them.
- **Inline code pills**:
  - Insert zero-draw spacer objects (`add_object(spacer_key, Vector2(inline_code_padding_h, 1.0), INLINE_ALIGNMENT_CENTER, 1)`) around each inline code run so surrounding words never crowd the pill edge.
  - Record each code run's logical character range `[start, end)` (in Godot `String` / UTF-32 code-point offsets) and compute per-line pill rectangles from `TextServer::shaped_text_get_selection(line_rid, start, end)` during layout.
  - Paint pill `StyleBox` (or rounded rect with `inline_code_bg_color`) **behind** the text pass.
- **Strikethrough & Link Underlines**:
  - Record character ranges for strikethrough and links; compute per-line horizontal segments from `TextServer::shaped_text_get_selection()` during layout and draw crisp lines at baseline/midline (using `line_ascent` / `line_descent`).
- **Inline Images**:
  - Resolved once via `image_resolver` (if valid) → `ResourceLoader` (for `res://` paths only) → cached in an internal `HashMap<String, Ref<Texture2D>>`.
  - Inserted into `TextParagraph` via `add_object(image_key, display_size, INLINE_ALIGNMENT_CENTER, 1)` where `display_size` is height-scaled to `font_size * inline_image_scale` preserving aspect ratio.
  - Located per line by iterating `line` in `0 .. get_line_count() - 1`, inspecting `get_line_objects(line)`, and calling `get_line_object_rect(line, image_key)` offset by the line/paragraph origin.
  - If an image fails to resolve, emit its `alt` text cleanly as a styled fallback.
- **Block Images**:
  - Centered or left-aligned texture scaled to fit `min(available_width, texture_width)` and `max_image_height` preserving aspect ratio, with rounded corners or clean framing.

### Table Auto-Layout (Two-Pass)
1. Measure each cell's unwrapped width (`max_w`) and minimum breakable width (`min_w`, clamped to a reasonable floor like `48px + 2 * cell_padding_h`).
2. Aggregate per-column `col_min[c]` and `col_max[c]`.
3. If `sum(col_max) <= available_width`, use `col_max` (or expand cleanly up to `available_width`).
4. Otherwise, allocate `col_min[c]` plus a proportional share of `available_width - sum(col_min)` weighted by `col_max[c] - col_min[c]`. If `sum(col_min) > available_width`, scale `col_min` proportionally so `sum(col_w) <= available_width`.
5. For each column `c`, clamp horizontal cell padding so inner text width is always strictly positive:
   `effective_pad_h = std::min(cell_padding_h, std::max(0.0f, (col_w[c] - 1.0f) * 0.5f))`
   `text_w = std::max(1.0f, col_w[c] - 2.0f * effective_pad_h)`
   Shape every cell `TextParagraph` at `text_w` with its `HorizontalAlignment` (`LEFT`, `CENTER`, `RIGHT`) and `BREAK_MANDATORY | BREAK_WORD_BOUND | BREAK_ADAPTIVE`.
6. Row height = `max(cell_height) + 2 * cell_padding_v`. Paint table card background, header row fill, alternating row tint, grid dividers, and outer border cleanly.

### Code Blocks (Wrap + Optional Horizontal Scroll)
- When `code_block_wrap == true`: `TextParagraph` wraps at `inner_width` (`BREAK_MANDATORY | BREAK_WORD_BOUND | BREAK_ADAPTIVE`).
- When `code_block_wrap == false`: `TextParagraph` uses `BREAK_MANDATORY` only (`width = -1`), measuring natural line width `content_width`. If `content_width > inner_width`, the code block stores a clamped `scroll_x` (`0 .. content_width - inner_width`), draws into a lightweight child `RID` canvas item configured with **both** `canvas_item_set_custom_rect(child_rid, true, Rect2(Vector2(), inner_rect.size))` and `canvas_item_set_clip(child_rid, true)` positioned at `inner_rect.position`, draws a subtle horizontal scrollbar thumb at the bottom edge, and accepts horizontal wheel (`MOUSE_BUTTON_WHEEL_LEFT`/`RIGHT`, Shift+wheel) and horizontal touch/mouse drag inside the block rect. All child `RID`s are freed on re-layout, `NOTIFICATION_EXIT_TREE`, and destructor.

### Typewriter Reveal (`visible_characters` / `visible_ratio`)
- Each text-bearing laid-out item records its cumulative character range `[char_start, char_end)` in document order, and `total_characters` is cached on the layout.
- When `visible_characters >= 0 && visible_characters < total_characters`:
  - Items with `char_end <= visible_characters` draw normally.
  - Items with `char_start >= visible_characters` are skipped (along with empty containers/rules after the frontier).
  - The **single frontier item** (`char_start < visible_characters < char_end`) uses the same per-glyph `shaped_text_get_glyphs()` draw loop without any re-shaping: glyphs with `glyph.start >= local_cutoff` simply advance `pen_x` without calling `font_draw_glyph()` (and inline objects/pills/decorations past `local_cutoff` are skipped or clipped to the cutoff).
  - **Result:** words never jump between lines mid-word as they type out, zero re-shaping occurs per typewriter tick, and multi-colored spans render accurately right up to the reveal cursor.

### Hit-Testing & Clip Culling
- During layout, materialize flat hit-target lists in control-local coordinates:
  - `LinkHitBox { Rect2 rect; String url }` (one per line fragment of every link — pixel-accurate, never false-triggers on trailing line whitespace).
  - `TaskHitBox { Rect2 rect; int task_index }` (generous click/tap target around each task checkbox).
  - `CodeScrollBox { Rect2 rect; int block_index; float max_scroll_x }`.
- During `_draw()`, compute the visible vertical range from the control's ancestor `ScrollContainer` (or canvas visible rect) and skip drawing blocks completely above/below the visible band.
- On `NOTIFICATION_MOUSE_EXIT`, clear hovered link state, reset cursor shape, and emit `link_hovered("")` if a link was hovered.

---

## 5. Repository Layout, Build System & Packaging

Mirrors `AuthenticationKit` and `PurchaseKit` while simplifying the native stack to one C++ tree.

```text
.
├── native/                              # Build-time only (never shipped)
│   ├── SConstruct                       # SCons build driving godot-cpp + md4c + src
│   ├── third_party/
│   │   ├── godot-cpp/                   # Pinned godot-4.4-stable
│   │   └── md4c/                        # Vendored md4c v0.6.0 (MIT)
│   ├── src/
│   │   ├── register_types.hpp / .cpp
│   │   ├── parser/
│   │   │   ├── document.hpp             # Pure C++ IR (no Godot headers)
│   │   │   └── markdown_parser.hpp/.cpp # md4c callbacks + callout post-pass
│   │   ├── layout/
│   │   │   ├── table_sizing.hpp/.cpp    # Pure C++ two-pass table column math (unit-testable)
│   │   │   └── layout_engine.hpp/.cpp   # Godot TextParagraph + box layout engine
│   │   └── nodes/
│   │       ├── markdown_callout_style.hpp/.cpp
│   │       ├── markdown_theme.hpp/.cpp
│   │       └── markdown_view.hpp/.cpp
│   └── tests/
│       └── test_native.cpp              # Headless C++ unit tests (parser, callouts, table sizing)
├── addons/gdmarkdown/                   # Shippable addon directory
│   ├── plugin.cfg
│   ├── plugin.gd (+ .uid)
│   ├── gdmarkdown.gdextension (+ .uid)
│   └── bin/
│       ├── ios/gdmarkdown.xcframework
│       ├── macos_arm64/gdmarkdown.framework
│       ├── android/
│       │   ├── libgdmarkdown.android.release.arm64.so
│       │   ├── libgdmarkdown.android.release.arm32.so
│       │   └── libgdmarkdown.android.release.x86_64.so
│       ├── linux_x86_64/libgdmarkdown.so
│       └── linux_arm64/libgdmarkdown.so
├── packaging/
│   └── gpm-package.toml                 # Kept out of repo root; staged by scripts/package-addon
├── scripts/
│   ├── stage-addon-tree                 # Single source of truth for what ships
│   ├── package-addon                    # Cuts gpm platform slices + whole-addon zip
│   ├── verify-package                   # Validates slice contract & reproducibility
│   ├── sign-ios-frameworks              # Per-slice + wrapper codesign (GDMARKDOWN_SIGNING_IDENTITY)
│   ├── test-gdextension-load            # Throwaway Godot headless load probe
│   └── capture-screenshots              # Renders demo screenshots
├── demo/                                # Interactive showcase (samples, themes, live editor, typewriter)
├── tests/                               # gdtest integration suite
└── Taskfile.yml
```

### `.gdextension` Contract
```ini
[configuration]
entry_symbol = "gdmarkdown_entry_point"
compatibility_minimum = "4.6"
reloadable = true

[libraries]
ios.debug              = "res://addons/gdmarkdown/bin/ios/gdmarkdown.xcframework"
ios.release            = "res://addons/gdmarkdown/bin/ios/gdmarkdown.xcframework"
ios.simulator.debug    = "res://addons/gdmarkdown/bin/ios/gdmarkdown.xcframework"
ios.simulator.release  = "res://addons/gdmarkdown/bin/ios/gdmarkdown.xcframework"
macos.arm64            = "res://addons/gdmarkdown/bin/macos_arm64/gdmarkdown.framework"
android.debug.arm64    = "res://addons/gdmarkdown/bin/android/libgdmarkdown.android.release.arm64.so"
android.release.arm64  = "res://addons/gdmarkdown/bin/android/libgdmarkdown.android.release.arm64.so"
android.debug.arm32    = "res://addons/gdmarkdown/bin/android/libgdmarkdown.android.release.arm32.so"
android.release.arm32  = "res://addons/gdmarkdown/bin/android/libgdmarkdown.android.release.arm32.so"
android.debug.x86_64   = "res://addons/gdmarkdown/bin/android/libgdmarkdown.android.release.x86_64.so"
android.release.x86_64 = "res://addons/gdmarkdown/bin/android/libgdmarkdown.android.release.x86_64.so"
linux.x86_64           = "res://addons/gdmarkdown/bin/linux_x86_64/libgdmarkdown.so"
linux.arm64            = "res://addons/gdmarkdown/bin/linux_arm64/libgdmarkdown.so"
```

### Build Tasks (`Taskfile.yml`)
- `task macos`: builds `macos.arm64` dynamic library via SCons and wraps it into `addons/gdmarkdown/bin/macos_arm64/gdmarkdown.framework` (with `Info.plist` and `@rpath/gdmarkdown.framework/gdmarkdown` install name).
- `task ios`: builds iOS device (`arm64`) + iOS Simulator (`arm64`) **dynamic** libraries (`ios_framework` / shared Mach-O dylibs with `@rpath/gdmarkdown.framework/gdmarkdown` install name, targeting Godot 4.6+ dynamic `.xcframework` loading) via SCons, wraps each into a `.framework` bundle with `Info.plist`, merges via `xcodebuild -create-xcframework`, and signs slices + wrapper via `scripts/sign-ios-frameworks`.
- `task android`: auto-discovers NDK (`ANDROID_NDK_ROOT` / `ANDROID_NDK_HOME` / `$HOME/Library/Android/sdk/ndk/*`), builds `arm64`, `arm32`, and `x86_64` `.so` libraries via SCons.
- `task linux`: builds `linux_x86_64` and `linux_arm64` `.so` libraries via SCons.
- `task test:native`: compiles and runs `native/tests/test_native.cpp`.
- `task test:extension-load`: runs `scripts/test-gdextension-load`.
- `task test:gdscript`: runs `./addons/gdtest/gdtest run`.
- `task lint`: runs `gdkit format/lint/license/uid check`.
- `task package` / `task verify:package`: cuts and verifies `gpm` platform slices.
- `task vendor DEST=...`: rsyncs the runtime addon tree (excluding build-time sources) into a consumer project.

### Packaging & `gpm` Slices
- Published slices: `core`, `ios`, `macos.arm64`, `android`, `linux.x86_64`, `linux.arm64`.
- `scripts/package-addon` asserts the `core` slice contains zero `.so`, `.framework`, `.xcframework`, `.a`, `.dylib`, or `native/` files, and that every required slice archive exists when packaging a full release (`--allow-partial` flag available for single-platform CI verification, or synthesized fixtures in unit tests mirroring `AuthenticationKit/scripts/test-package-addon`).

---

## 6. Interactive Demo (`demo/`)

A polished in-game showcase demonstrating every v2 capability:
- **Sample selector sidebar:**
  1. *Patch Notes* (headings, task checklist, tables with alignment, callouts, inline code pills, links)
  2. *RPG Quest Log* (custom `[!QUEST]` & `[!LORE]` callouts, interactive task checkboxes, inline item/currency icons via `image_resolver`, blockquote dialogue)
  3. *Alerts & Formatting Showcase* (all 5 GitHub alerts + custom titles, nested lists, nested blockquotes, wide tables, code blocks, thematic breaks, block images)
  4. *Live Playground* (editable `TextEdit` split pane updating `MarkdownView` in real time)
- **Toolbar controls:**
  - Theme preset picker (*Slate Dark*, *Paper Light*, *Parchment RPG*, *Terminal*)
  - Interactive Tasks toggle
  - Code Wrap toggle
  - Typewriter Reveal button / slider (`visible_ratio`)
- **Live event footer:** displays `link_clicked`, `link_hovered`, and `task_toggled` events in real time.

---

## 7. Verification Criteria

1. `task test:native` passes all C++ unit tests (CommonMark/GFM parsing, callout promotion, nested structures, task indexing, raw HTML safety, two-pass table column sizing).
2. `task macos`, `task ios`, and `task android` build clean native binaries locally (`gdmarkdown.framework`, `gdmarkdown.xcframework`, and Android `.so` ABIs); Linux build runs cleanly in CI (`ubuntu-22.04`).
3. `task test:extension-load` confirms Godot 4.7.2 loads the native GDExtension with zero errors.
4. `task test:gdscript` passes the full `gdtest` integration suite (rendering, sizing, themes, callouts, task toggling, links, image resolver, typewriter reveal, edge cases).
5. `task lint` passes `gdkit` format, lint, license, and `uid://` checks with zero warnings.
6. `task verify:package` passes slice contract and reproducibility checks.
7. `./scripts/capture-screenshots` renders crisp, visually verified demo screenshots.
