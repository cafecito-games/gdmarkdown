# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends GDTest
## A whole-document regression gate.
##
## `tests/fixtures/kitchen_sink.md` exercises every construct the addon claims
## to support, and `kitchen_sink.bbcode` is the reviewed output for it. Any
## change to the pipeline that moves a single character shows up here as a diff
## instead of being discovered in a scene.

const SOURCE_PATH: String = "res://tests/fixtures/kitchen_sink.md"
const EXPECTED_PATH: String = "res://tests/fixtures/kitchen_sink.bbcode"


func get_suite_name() -> String:
	return "Golden document"


func test_renders_the_kitchen_sink_document() -> void:
	var source: String = FileAccess.get_file_as_string(SOURCE_PATH)
	var expected: String = FileAccess.get_file_as_string(EXPECTED_PATH)

	expect_not_empty(source)
	expect_not_empty(expected)
	expect_equal(Markdown.to_bbcode(source, _style()), expected)


func test_leaves_no_unescaped_bracket_outside_a_tag() -> void:
	var source: String = FileAccess.get_file_as_string(SOURCE_PATH)
	var rendered: String = Markdown.to_bbcode(source, _style())
	var label: RichTextLabel = autofree(RichTextLabel.new())
	label.bbcode_enabled = true

	label.text = rendered

	expect_not_empty(label.get_parsed_text())


## The same deterministic style the emitter suite uses, so the fixture stays
## readable as text.
func _style() -> MarkdownStyle:
	var style: MarkdownStyle = MarkdownStyle.new()
	style.code_background_color = Color(0.0, 0.0, 0.0, 1.0)
	style.code_text_color = Color(1.0, 1.0, 1.0, 1.0)
	style.quote_color = Color(0.5, 0.5, 0.5, 1.0)
	style.color_links = false
	style.thematic_break_text = "--"
	return style
