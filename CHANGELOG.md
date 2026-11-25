# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Links and interactive tasks activate on release so touch scrolling can start
  on interactive content. Unwrapped code blocks wait for a horizontal drag
  before taking over the gesture.
- Task checkboxes use the list gutter as a tap target.

## [0.2.2] - 2026-11-25

### Fixed

- 64-bit Android native libraries (`arm64` and `x86_64`) are now linked with
  `-Wl,-z,max-page-size=16384` and verified at build time so every ELF
  `PT_LOAD` segment has at least 16 KB alignment, satisfying Android's 16 KB
  page-size requirement.

## [0.2.1] - 2026-11-23

### Fixed

- `MarkdownView` now repaints when scrolling moves the visible band it culls to.
  A document taller than its `ScrollContainer` is drawn one band at a time, but
  the band was recomputed only while drawing and nothing redrew when the
  container scrolled, so the view kept showing the band that was visible when it
  last drew and the rest of a long document appeared blank. The view now watches
  its own transform and its scroll ancestor's size, and redraws when the band
  actually moves.

## [0.2.0] - 2026-11-22

### Changed

- **Breaking:** Replaced the v1 pure-GDScript BBCode/`RichTextLabel` renderer
  with a native C++ GDExtension (`godot-cpp` + vendored `md4c` v0.6.0) rendering
  directly onto Godot's `Control`, `TextParagraph`, `TextServer`, and `StyleBox`
  pipeline.
- Raised minimum supported Godot version to **4.6+** (verified on **4.7.2**) to
  support dynamic iOS `.xcframework` loading.
- Upgraded `gpm` release packaging from a single pure-GDScript `core` archive to
  8 platform-targeted slices (`core`, `ios`, `macos.arm64`, `android.arm64`,
  `android.arm32`, `android.x86_64`, `linux.x86_64`, `linux.arm64`).

### Added

- `MarkdownView` (`Control`) native node with:
  - Full CommonMark and GFM rendering (headings, paragraphs, emphasis, strong,
    strikethrough, rounded inline code pills, fenced/indented code blocks,
    nested blockquotes and lists, thematic breaks, hard/soft line breaks, and
    `<br>`/`<br/>` support while omitting all other raw HTML).
  - Zero-minimum-width responsive sizing contract for lockup-free horizontal
    reflow inside `ScrollContainer`, `VBoxContainer`, and split panes.
  - Two-pass auto-sized pipe tables with per-column alignment and narrow-width
    cell padding clamping.
  - Interactive task-list checkboxes (`interactive_tasks`, `get_task_count()`,
    `is_task_checked()`, `set_task_checked()`, `task_toggled` signal) with
    automatic runtime state reset on document replacement.
  - Optional horizontal scrolling inside unwrapped code blocks
    (`code_block_wrap = false`) using clipped child `RID` canvas items.
  - Zero-reshaping typewriter reveal via synchronized `visible_characters` and
    `visible_ratio` properties.
  - Inline icon and standalone block image rendering via custom `image_resolver`
    `Callable` or local `res://` loading with texture caching
    (`clear_image_cache()`) and alt-text fallback.
  - Pixel-accurate link hit-testing, hover highlight, and `link_clicked`,
    `link_hovered`, `task_toggled`, and `layout_updated` signals.
- `MarkdownTheme` (`Resource`) with `create_dark()` and `create_light()`
  presets, font-family slots, font-relative (`em`) spacing rhythm, optional
  `StyleBox` overrides, extensible callout registry (`set_callout_style()`,
  `get_callout_style()`), metric clamping, and live `changed` propagation.
- `MarkdownCalloutStyle` (`Resource`) for built-in GitHub Alerts (`NOTE`, `TIP`,
  `IMPORTANT`, `WARNING`, `CAUTION`), custom callout titles (`> [!TIP] Pro Tip`),
  and custom game callout kinds (`> [!QUEST]`, `> [!LORE]`) with procedural
  vector fallback icons.
- Native C++ SCons build system (`native/SConstruct`, `scripts/build-native`,
  `Taskfile.yml`) targeting macOS (`arm64`), iOS (device + simulator dynamic
  `.xcframework` with codesigning), Android (`arm64`, `arm32`, `x86_64`), and
  Linux (`x86_64`, `arm64`).
- Multi-layer verification suite: headless C++ unit tests (`task test:native`),
  throwaway Godot GDExtension load probe (`task test:extension-load`), `gdtest`
  integration tests and benchmarks (`task test:gdscript`), and 8-slice package
  verification (`task verify:package`).
- Redesigned interactive showcase (`demo/`) with four harmonized theme presets
  (*Slate Dark*, *Paper Light*, *Parchment RPG*, *Terminal*), procedural
  `icon://` resolver, live split-pane Markdown editor, typewriter reveal
  controls, and real-time signal diagnostics.

### Removed

- Pure-GDScript v1 BBCode pipeline (`MarkdownLabel`, `Markdown`,
  `MarkdownStyle`, `MarkdownDocument`, `MarkdownNode`, and GDScript parser /
  BBCode emitter classes).

## [0.1.0] - 2026-11-09

### Added

- `MarkdownLabel`, a `RichTextLabel` that renders Markdown, with live preview in
  the editor and a `link_clicked` signal.
- `Markdown.to_bbcode()` and `Markdown.parse()` for converting Markdown without
  the node.
- `MarkdownStyle`, a resource holding heading sizes, code and quote colors, list
  glyphs, and image width.
- CommonMark support: headings (ATX and setext), paragraphs, emphasis, strong
  emphasis, code spans, fenced and indented code blocks, block quotes, ordered,
  unordered and nested lists, links, reference links, autolinks, images,
  thematic breaks, hard line breaks, backslash escapes, and HTML entities.
- GitHub Flavored Markdown support: tables with column alignment, strikethrough,
  task lists, and bare URL autolinks.
- An interactive demo scene with three style presets, which is also the
  project's main scene.
- Benchmarks for each pipeline stage, run with `task bench`.
- An MIT SPDX header in every GDScript file, managed by `gdkit license` and
  verified in CI.
