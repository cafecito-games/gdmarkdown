# Release Notes v2.0

A paragraph with *emphasis*, **strong emphasis**, ***bold italic***,
~~strikethrough~~, an `inline_code()` span, a
[project link](https://example.com "Project home"), and an inline icon
![coin](icon://gold).

> [!NOTE]
> Native C++ rendering via `md4c` and Godot `TextParagraph`.

> [!TIP] Pro Tip
> Customize typography and spacing through `MarkdownTheme`.

> [!IMPORTANT]
> Set `interactive_tasks = true` to allow clicking task checkboxes.

> [!WARNING]
> Very narrow tables shrink column widths proportionally.

> [!CAUTION]
> Raw `<script>alert("blocked")</script>` HTML is stripped safely.

> [!QUEST] Dragon's Hoard
> Gather 3 embers and return before dusk.

## Changes

- Fixed the grapple hook
- Rebalanced the **boss** fight
  - Lowered its health
  - Slowed its second phase
- Added [reference links][docs]

1. First step
2. Second step

### Tasks

- [x] Shipped the native renderer
- [ ] Write the postmortem
- [x] Verified callout alerts

> A quoted remark.
>
> > And a nested one.

```gdscript
func compute_total(items: Array[int]) -> int:
	var total: int = 0
	for value: int in items:
		total += value
	return total
```

| Item | Count | Price |
| :--- | :---: | ----: |
| Rope | 2 | 10 |
| Grappling Hook | 1 | 25 |
| Lantern Oil | 4 | 8 |

![Banner preview](icon://banner "Banner")

---

Visit https://example.com or email <team@example.com>. Literal \*asterisks\*
and entities like &hellip; pass through. A line ending with a tag<br/>
breaks hard.

[docs]: https://example.com/docs
