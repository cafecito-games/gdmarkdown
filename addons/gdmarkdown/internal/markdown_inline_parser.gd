class_name MarkdownInlineParser
extends RefCounted
## Expands the raw inline Markdown left on leaf blocks into inline child nodes.
##
## The scan follows CommonMark's precedence: backslash escapes and code spans
## bind tightest, then autolinks, then links and images, then emphasis runs.
## Emphasis is resolved by searching forward for a matching delimiter run while
## counting nested openers, rather than by CommonMark's delimiter stack. That
## resolves every ordinary nesting, including [code]***both***[/code], and
## differs only on pathological overlapping runs such as
## [code]*foo**bar*baz**[/code], which it renders as literal text instead of
## guessing.

const _MAX_DELIMITER_LENGTH: int = 2
const _HARD_BREAK_SPACES: int = 2

const _ENTITIES: Dictionary[String, String] = {
	"amp": "&",
	"apos": "'",
	"asymp": "≈",
	"bull": "•",
	"cent": "¢",
	"copy": "©",
	"dagger": "†",
	"darr": "↓",
	"deg": "°",
	"divide": "÷",
	"euro": "€",
	"ge": "≥",
	"gt": ">",
	"harr": "↔",
	"hellip": "…",
	"infin": "∞",
	"laquo": "«",
	"larr": "←",
	"ldquo": "“",
	"le": "≤",
	"lsquo": "‘",
	"lt": "<",
	"mdash": "—",
	"middot": "·",
	"nbsp": " ",
	"ndash": "–",
	"ne": "≠",
	"para": "¶",
	"plusmn": "±",
	"pound": "£",
	"quot": '"',
	"raquo": "»",
	"rarr": "→",
	"rdquo": "”",
	"reg": "®",
	"rsquo": "’",
	"sect": "§",
	"times": "×",
	"trade": "™",
	"uarr": "↑",
	"yen": "¥",
}

const _AUTOLINK_PREFIXES: Array[String] = ["https://", "http://", "www."]
const _AUTOLINK_TRAILING: String = ".,:;!?'\""

var _document: MarkdownDocument

## Pending text buffer handed back by the `_take_*` helpers. GDScript has no out
## parameters, so a helper that changes the buffer writes it here and the scan
## picks it up after a non-zero consume count.
var _pending: String = ""


## Expands every leaf block in [param document] in place.
static func parse_document(document: MarkdownDocument) -> void:
	var parser: MarkdownInlineParser = MarkdownInlineParser.new(document)
	parser._expand(document.root)


## Parses [param raw] into inline nodes, resolving references against
## [param document].
static func parse_text(raw: String, document: MarkdownDocument) -> Array[MarkdownNode]:
	var parser: MarkdownInlineParser = MarkdownInlineParser.new(document)
	return parser._parse_range(raw, 0, raw.length())


func _init(document: MarkdownDocument) -> void:
	_document = document


func _expand(node: MarkdownNode) -> void:
	match node.kind:
		MarkdownNode.Kind.PARAGRAPH, MarkdownNode.Kind.HEADING, MarkdownNode.Kind.TABLE_CELL:
			if not node.text.is_empty():
				node.add_child_nodes(_parse_range(node.text, 0, node.text.length()))
				node.text = ""
			return
		MarkdownNode.Kind.CODE_BLOCK:
			return
	for child: MarkdownNode in node.children:
		_expand(child)


# --- Main scan ---------------------------------------------------------------


func _parse_range(text: String, start: int, end: int) -> Array[MarkdownNode]:
	var nodes: Array[MarkdownNode] = []
	var buffer: String = ""
	var index: int = start
	while index < end:
		var character: String = text.substr(index, 1)
		var consumed: int = 0

		if character == "\\":
			consumed = _take_escape(text, index, end, nodes, buffer)
		elif character == "`":
			consumed = _take_code_span(text, index, end, nodes, buffer)
		elif character == "\n":
			consumed = _take_line_break(nodes, buffer)
		elif character == "&":
			consumed = _take_entity(text, index, end, buffer)
		elif character == "<":
			consumed = _take_angle_autolink(text, index, end, nodes, buffer)
		elif character == "!" or character == "[":
			consumed = _take_link(text, index, end, nodes, buffer)
		elif character == "*" or character == "_" or character == "~":
			consumed = _take_emphasis(text, index, end, nodes, buffer)
		elif character == "h" or character == "w":
			consumed = _take_bare_autolink(text, index, end, nodes, buffer)

		if consumed > 0:
			index += consumed
			buffer = _pending
			continue
		buffer += character
		index += 1

	_flush(nodes, buffer)
	return nodes


func _flush(nodes: Array[MarkdownNode], buffer: String) -> void:
	if buffer.is_empty():
		return
	nodes.append(MarkdownNode.new(MarkdownNode.Kind.TEXT, buffer))


func _emit(nodes: Array[MarkdownNode], buffer: String, node: MarkdownNode) -> void:
	_flush(nodes, buffer)
	_pending = ""
	nodes.append(node)


# --- Escapes, entities, breaks ----------------------------------------------


func _take_escape(
		text: String,
		index: int,
		end: int,
		nodes: Array[MarkdownNode],
		buffer: String
) -> int:
	if index + 1 >= end:
		return 0
	var next: String = text.substr(index + 1, 1)
	if next == "\n":
		_emit(nodes, buffer, MarkdownNode.new(MarkdownNode.Kind.LINE_BREAK))
		return 2
	if not MarkdownScanner.is_punctuation(next):
		return 0
	_pending = buffer + next
	return 2


func _take_entity(text: String, index: int, end: int, buffer: String) -> int:
	var semicolon: int = text.find(";", index)
	if semicolon < 0 or semicolon >= end or semicolon - index > 32:
		return 0
	var name: String = text.substr(index + 1, semicolon - index - 1)
	if name.is_empty():
		return 0
	var replacement: String = _resolve_entity(name)
	if replacement.is_empty():
		return 0
	_pending = buffer + replacement
	return semicolon - index + 1


func _resolve_entity(name: String) -> String:
	if _ENTITIES.has(name):
		return _ENTITIES[name]
	if not name.begins_with("#"):
		return ""
	var digits: String = name.substr(1)
	var code_point: int = 0
	if digits.begins_with("x") or digits.begins_with("X"):
		var hexadecimal: String = digits.substr(1)
		if not hexadecimal.is_valid_hex_number(false):
			return ""
		code_point = hexadecimal.hex_to_int()
	else:
		if not digits.is_valid_int():
			return ""
		code_point = digits.to_int()
	if code_point <= 0 or code_point > 0x10ffff:
		return ""
	return char(code_point)


func _take_line_break(nodes: Array[MarkdownNode], buffer: String) -> int:
	if buffer.ends_with(" ".repeat(_HARD_BREAK_SPACES)):
		_emit(nodes, buffer.rstrip(" "), MarkdownNode.new(MarkdownNode.Kind.LINE_BREAK))
		return 1
	_emit(nodes, buffer, MarkdownNode.new(MarkdownNode.Kind.SOFT_BREAK))
	return 1


# --- Code spans --------------------------------------------------------------


func _take_code_span(
		text: String,
		index: int,
		end: int,
		nodes: Array[MarkdownNode],
		buffer: String
) -> int:
	var opening: int = _run_length(text, index, end, "`")
	var content_start: int = index + opening
	var closing: int = _find_backtick_run(text, content_start, end, opening)
	if closing < 0:
		return 0
	var content: String = text.substr(content_start, closing - content_start)
	_emit(nodes, buffer, MarkdownNode.new(MarkdownNode.Kind.CODE_SPAN, _clean_code_span(content)))
	return closing + opening - index


func _clean_code_span(content: String) -> String:
	var collapsed: String = content.replace("\n", " ")
	var stripped: bool = (
			collapsed.length() > 2
			and collapsed.begins_with(" ")
			and collapsed.ends_with(" ")
			and not collapsed.strip_edges().is_empty()
	)
	if stripped:
		return collapsed.substr(1, collapsed.length() - 2)
	return collapsed


## Returns the start index of the first backtick run of exactly
## [param length] backticks at or after [param from], or -1.
func _find_backtick_run(text: String, from: int, end: int, length: int) -> int:
	var index: int = from
	while index < end:
		if text.substr(index, 1) != "`":
			index += 1
			continue
		var run: int = _run_length(text, index, end, "`")
		if run == length:
			return index
		index += run
	return -1


func _run_length(text: String, from: int, end: int, character: String) -> int:
	var length: int = 0
	while from + length < end and text.substr(from + length, 1) == character:
		length += 1
	return length


# --- Autolinks ---------------------------------------------------------------


func _take_angle_autolink(
		text: String,
		index: int,
		end: int,
		nodes: Array[MarkdownNode],
		buffer: String
) -> int:
	var closing: int = text.find(">", index)
	if closing < 0 or closing >= end:
		return 0
	var inner: String = text.substr(index + 1, closing - index - 1)
	if inner.is_empty() or inner.contains(" ") or inner.contains("<"):
		return 0
	var destination: String = ""
	if inner.contains("://"):
		destination = inner
	elif inner.contains("@") and not inner.contains("/"):
		destination = "mailto:" + inner
	else:
		return 0
	_emit(nodes, buffer, _build_link(destination, "", inner))
	return closing - index + 1


func _take_bare_autolink(
		text: String,
		index: int,
		end: int,
		nodes: Array[MarkdownNode],
		buffer: String
) -> int:
	if not buffer.is_empty():
		var previous: String = buffer.substr(buffer.length() - 1, 1)
		var boundary: bool = MarkdownScanner.is_whitespace(previous) or previous == "("
		if not boundary:
			return 0
	var matched_prefix: String = ""
	for prefix: String in _AUTOLINK_PREFIXES:
		if text.substr(index, prefix.length()) == prefix:
			matched_prefix = prefix
			break
	if matched_prefix.is_empty():
		return 0
	var stop: int = index
	while stop < end and not MarkdownScanner.is_whitespace(text.substr(stop, 1)):
		stop += 1
	var raw: String = text.substr(index, stop - index)
	while not raw.is_empty() and _AUTOLINK_TRAILING.contains(raw.substr(raw.length() - 1, 1)):
		raw = raw.substr(0, raw.length() - 1)
	if raw.ends_with(")") and raw.count("(") < raw.count(")"):
		raw = raw.substr(0, raw.length() - 1)
	if raw.length() <= matched_prefix.length():
		return 0
	var destination: String = raw if matched_prefix != "www." else "https://" + raw
	_emit(nodes, buffer, _build_link(destination, "", raw))
	return raw.length()


func _build_link(destination: String, title: String, label: String) -> MarkdownNode:
	var link: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.LINK)
	link.link_destination = destination
	link.link_title = title
	link.add_child_node(MarkdownNode.new(MarkdownNode.Kind.TEXT, label))
	return link


# --- Links and images --------------------------------------------------------


func _take_link(
		text: String,
		index: int,
		end: int,
		nodes: Array[MarkdownNode],
		buffer: String
) -> int:
	var is_image: bool = text.substr(index, 1) == "!"
	var bracket: int = index + 1 if is_image else index
	if bracket >= end or text.substr(bracket, 1) != "[":
		return 0
	var label_end: int = _find_bracket_close(text, bracket, end)
	if label_end < 0:
		return 0
	var label: String = text.substr(bracket + 1, label_end - bracket - 1)

	var target: _LinkTarget = _resolve_target(text, label_end + 1, end, label)
	if target == null:
		return 0

	var node: MarkdownNode = MarkdownNode.new(
			MarkdownNode.Kind.IMAGE if is_image else MarkdownNode.Kind.LINK
	)
	node.link_destination = target.destination
	node.link_title = target.title
	if is_image:
		node.text = label
	else:
		node.add_child_nodes(_parse_range(label, 0, label.length()))
	_emit(nodes, buffer, node)
	return target.after - index


## Reads whatever follows a link label: an inline destination, a full or
## collapsed reference, or a shortcut reference. Returns null when none of them
## resolves, which makes the brackets literal text.
func _resolve_target(text: String, from: int, end: int, label: String) -> _LinkTarget:
	if from < end and text.substr(from, 1) == "(":
		return _read_inline_target(text, from, end)
	if from < end and text.substr(from, 1) == "[":
		var second_end: int = _find_bracket_close(text, from, end)
		if second_end < 0:
			return null
		var second: String = text.substr(from + 1, second_end - from - 1)
		var key: String = second if not second.is_empty() else label
		return _reference_target(key, second_end + 1)
	return _reference_target(label, from)


func _reference_target(label: String, after: int) -> _LinkTarget:
	var definition: MarkdownNode = _document.find_link_reference(label)
	if definition == null:
		return null
	var target: _LinkTarget = _LinkTarget.new()
	target.destination = definition.link_destination
	target.title = definition.link_title
	target.after = after
	return target


func _read_inline_target(text: String, from: int, end: int) -> _LinkTarget:
	var closing: int = _find_paren_close(text, from, end)
	if closing < 0:
		return null
	var inner: String = text.substr(from + 1, closing - from - 1).strip_edges()
	var destination: String = inner
	var title: String = ""
	if inner.begins_with("<"):
		var angle: int = inner.find(">")
		if angle < 0:
			return null
		destination = inner.substr(1, angle - 1)
		title = _read_title(inner.substr(angle + 1).strip_edges())
	else:
		var space: int = inner.find(" ")
		if space >= 0:
			destination = inner.substr(0, space)
			title = _read_title(inner.substr(space + 1).strip_edges())
	var target: _LinkTarget = _LinkTarget.new()
	target.destination = destination
	target.title = title
	target.after = closing + 1
	return target


func _read_title(raw: String) -> String:
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


func _find_bracket_close(text: String, open: int, end: int) -> int:
	var depth: int = 0
	var index: int = open
	while index < end:
		var character: String = text.substr(index, 1)
		if character == "\\":
			index += 2
			continue
		if character == "`":
			index += maxi(1, _skip_code_span(text, index, end))
			continue
		if character == "[":
			depth += 1
		elif character == "]":
			depth -= 1
			if depth == 0:
				return index
		index += 1
	return -1


func _find_paren_close(text: String, open: int, end: int) -> int:
	var depth: int = 0
	var index: int = open
	while index < end:
		var character: String = text.substr(index, 1)
		if character == "\\":
			index += 2
			continue
		if character == "(":
			depth += 1
		elif character == ")":
			depth -= 1
			if depth == 0:
				return index
		index += 1
	return -1


## Returns how many characters a code span starting at [param index] occupies,
## or 0 when the run opens no span.
func _skip_code_span(text: String, index: int, end: int) -> int:
	var opening: int = _run_length(text, index, end, "`")
	var closing: int = _find_backtick_run(text, index + opening, end, opening)
	if closing < 0:
		return 0
	return closing + opening - index


# --- Emphasis ----------------------------------------------------------------


func _take_emphasis(
		text: String,
		index: int,
		end: int,
		nodes: Array[MarkdownNode],
		buffer: String
) -> int:
	var character: String = text.substr(index, 1)
	var run: int = _run_length(text, index, end, character)
	var needed: int = mini(run, _MAX_DELIMITER_LENGTH)
	if character == "~":
		if run < _MAX_DELIMITER_LENGTH:
			return 0
		needed = _MAX_DELIMITER_LENGTH
	if not _can_open(text, index, end, character, run, buffer):
		return 0

	var content_start: int = index + needed
	var closing: int = _find_closing_run(text, content_start, end, character, needed)
	if closing < 0:
		return 0
	var closing_run: int = _run_length(text, closing, end, character)
	var content_end: int = closing + closing_run - needed

	var kind: MarkdownNode.Kind = _emphasis_kind(character, needed)
	var node: MarkdownNode = MarkdownNode.new(kind)
	node.add_child_nodes(_parse_range(text, content_start, content_end))
	_emit(nodes, buffer, node)
	return closing + closing_run - index


func _emphasis_kind(character: String, length: int) -> MarkdownNode.Kind:
	if character == "~":
		return MarkdownNode.Kind.STRIKETHROUGH
	if length >= _MAX_DELIMITER_LENGTH:
		return MarkdownNode.Kind.STRONG
	return MarkdownNode.Kind.EMPHASIS


## Searches forward for a delimiter run of at least [param needed] characters
## that can close, skipping over code spans and over runs claimed by nested
## openers of the same character.
func _find_closing_run(text: String, from: int, end: int, character: String, needed: int) -> int:
	var depth: int = 0
	var index: int = from
	while index < end:
		var current: String = text.substr(index, 1)
		if current == "\\":
			index += 2
			continue
		if current == "`":
			index += maxi(1, _skip_code_span(text, index, end))
			continue
		if current != character:
			index += 1
			continue
		var run: int = _run_length(text, index, end, character)
		if run >= needed and _can_close(text, index, end, character):
			if depth == 0:
				return index
			depth -= 1
			index += run
			continue
		if run >= needed and _can_open(text, index, end, character, run, text.substr(index - 1, 1)):
			depth += 1
		index += run
	return -1


func _can_open(
		text: String,
		index: int,
		end: int,
		character: String,
		run: int,
		preceding_text: String
) -> bool:
	var following: String = text.substr(index + run, 1)
	if following.is_empty() or MarkdownScanner.is_whitespace(following) or index + run >= end:
		return false
	if character != "_":
		return true
	if preceding_text.is_empty():
		return true
	var previous: String = preceding_text.substr(preceding_text.length() - 1, 1)
	return MarkdownScanner.is_whitespace(previous) or MarkdownScanner.is_punctuation(previous)


func _can_close(text: String, index: int, end: int, character: String) -> bool:
	if index == 0:
		return false
	var previous: String = text.substr(index - 1, 1)
	if MarkdownScanner.is_whitespace(previous):
		return false
	if character != "_":
		return true
	var run: int = _run_length(text, index, end, character)
	var following: String = text.substr(index + run, 1)
	if following.is_empty():
		return true
	return MarkdownScanner.is_whitespace(following) or MarkdownScanner.is_punctuation(following)


class _LinkTarget:
	extends RefCounted

	var destination: String = ""
	var title: String = ""
	var after: int = 0
