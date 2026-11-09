class_name MarkdownScanner
extends RefCounted
## Character and line predicates shared by the block and inline parsers.
##
## Every helper works on single-character strings rather than code points,
## because GDScript's string indexing already yields one-character strings and
## the Markdown syntax this addon recognizes is entirely ASCII.

## Number of spaces a tab advances to the next tab stop.
const TAB_WIDTH: int = 4

const _ASCII_PUNCTUATION: String = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"


## Returns [code]true[/code] when [param line] holds nothing but whitespace.
static func is_blank(line: String) -> bool:
	return line.strip_edges().is_empty()


## Returns [code]true[/code] when [param character] is a space, tab, or line
## ending.
static func is_whitespace(character: String) -> bool:
	return character == " " or character == "\t" or character == "\n" or character == "\r"


## Returns [code]true[/code] when [param character] is one of the ASCII
## punctuation characters CommonMark allows a backslash to escape.
static func is_punctuation(character: String) -> bool:
	return not character.is_empty() and _ASCII_PUNCTUATION.contains(character)


## Returns [code]true[/code] when [param character] is a decimal digit.
static func is_digit(character: String) -> bool:
	return character.length() == 1 and character >= "0" and character <= "9"


## Returns the number of leading space columns in [param line], counting a tab
## as advancing to the next tab stop.
static func leading_indent(line: String) -> int:
	var columns: int = 0
	for index: int in line.length():
		var character: String = line.substr(index, 1)
		if character == " ":
			columns += 1
		elif character == "\t":
			columns += TAB_WIDTH - columns % TAB_WIDTH
		else:
			break
	return columns


## Returns [param line] with its leading whitespace replaced by the equivalent
## number of spaces, so later indentation arithmetic can count characters.
static func expand_leading_tabs(line: String) -> String:
	var indent: int = leading_indent(line)
	var content_start: int = 0
	while content_start < line.length():
		var character: String = line.substr(content_start, 1)
		if character != " " and character != "\t":
			break
		content_start += 1
	return " ".repeat(indent) + line.substr(content_start)


## Returns [param line] with up to [param columns] leading space characters
## removed.
static func strip_indent(line: String, columns: int) -> String:
	var removed: int = 0
	while removed < columns and removed < line.length() and line.substr(removed, 1) == " ":
		removed += 1
	return line.substr(removed)


## Joins [param parts] with [param separator]. [method String.join] takes a
## [PackedStringArray], whose [code]append[/code] returns a value that strict
## warnings refuse to let callers discard, so the parsers accumulate into an
## [code]Array[String][/code] and join it here.
static func join(separator: String, parts: Array[String]) -> String:
	var result: String = ""
	for index: int in parts.size():
		if index > 0:
			result += separator
		result += parts[index]
	return result


## Splits [param markdown_text] into lines after normalizing a byte order mark,
## CRLF and CR line endings, and leading tabs.
static func split_lines(markdown_text: String) -> Array[String]:
	var normalized: String = markdown_text.trim_prefix("\uFEFF")
	normalized = normalized.replace("\r\n", "\n").replace("\r", "\n")
	var lines: Array[String] = []
	for line: String in normalized.split("\n"):
		lines.append(expand_leading_tabs(line))
	return lines
