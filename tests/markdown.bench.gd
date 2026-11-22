# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends GDTestBenchmark
## Benchmarks for native [MarkdownView] parsing and layout using the kitchen-sink
## fixture at typical and stressed document sizes.
##
## Run with [code]./addons/gdtest/gdtest bench[/code]. Timings are informational
## and never fail a build.

const SOURCE_PATH: String = "res://tests/fixtures/kitchen_sink.md"
const LARGE_REPEAT_COUNT: int = 10

var _source: String = ""
var _large_source: String = ""
var _theme: MarkdownTheme = null
var _view: MarkdownView = null


## Loads the benchmark fixture, builds the repeated stress document, and creates
## a shared [MarkdownView] instance.
func before_all() -> void:
	_source = FileAccess.get_file_as_string(SOURCE_PATH)
	_large_source = _repeat_document(_source, LARGE_REPEAT_COUNT)
	_theme = MarkdownTheme.create_dark()
	_view = MarkdownView.new()
	_view.markdown_theme = _theme
	_view.custom_minimum_size = Vector2(640.0, 0.0)


## Releases the shared [MarkdownView] after all benchmark cases complete.
func after_all() -> void:
	if _view != null:
		_view.free()
		_view = null


## Measures full parse and layout time for a single kitchen-sink document.
func benchmark_parse_and_layout_kitchen_sink() -> void:
	_view.markdown = _source
	var minimum_size: Vector2 = _view.get_combined_minimum_size()
	assert(minimum_size.y > 0.0)


## Measures full parse and layout time for a 10x repeated stress document.
func benchmark_parse_and_layout_large_document() -> void:
	_view.markdown = _large_source
	var minimum_size: Vector2 = _view.get_combined_minimum_size()
	assert(minimum_size.y > 0.0)


## Measures re-layout cost across a container width reduction and restore cycle.
func benchmark_relayout_width_change() -> void:
	_view.markdown = _source
	_view.custom_minimum_size = Vector2(320.0, 0.0)
	_view.markdown = _source
	var narrow_height: float = _view.get_combined_minimum_size().y
	_view.custom_minimum_size = Vector2(640.0, 0.0)
	assert(narrow_height > 0.0)


## Concatenates [param source] [param times] times separated by blank lines.
func _repeat_document(source: String, times: int) -> String:
	var parts: PackedStringArray = PackedStringArray()
	var resize_error: int = parts.resize(times)
	assert(resize_error == OK)
	for index: int in range(times):
		parts[index] = source
	return "\n\n".join(parts)
