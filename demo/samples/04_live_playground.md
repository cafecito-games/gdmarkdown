# Live Playground

Edit the Markdown in the left pane to watch **`MarkdownView`** re-parse and re-layout on every keystroke.

> [!TIP] Try Custom Syntax
> - Change `[!TIP]` to `[!NOTE]`, `[!WARNING]`, `[!CAUTION]`, `[!QUEST]`, or `[!LORE]`
> - Toggle **Interactive Tasks** in the toolbar and click the checkboxes below
> - Insert inline icons with `![Gold](icon://gold)` or `![Potion](icon://potion)`

## Scratchpad Checklist

- [x] Native C++ `MarkdownView` layout active
- [ ] Add a custom `> [!QUEST]` block
- [ ] Test **Typewriter Reveal** with the toolbar slider

| Key | Value | Notes |
| :-- | :---: | ----: |
| `icon://gold` | ![Gold](icon://gold) | Inline coin |
| `icon://sword` | ![Sword](icon://sword) | Inline blade |
| `icon://potion` | ![Potion](icon://potion) | Inline flask |
| `icon://shield` | ![Shield](icon://shield) | Inline shield |

```gdscript
var view: MarkdownView = MarkdownView.new()
view.markdown_theme = MarkdownTheme.create_dark()
view.markdown = "Hello from **gdmarkdown v2**!"
```
