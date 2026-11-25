# Markdown syntax in gdmarkdown

`MarkdownView` renders CommonMark with GFM tables, task lists, strikethrough,
and bare autolinks. It also supports GitHub-style alerts and custom callouts.

## Headings, paragraphs, and breaks

Use one to six `#` characters followed by a space for headings. You can also
underline a heading with `=` for level 1 or `-` for level 2. Separate
paragraphs with a blank line. Put a thematic break such as `---`, `***`, or
`___` on its own line. Leave a blank line before `---` after a paragraph so
it renders as a rule instead of a level 2 heading.

```markdown
# Chapter title

Section title
-------------

The first paragraph
continues on the same line when rendered.

This line ends with a backslash\
and the next line starts below it.

---
```

A single newline within a paragraph renders as a space. End a line with two
or more spaces or a backslash to force a line break. Inline `<br>`, `<br/>`,
and `<br />` also force a line break.

## Inline formatting

| Effect | Markdown |
| --- | --- |
| Italic | `*text*` or `_text_` |
| Bold | `**text**` or `__text__` |
| Bold and italic | `***text***` |
| Strikethrough | `~~text~~` |
| Inline code | `` `code` `` |
| Literal Markdown punctuation | `\*literal asterisks\*` |
| Character references | `&amp;`, `&#169;`, or `&#xA9;` |

Use a longer backtick delimiter if an inline code span contains backticks.

## Links

Use inline links, reference links, or autolinks. Reference links can use a
separate label such as `[the guide][guide]` or reuse the visible label
with `[guide][]` or `[guide]`.

```markdown
[Read the guide](https://example.com/guide)
[Read the guide][guide]
[guide]

[guide]: https://example.com/guide

<https://example.com>
<help@example.com>
https://example.com
www.example.com
help@example.com
```

Clicking a link emits `link_clicked(url)`. Your game handles navigation or
any other action tied to that destination.

## Lists and checkboxes

Use `-`, `+`, or `*` for bullets and a number followed by `.` or `)`
for ordered lists. The first marker sets the starting number. Indent nested
items to align with the parent item's text. Both tight and loose lists are
supported. A blank line between items makes the list loose.

```markdown
3. Open the gate.
4. Explore the courtyard.
   - Check the fountain.
   - Search the tower.

- [x] Find the key.
- [ ] Return to the gate.
```

Use `[ ]` for unchecked tasks and `[x]` or `[X]` for checked tasks.
Checkboxes are read-only by default. Set `MarkdownView.interactive_tasks`
to `true` to let players toggle them and emit
`task_toggled(task_index, checked)`. Indices are zero-based in document
order, with a parent task indexed before its nested tasks. Assigning new
Markdown initializes the checkboxes from the source again.

## Blockquotes and callouts

Prefix quoted lines with `>`. Use another `>` inside the quote for a
nested quote, or a line containing only `>` to separate quoted paragraphs.

```markdown
> The old road leads north.
>
> > The bridge is closed after dark.
```

To make a callout, start the quote with `[!KIND]` on the opening line.
Use a space after the marker to add a custom title. Titles render as plain
text. Callout kinds are case-insensitive, start with an ASCII letter, and
can contain ASCII letters, digits, hyphens, and underscores.

```markdown
> [!NOTE]
> Save your progress before entering the ruins.

> [!TIP] Shortcut
> Follow the river to reach the keep.

> [!QUEST] The lost key
> - Search the courtyard.
> - Return to the gate.
```

The built-in kinds are `NOTE`, `TIP`, `IMPORTANT`, `WARNING`, and
`CAUTION`. Custom kinds use the style registered in `MarkdownTheme` or
a generated fallback. The parser accepts Obsidian-style `+` and `-` suffixes
after the marker. The rendered callout body is always visible.

## Code blocks

Fence code with at least three backticks or tildes. Close the block with
the same character and at least as many of it. An indented code block uses
four spaces or a tab at the top level.

````markdown
```gdscript
print("Hello, world!")
```

    This is an indented code block.
````

A fence can include a language label. All code blocks render as plain
monospaced text without syntax highlighting.

## Tables

Use a header row and a delimiter row with one entry per column. Use
`:---` for left alignment, `:---:` for centered text, and `---:` for
right alignment. A plain `---` uses the default alignment. Leading and
trailing pipes are optional.

```markdown
| Item | Rarity | Cost |
| :--- | :----: | ---: |
| **Potion** | Common | 20 |
| Map \| East | Rare | 50 |
```

Cells support inline formatting. Escape a literal pipe as `\|` so it stays
in the cell.

## Images

Use `![alt text](source)`. An image alone in a paragraph becomes a
standalone image that fits the available width. An image amid other
content stays inline and scales to the text height.

```markdown
![Map of the ruins](res://art/ruins-map.png)

You found a ![gold key](res://art/key.png) in the courtyard.
```

`MarkdownView` tries your `image_resolver` callback first, then loads
`res://` resources directly. If neither produces a texture, the view
renders the alt text. The renderer does no network fetching. To make an
image clickable, wrap it in a Markdown link.

## Unsupported syntax

Raw HTML blocks are omitted, including their contents. Inline HTML tags
are omitted except for the line-break forms above. Use Markdown for layout
and `MarkdownTheme` for styling.

Footnotes, LaTeX math, and wiki links are outside the supported dialect.
