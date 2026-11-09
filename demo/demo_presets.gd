# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

class_name MarkdownDemoPresets
extends RefCounted
## Styling presets shared by the interactive demo and the screenshot capture.
##
## A preset pairs a [MarkdownStyle] with the surrounding colors a host would set
## on its own: the page background and the label's default text color. Keeping
## them together is what makes the demo's style picker look like three different
## applications rather than three bullet characters.


## Returns every preset, in picker order.
static func all() -> Array[Preset]:
	var presets: Array[Preset] = []
	presets.assign([dusk(), parchment(), terminal()])
	return presets


## The addon's own defaults, on a dark page.
static func dusk() -> Preset:
	var preset: Preset = Preset.new()
	preset.title = "Dusk"
	preset.background = Color(0.09, 0.10, 0.13)
	preset.text = Color(0.87, 0.89, 0.93)
	preset.style = MarkdownStyle.new()
	return preset


## A warm light page, showing that nothing in the addon assumes a dark theme.
static func parchment() -> Preset:
	var preset: Preset = Preset.new()
	preset.title = "Parchment"
	preset.background = Color(0.96, 0.94, 0.88)
	preset.text = Color(0.17, 0.15, 0.13)
	var style: MarkdownStyle = MarkdownStyle.new()
	style.code_background_color = Color(0.89, 0.86, 0.78)
	style.code_text_color = Color(0.42, 0.22, 0.12)
	style.quote_color = Color(0.45, 0.38, 0.30)
	style.link_color = Color(0.36, 0.26, 0.62)
	style.unordered_bullet = "—"
	style.thematic_break_text = "· · · · · · · · · ·"
	preset.style = style
	return preset


## Tight headings and ASCII markers, for a console or terminal-styled game.
static func terminal() -> Preset:
	var preset: Preset = Preset.new()
	preset.title = "Terminal"
	preset.background = Color(0.03, 0.06, 0.05)
	preset.text = Color(0.62, 0.89, 0.68)
	preset.font_size = 15
	var style: MarkdownStyle = MarkdownStyle.new()
	style.heading_font_sizes = PackedInt32Array([24, 22, 20, 18, 17, 16])
	style.code_background_color = Color(0.0, 0.14, 0.09, 0.85)
	style.code_text_color = Color(0.78, 0.98, 0.72)
	style.quote_color = Color(0.40, 0.64, 0.45)
	style.link_color = Color(0.55, 0.93, 0.95)
	style.unordered_bullet = "*"
	style.task_unchecked_glyph = "[ ]"
	style.task_checked_glyph = "[x]"
	style.thematic_break_text = "=========="
	style.show_code_language = true
	preset.style = style
	return preset


class Preset:
	extends RefCounted

	var title: String = ""
	var background: Color = Color.BLACK
	var text: Color = Color.WHITE
	var font_size: int = 16
	var style: MarkdownStyle = null


	## Applies this preset's surrounding colors to [param label].
	func apply_to(label: MarkdownLabel) -> void:
		label.markdown_style = style
		label.add_theme_color_override("default_color", text)
		label.add_theme_font_size_override("normal_font_size", font_size)
