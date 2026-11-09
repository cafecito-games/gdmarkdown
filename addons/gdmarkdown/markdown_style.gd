@tool
class_name MarkdownStyle
extends Resource
## Visual settings a [MarkdownLabel] applies while turning Markdown into BBCode.
##
## The defaults render legibly against Godot's default dark UI without any
## configuration. Saving a configured instance as a [code].tres[/code] lets
## several labels share one appearance; changing any property emits
## [signal Resource.changed], so every label using it re-renders.

## Font sizes for heading levels 1 through 6.
@export var heading_font_sizes: PackedInt32Array = PackedInt32Array([32, 28, 24, 20, 18, 16]):
	set(value):
		heading_font_sizes = value
		emit_changed()

## Whether headings render bold.
@export var heading_bold: bool = true:
	set(value):
		heading_bold = value
		emit_changed()

## Background behind code spans and fenced code blocks.
@export var code_background_color: Color = Color(0.13, 0.14, 0.17, 0.75):
	set(value):
		code_background_color = value
		emit_changed()

## Text color of code spans and fenced code blocks.
@export var code_text_color: Color = Color(0.91, 0.76, 0.58):
	set(value):
		code_text_color = value
		emit_changed()

## Text color of block quotes and thematic breaks.
@export var quote_color: Color = Color(0.67, 0.71, 0.78):
	set(value):
		quote_color = value
		emit_changed()

## Text color of links, applied only when [member color_links] is enabled.
@export var link_color: Color = Color(0.45, 0.66, 1.0):
	set(value):
		link_color = value
		emit_changed()

## Whether links are colored. Disable it to inherit the label's own theme.
@export var color_links: bool = true:
	set(value):
		color_links = value
		emit_changed()

## Bullet drawn in front of unordered list items.
@export var unordered_bullet: String = "•":
	set(value):
		unordered_bullet = value
		emit_changed()

## Glyph drawn in front of an unchecked task list item.
@export var task_unchecked_glyph: String = "☐":
	set(value):
		task_unchecked_glyph = value
		emit_changed()

## Glyph drawn in front of a checked task list item.
@export var task_checked_glyph: String = "☑":
	set(value):
		task_checked_glyph = value
		emit_changed()

## Text drawn for a thematic break.
@export var thematic_break_text: String = "──────────":
	set(value):
		thematic_break_text = value
		emit_changed()

## Whether a single newline inside a paragraph is kept as a line break. Markdown
## treats it as a space, which is the default.
@export var preserve_soft_breaks: bool = false:
	set(value):
		preserve_soft_breaks = value
		emit_changed()

## Whether a fenced or indented code block is drawn as a solid rectangle. The
## rectangle is a single-cell BBCode table, which does not wrap long lines;
## turning this off falls back to a background that follows the text and wraps,
## at the cost of a ragged right edge.
@export var solid_code_background: bool = true:
	set(value):
		solid_code_background = value
		emit_changed()

## Padding inside a solid code block, in pixels.
@export_range(0, 64, 1) var code_block_padding: int = 10:
	set(value):
		code_block_padding = maxi(0, value)
		emit_changed()

## Whether a fenced code block's info string is shown above the block.
@export var show_code_language: bool = false:
	set(value):
		show_code_language = value
		emit_changed()

## Width images are scaled to, in pixels. Zero keeps their natural size.
@export_range(0, 4096, 1) var image_width: int = 0:
	set(value):
		image_width = maxi(0, value)
		emit_changed()

## Padding inside a table cell, in pixels: x horizontally, y vertically.
@export var table_cell_padding: Vector2i = Vector2i(10, 4):
	set(value):
		table_cell_padding = Vector2i(maxi(0, value.x), maxi(0, value.y))
		emit_changed()

## Whether table cells render inside a BBCode table. Disable it to render rows as
## plain lines, which reflows better in a narrow label.
@export var render_tables: bool = true:
	set(value):
		render_tables = value
		emit_changed()


## Returns the font size configured for [param level], clamped to the sizes the
## style defines.
func heading_font_size(level: int) -> int:
	if heading_font_sizes.is_empty():
		return 16
	var index: int = clampi(level - 1, 0, heading_font_sizes.size() - 1)
	return heading_font_sizes[index]
