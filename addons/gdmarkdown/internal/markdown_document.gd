# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

class_name MarkdownDocument
extends RefCounted
## A parsed Markdown document: its block tree plus the link reference
## definitions collected while parsing it.
##
## Link reference definitions are document-scoped and may appear after the
## paragraph that uses them, so the block parser gathers them first and the
## inline parser resolves against them afterwards.

## Root of the block tree, always a [constant MarkdownNode.Kind.DOCUMENT] node.
var root: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.DOCUMENT)

## Link reference definitions, keyed by normalized label. Each value is a
## [constant MarkdownNode.Kind.LINK] node carrying only a destination and title.
var link_references: Dictionary[String, MarkdownNode] = {}


## Normalizes a link label the way CommonMark matches them: case folded, with
## runs of whitespace collapsed to a single space.
static func normalize_label(label: String) -> String:
	var trimmed: String = label.strip_edges()
	var collapsed: String = ""
	var pending_space: bool = false
	for index: int in trimmed.length():
		var character: String = trimmed.substr(index, 1)
		if character == " " or character == "\t" or character == "\n" or character == "\r":
			pending_space = not collapsed.is_empty()
			continue
		if pending_space:
			collapsed += " "
			pending_space = false
		collapsed += character
	return collapsed.to_lower()


## Records a link reference definition. The first definition of a label wins,
## matching CommonMark.
func add_link_reference(label: String, destination: String, title: String) -> void:
	var key: String = normalize_label(label)
	if key.is_empty() or link_references.has(key):
		return
	var definition: MarkdownNode = MarkdownNode.new(MarkdownNode.Kind.LINK)
	definition.link_destination = destination
	definition.link_title = title
	link_references[key] = definition


## Returns the definition for [param label], or null when the document defines
## no such label.
func find_link_reference(label: String) -> MarkdownNode:
	var key: String = normalize_label(label)
	if not link_references.has(key):
		return null
	return link_references[key]
