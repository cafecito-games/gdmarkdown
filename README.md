# gdmarkdown

**Markdown rendering for Godot 4, straight into a `RichTextLabel`.**

[![Godot 4.7](https://img.shields.io/badge/Godot-4.7-478cbf?logo=godotengine&logoColor=white)](https://godotengine.org)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

Drop a `MarkdownLabel` into a scene, assign Markdown to it, and it renders. No
build step, no native code, no runtime dependencies — the addon is pure
GDScript and every public symbol is statically typed.

```gdscript
var label: MarkdownLabel = MarkdownLabel.new()
label.markdown = """
# Patch notes

Fixed the **grapple hook** clipping through walls.

- [x] Reproduced the bug
- [ ] Write a regression test

See the [issue](https://example.com/issues/42) for details.
"""
add_child(label)
```

## Install

With [gpm](https://github.com/cafecito-games/godot-package-manager):

```sh
gpm add --name gdmarkdown \
  --source github-release \
  --repo cafecito-games/gdmarkdown \
  --version <tag>
```

Then enable **gdmarkdown** in *Project ▸ Project Settings ▸ Plugins*. The addon
is pure GDScript, so it publishes a single `core` slice and installs the same
files on every platform.

## Using it

### The node

`MarkdownLabel` extends `RichTextLabel`, so every property you already know —
`fit_content`, `scroll_active`, `theme`, `selection_enabled` — works unchanged.

| Member | Meaning |
| --- | --- |
| `markdown: String` | The Markdown source. Setting it re-renders. |
| `markdown_style: MarkdownStyle` | Visual settings. Empty means defaults. |
| `link_clicked(target: String)` | Emitted when a link is clicked. |
| `get_document() -> MarkdownDocument` | The parsed tree, for building an outline. |

Conversion runs in the editor too, so the inspector shows the rendered result as
you type.

Links deliberately do nothing on their own. Opening a URL is one line in the
consumer, and the addon never calls `OS.shell_open` on content it rendered:

```gdscript
func _on_link_clicked(target: String) -> void:
	if target.begins_with("https://"):
		OS.shell_open(target)
```

### Without the node

`Markdown` is a static facade for a `RichTextLabel` you already own:

```gdscript
label.bbcode_enabled = true
label.text = Markdown.to_bbcode(source, my_style)
```

`Markdown.parse(source)` returns the document tree instead, which is what you
want for a table of contents or a search index.

### Styling

`MarkdownStyle` is a `Resource`, so it is inspector-editable, saveable as a
`.tres`, and shareable between labels. Changing any property re-renders every
label using it.

| Property | Default | Meaning |
| --- | --- | --- |
| `heading_font_sizes` | `[32, 28, 24, 20, 18, 16]` | Font size per heading level |
| `heading_bold` | `true` | Whether headings render bold |
| `code_background_color` | dark slate | Background of code spans and blocks |
| `code_text_color` | warm sand | Text color of code spans and blocks |
| `quote_color` | muted grey | Block quotes and thematic breaks |
| `link_color` | blue | Link color, when `color_links` is on |
| `color_links` | `true` | Disable to inherit the label's theme |
| `unordered_bullet` | `•` | Bullet in front of unordered items |
| `task_unchecked_glyph` | `☐` | Unchecked task list marker |
| `task_checked_glyph` | `☑` | Checked task list marker |
| `thematic_break_text` | `──────────` | Text drawn for `---` |
| `preserve_soft_breaks` | `false` | Keep single newlines as line breaks |
| `show_code_language` | `false` | Print a fence's info string above it |
| `image_width` | `0` | Scale images to this width; 0 keeps natural size |
| `render_tables` | `true` | Off renders rows as plain lines, which reflows better |

## What is supported

Everything a `RichTextLabel` can express, from CommonMark and the GitHub
Flavored Markdown extensions:

- Headings, both ATX (`# Title`) and setext (`Title` over `===`)
- Emphasis, strong emphasis, and strikethrough, including nested `***both***`
- Code spans, fenced code blocks (``` and `~~~`), and indented code blocks
- Block quotes, nested to any depth
- Ordered, unordered, nested, and task lists, tight or loose, with any start number
- Links, reference links, angle autolinks, and bare URL autolinks
- Images from `res://` and `user://`
- Tables with per-column alignment
- Thematic breaks, hard line breaks, backslash escapes, and HTML entities

### Deliberate limits

- **Images are local only.** A `res://` or `user://` path the project can load
  becomes an `[img]`. Anything else, a remote URL included, renders as its
  alternate text linked to the URL. The addon ships no HTTP client, so nothing
  it renders can reach the network.
- **Raw HTML renders as literal text.** A `RichTextLabel` cannot lay out HTML,
  and passing the tags through would mean inventing a mapping per element.
- **Emphasis resolution is forward-searching, not CommonMark's delimiter
  stack.** Every ordinary nesting resolves identically. Pathological
  overlapping runs such as `*foo**bar*baz**` render as literal text rather than
  being guessed at.
- **Syntax highlighting is out of scope.** A fence's info string is parsed and
  available on the tree; `show_code_language` prints it above the block.

## Developing

Requires [gdkit](https://github.com/cafecito-games/gdkit) for linting and
formatting, [gpm](https://github.com/cafecito-games/godot-package-manager) to
install the test runner, and Godot 4.7.

```sh
gpm install   # installs the gdtest runner into addons/gdtest
task lint     # gdkit format check, lint check, uid check
task test     # ./addons/gdtest/gdtest run
task          # both
```

Every GDScript warning Godot 4.7 offers is enabled as an **error** in
`project.godot`, including `untyped_declaration`, `inferred_declaration` and the
four `unsafe_*` warnings. That means no `:=` inference and no implicit `Variant`
anywhere in the addon, by construction rather than by review.

## License

MIT. See [LICENSE](LICENSE).
