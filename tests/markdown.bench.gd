# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends GDTestBenchmark
## Timings for each stage of the pipeline, separately and together.
##
## The stages are measured apart because they have different costs and different
## reasons to regress: block parsing is line-oriented and mostly linear, inline
## parsing backtracks over delimiter runs, and emission is string building. A
## full conversion is measured too, since that is what assigning
## [member MarkdownLabel.markdown] actually costs.
##
## Run with [code]task bench[/code]. Timings are informational and never fail a
## build.

const SOURCE_PATH: String = "res://tests/fixtures/kitchen_sink.md"
const LARGE_REPEAT_COUNT: int = 25

var _source: String = ""
var _large_source: String = ""
var _parsed: MarkdownDocument = null
var _blocks_only: MarkdownDocument = null
var _style: MarkdownStyle = null


func before_all() -> void:
	_source = FileAccess.get_file_as_string(SOURCE_PATH)
	_large_source = _repeat_document(_source, LARGE_REPEAT_COUNT)
	_style = MarkdownStyle.new()


## Reparsed per sample so emission and inline expansion each start from a tree
## nothing has already mutated.
func before_sample(_info: GDTestBenchmarkInfo) -> void:
	_blocks_only = MarkdownBlockParser.parse(_source)
	_parsed = Markdown.parse(_source)


func benchmark_parse_blocks() -> void:
	var document: MarkdownDocument = MarkdownBlockParser.parse(_source)
	assert(document != null)


func benchmark_expand_inlines() -> void:
	MarkdownInlineParser.parse_document(_blocks_only)


func benchmark_emit_bbcode() -> void:
	var bbcode: String = MarkdownBbcodeEmitter.emit(_parsed, _style)
	assert(not bbcode.is_empty())


func benchmark_full_conversion() -> void:
	var bbcode: String = Markdown.to_bbcode(_source, _style)
	assert(not bbcode.is_empty())


func benchmark_full_conversion_of_a_large_document() -> void:
	var bbcode: String = Markdown.to_bbcode(_large_source, _style)
	assert(not bbcode.is_empty())


func benchmark_inline_heavy_paragraph() -> void:
	var source: String = (
			"A paragraph with *emphasis*, **strong**, ~~struck~~, `code`, "
			+ "a [link](https://example.com), an ![image](res://a.png), "
			+ "https://example.com and &hellip; entities, repeated often. "
	).repeat(20)
	var bbcode: String = Markdown.to_bbcode(source, _style)
	assert(not bbcode.is_empty())


func _repeat_document(source: String, times: int) -> String:
	var parts: Array[String] = []
	var index: int = 0
	while index < times:
		parts.append(source)
		index += 1
	return MarkdownScanner.join("\n\n", parts)
