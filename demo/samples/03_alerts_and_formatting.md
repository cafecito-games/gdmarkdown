# Alerts & Formatting Showcase

![Harbor Crest](icon://banner)

*Emphasis*, **strong emphasis**, ***bold italic***, ~~deprecated strike~~, inline code pills like `MarkdownView.visible_ratio`, inline icons ![Gold](icon://gold) ![Sword](icon://sword), and [interactive links](https://godotengine.org) all shape natively through Godot's `TextParagraph`.

## GitHub Alerts & Custom Titles

> [!NOTE]
> Standard informational callout with a crisp vector icon and subtle surface tint.

> [!TIP] Pro Tip
> Pass any text after `[!TIP]` to replace the default header with a custom title.

> [!IMPORTANT]
> Crucial system constraints stand out cleanly without visual clutter.

> [!WARNING]
> Modifying `MarkdownTheme` properties triggers an automatic layout and redraw pass.

> [!CAUTION]
> Links never call `OS.shell_open()` on their own—always inspect `link_clicked(url)` in your host scene.

## Nested Lists & Blockquotes

1. Native `md4c` parsing with zero BBCode escaping
2. Two-pass auto-sized tables with per-column alignment
   - Nested unordered bullet with `inline_code`
   - Supports arbitrary depth and tight or loose spacing
3. Interactive task checkboxes synced to `task_toggled(index, checked)`

> Outer dialogue quote with **rich inline styling**.
>
> > Nested attribution quote showing clean recursive indentation.

## Code Block (Toggle Wrap Code)

```gdscript
func _configure_markdown_view(view: MarkdownView, theme: MarkdownTheme) -> void:
	view.markdown_theme = theme
	view.interactive_tasks = true
	view.image_resolver = Callable(MarkdownDemoIcons, "resolve")
	var sample_note: String = "Long line for horizontal scroll testing when Wrap Code is toggled off: MarkdownView clips and scrolls cleanly without widening the parent ScrollContainer."
	view.markdown = "# Ready\n\n" + sample_note
```

---

## Aligned Data Table

| Feature | Engine Pipeline | Alignment | Status |
| :------ | :-------------- | :-------: | -----: |
| Text shaping | `TextParagraph` + `TextServer` | Center | `100%` |
| Callout alerts | Procedural vectors + `StyleBox` | Center | `100%` |
| Typewriter reveal | Per-glyph cutoff (zero reshape) | Center | `100%` |
