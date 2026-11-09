# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

class_name MarkdownNode
extends RefCounted
## One node of a parsed Markdown document tree.
##
## The tree is deliberately built from a single enum-tagged type rather than one
## class per construct: GDScript has no sum types, so a class hierarchy would
## force every test to assemble an object graph just to assert a shape. Only the
## fields that belong to a node's [member kind] carry meaning; the rest keep
## their defaults.

enum Kind {
	DOCUMENT,
	HEADING,
	PARAGRAPH,
	BLOCK_QUOTE,
	CODE_BLOCK,
	LIST,
	LIST_ITEM,
	THEMATIC_BREAK,
	TABLE,
	TABLE_ROW,
	TABLE_CELL,
	TEXT,
	EMPHASIS,
	STRONG,
	STRIKETHROUGH,
	CODE_SPAN,
	LINK,
	IMAGE,
	LINE_BREAK,
	SOFT_BREAK,
}

## Whether a list item carries a GitHub Flavored Markdown task marker.
enum TaskState { NONE, UNCHECKED, CHECKED }

## Column alignment requested by a table's delimiter row.
enum Alignment { NONE, LEFT, CENTER, RIGHT }

## Which construct this node represents.
var kind: Kind = Kind.DOCUMENT

## Child nodes, in document order.
var children: Array[MarkdownNode] = []

## Literal content, used by [constant Kind.TEXT], [constant Kind.CODE_SPAN],
## [constant Kind.CODE_BLOCK] and the alternate text of [constant Kind.IMAGE].
var text: String = ""

## Heading level, 1 through 6.
var heading_level: int = 0

## Info string of a fenced code block, trimmed to its first word.
var code_language: String = ""

## Whether a [constant Kind.LIST] is ordered.
var list_ordered: bool = false

## First number of an ordered [constant Kind.LIST].
var list_start: int = 1

## Whether a [constant Kind.LIST] is tight, meaning its items hold no blank
## lines and render without paragraph spacing.
var list_tight: bool = true

## Task marker state of a [constant Kind.LIST_ITEM].
var task_state: TaskState = TaskState.NONE

## Destination of a [constant Kind.LINK] or [constant Kind.IMAGE].
var link_destination: String = ""

## Optional title of a [constant Kind.LINK] or [constant Kind.IMAGE].
var link_title: String = ""

## Per-column [enum Alignment] values of a [constant Kind.TABLE].
var column_alignments: Array[int] = []


func _init(node_kind: Kind = Kind.DOCUMENT, node_text: String = "") -> void:
	kind = node_kind
	text = node_text


## Appends [param child] to [member children].
func add_child_node(child: MarkdownNode) -> void:
	children.append(child)


## Appends every node in [param nodes] to [member children].
func add_child_nodes(nodes: Array[MarkdownNode]) -> void:
	children.append_array(nodes)


## Returns a stable, indented rendering of this subtree. Tests assert on this
## string rather than walking the tree field by field.
func to_debug_string(indent_level: int = 0) -> String:
	var lines: Array[String] = []
	lines.append("\t".repeat(indent_level) + _describe())
	for child: MarkdownNode in children:
		lines.append(child.to_debug_string(indent_level + 1))
	return MarkdownScanner.join("\n", lines)


func _describe() -> String:
	var parts: Array[String] = []
	parts.append(_kind_name())
	match kind:
		Kind.HEADING:
			parts.append("level=%d" % heading_level)
		Kind.CODE_BLOCK:
			if not code_language.is_empty():
				parts.append("language=%s" % code_language)
		Kind.LIST:
			parts.append("ordered=%s" % list_ordered)
			parts.append("tight=%s" % list_tight)
			if list_ordered:
				parts.append("start=%d" % list_start)
		Kind.LIST_ITEM:
			if task_state != TaskState.NONE:
				parts.append("task=%s" % _task_state_name())
		Kind.TABLE:
			parts.append("alignments=%s" % _alignment_names())
		Kind.LINK, Kind.IMAGE:
			parts.append("destination=%s" % link_destination)
			if not link_title.is_empty():
				parts.append("title=%s" % link_title)
	if not text.is_empty():
		parts.append("text=%s" % text.c_escape())
	return MarkdownScanner.join(" ", parts)


func _kind_name() -> String:
	var keys: Array[String] = []
	keys.assign(Kind.keys())
	return keys[kind]


func _task_state_name() -> String:
	var keys: Array[String] = []
	keys.assign(TaskState.keys())
	return keys[task_state]


func _alignment_names() -> String:
	var keys: Array[String] = []
	keys.assign(Alignment.keys())
	var names: Array[String] = []
	for alignment: int in column_alignments:
		names.append(keys[alignment])
	return "[%s]" % MarkdownScanner.join(",", names)
