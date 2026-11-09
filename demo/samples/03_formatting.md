# Formatting reference

## Inline

*Emphasis*, **strong emphasis**, ***both***, ~~strikethrough~~, a `code span`,
and a [link with a tooltip](https://example.com "Hover me").

Bare URLs autolink: https://godotengine.org · www.example.com

Escapes stay literal: \*not emphasis\*, and entities resolve: &hellip; &mdash;

A line ending in two spaces  
breaks hard, while a plain newline
joins into one paragraph.

## Lists

1. Ordered lists
2. Keep their numbering
   - And nest
   - Any depth
7. And renumber from the list's start

## Code

```gdscript
func _ready() -> void:
	var label: MarkdownLabel = MarkdownLabel.new()
	label.markdown = "# Hello"
	add_child(label)
```

## Tables

| Column | Centered | Right |
| :----- | :------: | ----: |
| one | two | 3 |
| four | five | 60 |
