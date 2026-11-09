class_name MarkdownBlockParser
extends RefCounted
## Builds the block structure of a Markdown document.
##
## Leaf blocks keep their raw Markdown in [member MarkdownNode.text]; turning
## that text into inline children is [MarkdownInlineParser]'s job. Container
## blocks are parsed by stripping their own marker and recursing, which is what
## makes arbitrary nesting (a task list inside a blockquote inside a list item)
## fall out without special cases.

const _MAX_HEADING_LEVEL: int = 6
const _CODE_BLOCK_INDENT: int = 4
const _MIN_FENCE_LENGTH: int = 3
const _MIN_THEMATIC_BREAK_LENGTH: int = 3

var _document: MarkdownDocument


## Parses [param markdown_text] into a document whose leaf blocks still hold raw
## inline Markdown.
static func parse(markdown_text: String) -> MarkdownDocument:
	var document: MarkdownDocument = MarkdownDocument.new()
	var parser: MarkdownBlockParser = MarkdownBlockParser.new(document)
	var lines: Array[String] = MarkdownScanner.split_lines(markdown_text)
	document.root.add_child_nodes(parser.parse_sequence(lines))
	return document


func _init(document: MarkdownDocument) -> void:
	_document = document


## Parses a sequence of already de-indented lines into sibling blocks.
func parse_sequence(lines: Array[String]) -> Array[MarkdownNode]:
	var nodes: Array[MarkdownNode] = []
	var index: int = 0
	while index < lines.size():
		var line: String = lines[index]
		if MarkdownScanner.is_blank(line):
			index += 1
			continue
		index = _parse_one_block(lines, index, nodes)
	return nodes


func _parse_one_block(lines: Array[String], start: int, nodes: Array[MarkdownNode]) -> int:
	var line: String = lines[start]
	var indent: int = MarkdownScanner.leading_indent(line)
	if indent >= _CODE_BLOCK_INDENT:
		return _parse_indented_code(lines, start, nodes)

	var body: String = line.substr(indent)
	if _fence_length(body) > 0:
		return _parse_fenced_code(lines, start, nodes)
	if _is_thematic_break(body):
		nodes.append(MarkdownNode.new(MarkdownNode.Kind.THEMATIC_BREAK))
		return start + 1
	var heading: MarkdownNode = _match_atx_heading(body)
	if heading != null:
		nodes.append(heading)
		return start + 1
	if body.begins_with(">"):
		return _parse_block_quote(lines, start, nodes)
	if _match_list_marker(body) != null:
		return _parse_list(lines, start, nodes)
	if _is_table_start(lines, start):
		return _parse_table(lines, start, nodes)
	var after_definition: int = _consume_link_reference_definitions(lines, start)
	if after_definition > start:
		return after_definition
	return _parse_paragraph(lines, start, nodes)


# --- Leaf blocks -------------------------------------------------------------


func _parse_paragraph(lines: Array[String], start: int, nodes: Array[MarkdownNode]) -> int:
	var collected: Array[String] = []
	var index: int = start
	while index < lines.size():
		var line: String = lines[index]
		if MarkdownScanner.is_blank(line):
			break
		var body: String = line.substr(MarkdownScanner.leading_indent(line))
		if not collected.is_empty():
			var level: int = _setext_level(body)
			if level > 0:
				var heading: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.HEADING)
				heading.heading_level = level
				heading.text = MarkdownScanner.join("\n", collected)
				nodes.append(heading)
				return index + 1
			if _interrupts_paragraph(lines, index):
				break
		collected.append(line.lstrip(" \t"))
		index += 1
	if collected.is_empty():
		return start + 1
	var paragraph: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.PARAGRAPH)
	paragraph.text = MarkdownScanner.join("\n", collected)
	nodes.append(paragraph)
	return index


func _parse_indented_code(lines: Array[String], start: int, nodes: Array[MarkdownNode]) -> int:
	var collected: Array[String] = []
	var index: int = start
	var last_content: int = start
	while index < lines.size():
		var line: String = lines[index]
		if MarkdownScanner.is_blank(line):
			collected.append("")
			index += 1
			continue
		if MarkdownScanner.leading_indent(line) < _CODE_BLOCK_INDENT:
			break
		collected.append(MarkdownScanner.strip_indent(line, _CODE_BLOCK_INDENT))
		index += 1
		last_content = index
	var code: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.CODE_BLOCK)
	code.text = MarkdownScanner.join("\n", collected.slice(0, last_content - start))
	nodes.append(code)
	return last_content


func _parse_fenced_code(lines: Array[String], start: int, nodes: Array[MarkdownNode]) -> int:
	var opening: String = lines[start]
	var indent: int = MarkdownScanner.leading_indent(opening)
	var body: String = opening.substr(indent)
	var fence_character: String = body.substr(0, 1)
	var fence_length: int = _fence_length(body)
	var info: String = body.substr(fence_length).strip_edges()

	var collected: Array[String] = []
	var index: int = start + 1
	while index < lines.size():
		var line: String = lines[index]
		var line_body: String = line.substr(MarkdownScanner.leading_indent(line))
		if _is_closing_fence(line_body, fence_character, fence_length):
			index += 1
			break
		collected.append(MarkdownScanner.strip_indent(line, indent))
		index += 1

	var code: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.CODE_BLOCK)
	code.text = MarkdownScanner.join("\n", collected)
	code.code_language = _info_language(info)
	nodes.append(code)
	return index


# --- Container blocks --------------------------------------------------------


func _parse_block_quote(lines: Array[String], start: int, nodes: Array[MarkdownNode]) -> int:
	var inner: Array[String] = []
	var index: int = start
	while index < lines.size():
		var line: String = lines[index]
		if MarkdownScanner.is_blank(line):
			break
		var body: String = line.substr(MarkdownScanner.leading_indent(line))
		if body.begins_with(">"):
			var stripped: String = body.substr(1)
			if stripped.begins_with(" "):
				stripped = stripped.substr(1)
			inner.append(stripped)
			index += 1
			continue
		if _starts_block(body):
			break
		inner.append(line.lstrip(" \t"))
		index += 1
	var quote: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.BLOCK_QUOTE)
	quote.add_child_nodes(parse_sequence(inner))
	nodes.append(quote)
	return index


func _parse_list(lines: Array[String], start: int, nodes: Array[MarkdownNode]) -> int:
	var first_marker: _ListMarker = _match_list_marker(
			lines[start].substr(MarkdownScanner.leading_indent(lines[start]))
	)
	var list: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.LIST)
	list.list_ordered = first_marker.is_ordered
	list.list_start = first_marker.number
	var tight: bool = true
	var index: int = start

	while index < lines.size():
		if MarkdownScanner.is_blank(lines[index]):
			var resumed: int = _next_item_after_blanks(lines, index, first_marker)
			if resumed < 0:
				break
			tight = false
			index = resumed
			continue
		var indent: int = MarkdownScanner.leading_indent(lines[index])
		if indent >= _CODE_BLOCK_INDENT:
			break
		var marker: _ListMarker = _match_list_marker(lines[index].substr(indent))
		if marker == null or not _same_list_type(first_marker, marker):
			break
		var parsed: _ItemResult = _parse_list_item(lines, index, indent, marker)
		index = parsed.next_index
		if parsed.had_blank:
			tight = false
		list.add_child_node(parsed.item)

	list.list_tight = tight
	nodes.append(list)
	return index


## Parses one list item starting at [param start], reporting the item, the index
## after it, and whether it held a blank line (which makes the whole list loose).
func _parse_list_item(
		lines: Array[String],
		start: int,
		indent: int,
		marker: _ListMarker
) -> _ItemResult:
	var content_indent: int = indent + marker.content_offset
	var collected: Array[String] = []
	collected.append(lines[start].substr(mini(content_indent, lines[start].length())))
	var index: int = start + 1
	var had_blank: bool = false

	while index < lines.size():
		var line: String = lines[index]
		if MarkdownScanner.is_blank(line):
			var resumed: int = _skip_blanks_into_item(lines, index, content_indent, collected)
			if resumed < 0:
				break
			had_blank = true
			index = resumed
			continue
		if MarkdownScanner.leading_indent(line) >= content_indent:
			collected.append(MarkdownScanner.strip_indent(line, content_indent))
			index += 1
			continue
		var body: String = line.substr(MarkdownScanner.leading_indent(line))
		if had_blank or _starts_block(body) or _match_list_marker(body) != null:
			break
		collected.append(line.lstrip(" \t"))
		index += 1

	var item: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.LIST_ITEM)
	item.task_state = _take_task_state(collected)
	item.add_child_nodes(parse_sequence(collected))

	var result: _ItemResult = _ItemResult.new()
	result.item = item
	result.next_index = index
	result.had_blank = had_blank
	return result


## Returns the index of the next item of the same list after a run of blank
## lines, or -1 when the list ends there.
func _next_item_after_blanks(lines: Array[String], start: int, first: _ListMarker) -> int:
	var index: int = start
	while index < lines.size() and MarkdownScanner.is_blank(lines[index]):
		index += 1
	if index >= lines.size():
		return -1
	var indent: int = MarkdownScanner.leading_indent(lines[index])
	if indent >= _CODE_BLOCK_INDENT:
		return -1
	var marker: _ListMarker = _match_list_marker(lines[index].substr(indent))
	if marker == null or not _same_list_type(first, marker):
		return -1
	return index


## Appends the blank lines starting at [param start] to [param collected] when
## indented content follows them, and returns the index of that content.
## Returns -1 when the blank run ends the item.
func _skip_blanks_into_item(
		lines: Array[String],
		start: int,
		content_indent: int,
		collected: Array[String]
) -> int:
	var index: int = start
	while index < lines.size() and MarkdownScanner.is_blank(lines[index]):
		index += 1
	if index >= lines.size() or MarkdownScanner.leading_indent(lines[index]) < content_indent:
		return -1
	var blank: int = start
	while blank < index:
		collected.append("")
		blank += 1
	return index


# --- Tables ------------------------------------------------------------------


func _parse_table(lines: Array[String], start: int, nodes: Array[MarkdownNode]) -> int:
	var header_cells: Array[String] = _split_table_row(lines[start])
	var table: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.TABLE)
	table.column_alignments = _parse_table_alignments(lines[start + 1])
	table.add_child_node(_build_table_row(header_cells, header_cells.size()))

	var index: int = start + 2
	while index < lines.size():
		var line: String = lines[index]
		if MarkdownScanner.is_blank(line) or not line.contains("|"):
			break
		table.add_child_node(_build_table_row(_split_table_row(line), header_cells.size()))
		index += 1
	nodes.append(table)
	return index


func _build_table_row(cells: Array[String], column_count: int) -> MarkdownNode:
	var row: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.TABLE_ROW)
	var column: int = 0
	while column < column_count:
		var cell: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.TABLE_CELL)
		if column < cells.size():
			cell.text = cells[column]
		row.add_child_node(cell)
		column += 1
	return row


func _split_table_row(line: String) -> Array[String]:
	var trimmed: String = line.strip_edges()
	if trimmed.begins_with("|"):
		trimmed = trimmed.substr(1)
	if trimmed.ends_with("|") and not trimmed.ends_with("\\|"):
		trimmed = trimmed.substr(0, trimmed.length() - 1)
	var cells: Array[String] = []
	var current: String = ""
	var index: int = 0
	while index < trimmed.length():
		var character: String = trimmed.substr(index, 1)
		if character == "\\" and index + 1 < trimmed.length():
			var escaped: String = trimmed.substr(index + 1, 1)
			if escaped == "|":
				current += "|"
				index += 2
				continue
			current += character
			index += 1
			continue
		if character == "|":
			cells.append(current.strip_edges())
			current = ""
			index += 1
			continue
		current += character
		index += 1
	cells.append(current.strip_edges())
	return cells


func _parse_table_alignments(delimiter_line: String) -> Array[int]:
	var alignments: Array[int] = []
	for cell: String in _split_table_row(delimiter_line):
		var starts: bool = cell.begins_with(":")
		var ends: bool = cell.ends_with(":")
		if starts and ends:
			alignments.append(MarkdownNode.Alignment.CENTER)
		elif ends:
			alignments.append(MarkdownNode.Alignment.RIGHT)
		elif starts:
			alignments.append(MarkdownNode.Alignment.LEFT)
		else:
			alignments.append(MarkdownNode.Alignment.NONE)
	return alignments


func _is_table_start(lines: Array[String], start: int) -> bool:
	if start + 1 >= lines.size():
		return false
	if not lines[start].contains("|"):
		return false
	var header_count: int = _split_table_row(lines[start]).size()
	var delimiters: Array[String] = _split_table_row(lines[start + 1])
	if delimiters.size() != header_count or header_count == 0:
		return false
	for cell: String in delimiters:
		if not _is_table_delimiter_cell(cell):
			return false
	return true


func _is_table_delimiter_cell(cell: String) -> bool:
	var inner: String = cell.trim_prefix(":").trim_suffix(":")
	if inner.is_empty():
		return false
	return inner.count("-") == inner.length()


# --- Link reference definitions ----------------------------------------------


## Consumes every link reference definition starting at [param start], records
## them on the document, and returns the index after the last one. Returns
## [param start] when the line is not a definition.
func _consume_link_reference_definitions(lines: Array[String], start: int) -> int:
	var index: int = start
	while index < lines.size():
		var line: String = lines[index]
		if MarkdownScanner.leading_indent(line) >= _CODE_BLOCK_INDENT:
			break
		var body: String = line.substr(MarkdownScanner.leading_indent(line))
		if not body.begins_with("["):
			break
		var label_end: int = _find_label_end(body, 0)
		if label_end < 0 or not body.substr(label_end + 1).begins_with(":"):
			break
		var label: String = body.substr(1, label_end - 1)
		var rest: String = body.substr(label_end + 2).strip_edges()
		if rest.is_empty():
			break
		var destination: String = ""
		var title: String = ""
		var space: int = rest.find(" ")
		if space < 0:
			destination = rest
		else:
			destination = rest.substr(0, space)
			title = _unquote_title(rest.substr(space + 1).strip_edges())
		_document.add_link_reference(label, _unwrap_destination(destination), title)
		index += 1
	return index


func _unquote_title(raw: String) -> String:
	if raw.length() < 2:
		return ""
	var opener: String = raw.substr(0, 1)
	var closer: String = raw.substr(raw.length() - 1, 1)
	var matched: bool = (
			opener == '"' and closer == '"'
			or opener == "'" and closer == "'"
			or opener == "(" and closer == ")"
	)
	if not matched:
		return ""
	return raw.substr(1, raw.length() - 2)


func _unwrap_destination(raw: String) -> String:
	if raw.begins_with("<") and raw.ends_with(">"):
		return raw.substr(1, raw.length() - 2)
	return raw


## Returns the index of the [code]][/code] closing the label that starts at
## [param open], honoring backslash escapes and nested brackets, or -1.
func _find_label_end(text: String, open: int) -> int:
	var depth: int = 0
	var index: int = open
	while index < text.length():
		var character: String = text.substr(index, 1)
		if character == "\\":
			index += 2
			continue
		if character == "[":
			depth += 1
		elif character == "]":
			depth -= 1
			if depth == 0:
				return index
		index += 1
	return -1


# --- Line classification -----------------------------------------------------


func _starts_block(body: String) -> bool:
	return (
			_fence_length(body) > 0
			or _is_thematic_break(body)
			or _match_atx_heading(body) != null
			or body.begins_with(">")
	)


func _interrupts_paragraph(lines: Array[String], index: int) -> bool:
	var body: String = lines[index].substr(MarkdownScanner.leading_indent(lines[index]))
	if _starts_block(body) or _is_table_start(lines, index):
		return true
	var marker: _ListMarker = _match_list_marker(body)
	if marker == null:
		return false
	# An ordered list only interrupts a paragraph when it starts at 1, so that a
	# wrapped line beginning with a year ("2024. A good year") stays prose.
	return not marker.is_ordered or marker.number == 1


func _is_thematic_break(body: String) -> bool:
	var compact: String = body.replace(" ", "").replace("\t", "")
	if compact.length() < _MIN_THEMATIC_BREAK_LENGTH:
		return false
	var character: String = compact.substr(0, 1)
	if character != "-" and character != "_" and character != "*":
		return false
	return compact.count(character) == compact.length()


func _setext_level(body: String) -> int:
	var compact: String = body.strip_edges()
	if compact.is_empty():
		return 0
	var character: String = compact.substr(0, 1)
	if character != "=" and character != "-":
		return 0
	if compact.count(character) != compact.length():
		return 0
	return 1 if character == "=" else 2


func _match_atx_heading(body: String) -> MarkdownNode:
	var level: int = 0
	while level < body.length() and body.substr(level, 1) == "#":
		level += 1
	if level == 0 or level > _MAX_HEADING_LEVEL:
		return null
	var rest: String = body.substr(level)
	if not rest.is_empty() and not rest.begins_with(" ") and not rest.begins_with("\t"):
		return null
	var heading: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.HEADING)
	heading.heading_level = level
	heading.text = _strip_closing_hashes(rest.strip_edges())
	return heading


func _strip_closing_hashes(content: String) -> String:
	var end: int = content.length()
	while end > 0 and content.substr(end - 1, 1) == "#":
		end -= 1
	if end == content.length():
		return content
	if end == 0:
		return ""
	if content.substr(end - 1, 1) != " ":
		return content
	return content.substr(0, end).strip_edges()


func _fence_length(body: String) -> int:
	var character: String = body.substr(0, 1)
	if character != "`" and character != "~":
		return 0
	var length: int = 0
	while length < body.length() and body.substr(length, 1) == character:
		length += 1
	if length < _MIN_FENCE_LENGTH:
		return 0
	if character == "`" and body.substr(length).contains("`"):
		return 0
	return length


func _is_closing_fence(body: String, character: String, opening_length: int) -> bool:
	var length: int = 0
	while length < body.length() and body.substr(length, 1) == character:
		length += 1
	if length < opening_length:
		return false
	return body.substr(length).strip_edges().is_empty()


func _info_language(info: String) -> String:
	var space: int = info.find(" ")
	if space < 0:
		return info
	return info.substr(0, space)


# --- List markers ------------------------------------------------------------


func _match_list_marker(body: String) -> _ListMarker:
	var first: String = body.substr(0, 1)
	if first == "-" or first == "+" or first == "*":
		return _build_bullet_marker(body, first)
	if MarkdownScanner.is_digit(first):
		return _build_ordered_marker(body)
	return null


func _build_bullet_marker(body: String, bullet: String) -> _ListMarker:
	var following: String = body.substr(1, 1)
	if not following.is_empty() and following != " " and following != "\t":
		return null
	var marker: _ListMarker = _ListMarker.new()
	marker.bullet = bullet
	marker.content_offset = 1 + _space_run(body, 1)
	return marker


func _build_ordered_marker(body: String) -> _ListMarker:
	var digits: int = 0
	while digits < body.length() and MarkdownScanner.is_digit(body.substr(digits, 1)):
		digits += 1
	if digits == 0 or digits > 9:
		return null
	var delimiter: String = body.substr(digits, 1)
	if delimiter != "." and delimiter != ")":
		return null
	var following: String = body.substr(digits + 1, 1)
	if not following.is_empty() and following != " " and following != "\t":
		return null
	var marker: _ListMarker = _ListMarker.new()
	marker.is_ordered = true
	marker.delimiter = delimiter
	marker.number = body.substr(0, digits).to_int()
	marker.content_offset = digits + 1 + _space_run(body, digits + 1)
	return marker


## Returns how many spaces follow [param from], clamped so an indented code
## block inside a list item still de-indents against the marker's own column.
func _space_run(body: String, from: int) -> int:
	var spaces: int = 0
	while from + spaces < body.length() and body.substr(from + spaces, 1) == " ":
		spaces += 1
	if spaces == 0 or spaces > _CODE_BLOCK_INDENT:
		return 1
	return spaces


func _same_list_type(first: _ListMarker, other: _ListMarker) -> bool:
	if first.is_ordered != other.is_ordered:
		return false
	if first.is_ordered:
		return first.delimiter == other.delimiter
	return first.bullet == other.bullet


## Removes a GitHub Flavored Markdown task marker from the first line of
## [param collected] and returns the state it encoded.
func _take_task_state(collected: Array[String]) -> MarkdownNode.TaskState:
	if collected.is_empty():
		return MarkdownNode.TaskState.NONE
	var first: String = collected[0]
	if first.length() < 3 or not first.begins_with("["):
		return MarkdownNode.TaskState.NONE
	if first.substr(2, 1) != "]":
		return MarkdownNode.TaskState.NONE
	var rest: String = first.substr(3)
	if not rest.is_empty() and not rest.begins_with(" ") and not rest.begins_with("\t"):
		return MarkdownNode.TaskState.NONE
	var box: String = first.substr(1, 1)
	if box == " ":
		collected[0] = rest.lstrip(" \t")
		return MarkdownNode.TaskState.UNCHECKED
	if box == "x" or box == "X":
		collected[0] = rest.lstrip(" \t")
		return MarkdownNode.TaskState.CHECKED
	return MarkdownNode.TaskState.NONE


class _ListMarker:
	extends RefCounted

	var is_ordered: bool = false
	var number: int = 1
	var delimiter: String = ""
	var bullet: String = ""
	var content_offset: int = 0


class _ItemResult:
	extends RefCounted

	var item: MarkdownNode = null
	var next_index: int = 0
	var had_blank: bool = false
