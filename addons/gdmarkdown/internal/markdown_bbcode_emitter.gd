class_name MarkdownBbcodeEmitter
extends RefCounted
## Renders a parsed Markdown document as BBCode for a [RichTextLabel].
##
## Lists are rendered with explicit markers and [code][indent][/code] rather
## than with BBCode's own [code][ul][/code] and [code][ol][/code] tags, because
## those split items on newlines and so cannot carry nested lists, task glyphs,
## or an ordered list that starts at something other than one.

const _BLOCK_SEPARATOR: String = "\n\n"

var _style: MarkdownStyle


## Renders [param document] using [param style].
static func emit(document: MarkdownDocument, style: MarkdownStyle) -> String:
	var emitter: MarkdownBbcodeEmitter = MarkdownBbcodeEmitter.new(style)
	return emitter.emit_blocks(document.root.children)


## Escapes the one character BBCode treats as markup.
static func escape(text: String) -> String:
	return text.replace("[", "[lb]")


## Encodes a destination for use inside a BBCode tag attribute, where a space
## would end the attribute and a bracket would end the tag.
static func encode_destination(destination: String) -> String:
	return destination.replace(
			"%",
			"%25"
	).replace(" ", "%20").replace("[", "%5B").replace("]", "%5D")


func _init(style: MarkdownStyle) -> void:
	_style = style if style != null else MarkdownStyle.new()


## Renders a sequence of sibling blocks, separated by one blank line.
func emit_blocks(nodes: Array[MarkdownNode]) -> String:
	var parts: Array[String] = []
	for node: MarkdownNode in nodes:
		var rendered: String = _emit_block(node)
		if not rendered.is_empty():
			parts.append(rendered)
	return MarkdownScanner.join(_BLOCK_SEPARATOR, parts)


## Renders a sequence of inline nodes.
func emit_inlines(nodes: Array[MarkdownNode]) -> String:
	var parts: Array[String] = []
	for node: MarkdownNode in nodes:
		parts.append(_emit_inline(node))
	return MarkdownScanner.join("", parts)


# --- Blocks ------------------------------------------------------------------


func _emit_block(node: MarkdownNode) -> String:
	match node.kind:
		MarkdownNode.Kind.PARAGRAPH:
			return emit_inlines(node.children)
		MarkdownNode.Kind.HEADING:
			return _emit_heading(node)
		MarkdownNode.Kind.CODE_BLOCK:
			return _emit_code_block(node)
		MarkdownNode.Kind.BLOCK_QUOTE:
			return _emit_block_quote(node)
		MarkdownNode.Kind.LIST:
			return _emit_list(node)
		MarkdownNode.Kind.THEMATIC_BREAK:
			return _emit_thematic_break()
		MarkdownNode.Kind.TABLE:
			return _emit_table(node)
	return emit_inlines(node.children)


func _emit_heading(node: MarkdownNode) -> String:
	var content: String = emit_inlines(node.children)
	if _style.heading_bold:
		content = "[b]%s[/b]" % content
	return "[font_size=%d]%s[/font_size]" % [_style.heading_font_size(node.heading_level), content]


func _emit_code_block(node: MarkdownNode) -> String:
	var body: String = _code_block_body(escape(node.text))
	if _style.show_code_language and not node.code_language.is_empty():
		var label: String = "[color=%s][i]%s[/i][/color]" % [
			_color(_style.quote_color),
			escape(node.code_language),
		]
		return "%s\n%s" % [label, body]
	return body


## Wraps a code block's text in its background. A single-cell table is used
## rather than a background tag because a background tag only covers the glyphs
## it wraps, which leaves a code block with one ragged edge; a cell is a true
## rectangle and takes real padding.
func _code_block_body(escaped: String) -> String:
	var background: String = _color(_style.code_background_color)
	var foreground: String = _color(_style.code_text_color)
	if not _style.solid_code_background:
		return "[bgcolor=%s][color=%s][code]%s[/code][/color][/bgcolor]" % [
			background,
			foreground,
			escaped,
		]
	var padding: int = _style.code_block_padding
	return "[table=1][cell bg=%s,%s padding=%d,%d,%d,%d][color=%s][code]%s[/code][/color][/cell][/table]" % [
		background,
		background,
		padding,
		padding,
		padding,
		padding,
		foreground,
		escaped,
	]


func _emit_block_quote(node: MarkdownNode) -> String:
	return "[indent][color=%s]%s[/color][/indent]" % [
		_color(_style.quote_color),
		emit_blocks(node.children),
	]


func _emit_thematic_break() -> String:
	return "[center][color=%s]%s[/color][/center]" % [
		_color(_style.quote_color),
		escape(_style.thematic_break_text),
	]


func _emit_list(node: MarkdownNode) -> String:
	var lines: Array[String] = []
	var number: int = node.list_start
	for item: MarkdownNode in node.children:
		lines.append(
				"[indent]%s %s[/indent]" % [_item_marker(node, item, number), _emit_item(item)]
		)
		number += 1
	var separator: String = "\n" if node.list_tight else _BLOCK_SEPARATOR
	return MarkdownScanner.join(separator, lines)


func _item_marker(list: MarkdownNode, item: MarkdownNode, number: int) -> String:
	match item.task_state:
		MarkdownNode.TaskState.UNCHECKED:
			return escape(_style.task_unchecked_glyph)
		MarkdownNode.TaskState.CHECKED:
			return escape(_style.task_checked_glyph)
	if list.list_ordered:
		return "%d." % number
	return escape(_style.unordered_bullet)


## Renders the blocks inside a list item. A paragraph contributes only its
## inline content, so a tight item stays on the marker's line.
func _emit_item(item: MarkdownNode) -> String:
	var parts: Array[String] = []
	for child: MarkdownNode in item.children:
		if child.kind == MarkdownNode.Kind.PARAGRAPH:
			parts.append(emit_inlines(child.children))
		else:
			parts.append(_emit_block(child))
	return MarkdownScanner.join("\n", parts)


# --- Tables ------------------------------------------------------------------


func _emit_table(node: MarkdownNode) -> String:
	if node.children.is_empty():
		return ""
	var column_count: int = node.children[0].children.size()
	if not _style.render_tables:
		return _emit_plain_table(node)
	var parts: Array[String] = []
	parts.append("[table=%d]" % maxi(1, column_count))
	var is_header: bool = true
	for row: MarkdownNode in node.children:
		parts.append(_emit_table_row(node, row, is_header))
		is_header = false
	parts.append("[/table]")
	return MarkdownScanner.join("", parts)


func _emit_table_row(table: MarkdownNode, row: MarkdownNode, is_header: bool) -> String:
	var parts: Array[String] = []
	var column: int = 0
	for cell: MarkdownNode in row.children:
		var content: String = emit_inlines(cell.children)
		if is_header:
			content = "[b]%s[/b]" % content
		parts.append("[cell %s]%s[/cell]" % [_cell_padding(), _align(table, column, content)])
		column += 1
	return MarkdownScanner.join("", parts)


func _cell_padding() -> String:
	var padding: Vector2i = _style.table_cell_padding
	return "padding=%d,%d,%d,%d" % [padding.x, padding.y, padding.x, padding.y]


func _emit_plain_table(node: MarkdownNode) -> String:
	var lines: Array[String] = []
	var is_header: bool = true
	for row: MarkdownNode in node.children:
		var cells: Array[String] = []
		for cell: MarkdownNode in row.children:
			var content: String = emit_inlines(cell.children)
			cells.append("[b]%s[/b]" % content if is_header else content)
		lines.append(MarkdownScanner.join(" | ", cells))
		is_header = false
	return MarkdownScanner.join("\n", lines)


func _align(table: MarkdownNode, column: int, content: String) -> String:
	if column >= table.column_alignments.size():
		return content
	match table.column_alignments[column]:
		MarkdownNode.Alignment.CENTER:
			return "[center]%s[/center]" % content
		MarkdownNode.Alignment.RIGHT:
			return "[right]%s[/right]" % content
	return content


# --- Inlines -----------------------------------------------------------------


func _emit_inline(node: MarkdownNode) -> String:
	match node.kind:
		MarkdownNode.Kind.TEXT:
			return escape(node.text)
		MarkdownNode.Kind.EMPHASIS:
			return "[i]%s[/i]" % emit_inlines(node.children)
		MarkdownNode.Kind.STRONG:
			return "[b]%s[/b]" % emit_inlines(node.children)
		MarkdownNode.Kind.STRIKETHROUGH:
			return "[s]%s[/s]" % emit_inlines(node.children)
		MarkdownNode.Kind.CODE_SPAN:
			return "[bgcolor=%s][color=%s][code]%s[/code][/color][/bgcolor]" % [
				_color(_style.code_background_color),
				_color(_style.code_text_color),
				escape(node.text),
			]
		MarkdownNode.Kind.LINK:
			return _emit_link(node)
		MarkdownNode.Kind.IMAGE:
			return _emit_image(node)
		MarkdownNode.Kind.LINE_BREAK:
			return "\n"
		MarkdownNode.Kind.SOFT_BREAK:
			return "\n" if _style.preserve_soft_breaks else " "
	return emit_inlines(node.children)


func _emit_link(node: MarkdownNode) -> String:
	var content: String = emit_inlines(node.children)
	if _style.color_links:
		content = "[color=%s]%s[/color]" % [_color(_style.link_color), content]
	if not node.link_title.is_empty():
		content = "[hint=%s]%s[/hint]" % [escape(node.link_title), content]
	return "[url=%s]%s[/url]" % [encode_destination(node.link_destination), content]


## Renders an image. Only a path the project can actually load becomes an
## [code][img][/code]. A remote URL keeps its alternate text and links out,
## since the addon ships no HTTP client; a local path that does not resolve
## falls back to alternate text alone, so nothing is silently dropped.
func _emit_image(node: MarkdownNode) -> String:
	var alternate: String = node.text if not node.text.is_empty() else node.link_destination
	if _is_local_path(node.link_destination):
		if not ResourceLoader.exists(node.link_destination):
			return escape(alternate)
		if _style.image_width > 0:
			return "[img=%d]%s[/img]" % [_style.image_width, node.link_destination]
		return "[img]%s[/img]" % node.link_destination
	if node.link_destination.contains("://"):
		return "[url=%s]%s[/url]" % [encode_destination(node.link_destination), escape(alternate)]
	return escape(alternate)


func _is_local_path(path: String) -> bool:
	return path.begins_with("res://") or path.begins_with("user://")


func _color(color: Color) -> String:
	return "#" + color.to_html(true)
