# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
