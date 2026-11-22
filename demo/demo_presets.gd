# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

class_name MarkdownDemoPresets
extends RefCounted
## Theme presets shared by the interactive demo and the screenshot capture script.
##
## Each preset pairs a configured [MarkdownTheme] (including custom game callouts
## such as [code]QUEST[/code] and [code]LORE[/code]) with matching window chrome
## colors so switching themes harmonizes the entire showcase UI.


## Returns every showcase preset in toolbar picker order.
static func all() -> Array[Preset]:
	var presets: Array[Preset] = []
	presets.assign([slate_dark(), paper_light(), parchment_rpg(), terminal()])
	return presets


## Builds the dark slate preset with warm gold [code]QUEST[/code] and amethyst
## [code]LORE[/code] [MarkdownCalloutStyle] entries.
static func slate_dark() -> Preset:
	var preset: Preset = Preset.new()
	preset.title = "Slate Dark"
	preset.is_dark = true
	preset.window_background = Color(0.043, 0.055, 0.078)
	preset.sidebar_surface = Color(0.067, 0.086, 0.125)
	preset.card_surface = Color(0.078, 0.098, 0.141)
	preset.editor_surface = Color(0.055, 0.071, 0.102)
	preset.border_color = Color(0.145, 0.180, 0.247)
	preset.chrome_text = Color(0.902, 0.925, 0.961)
	preset.muted_text = Color(0.525, 0.580, 0.667)
	preset.accent_color = Color(0.388, 0.659, 1.0)

	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	theme.base_font_size = 15
	theme.line_spacing = 5.0
	theme.paragraph_spacing_em = 0.8
	theme.heading_top_spacing_em = 1.15
	theme.heading_bottom_spacing_em = 0.4
	theme.corner_radius = 6.0
	theme.code_bg_color = Color(0.055, 0.071, 0.102)
	theme.inline_code_bg_color = Color(0.145, 0.180, 0.247, 0.95)
	theme.code_text_color = Color(0.941, 0.804, 0.561)
	theme.blockquote_bg_color = Color(0.106, 0.133, 0.192, 0.78)
	theme.blockquote_accent_color = Color(0.369, 0.467, 0.620)
	theme.table_border_color = Color(0.180, 0.224, 0.302)
	theme.table_header_bg_color = Color(0.114, 0.141, 0.200)
	theme.table_alt_row_bg_color = Color(0.094, 0.118, 0.169, 0.65)
	theme.rule_color = Color(0.196, 0.239, 0.318)

	var quest_color: Color = Color(0.957, 0.761, 0.298)
	var lore_color: Color = Color(0.741, 0.561, 0.980)
	theme.set_callout_style(
			"QUEST",
			MarkdownCalloutStyle.create(
					"Quest",
					quest_color,
					quest_color,
					MarkdownDemoIcons.create_callout_icon("QUEST", quest_color)
			)
	)
	theme.set_callout_style(
			"LORE",
			MarkdownCalloutStyle.create(
					"Lore",
					lore_color,
					lore_color,
					MarkdownDemoIcons.create_callout_icon("LORE", lore_color)
			)
	)
	preset.theme = theme
	return preset


## Builds a crisp editorial light preset derived from
## [method MarkdownTheme.create_light] with matching light callouts.
static func paper_light() -> Preset:
	var preset: Preset = Preset.new()
	preset.title = "Paper Light"
	preset.is_dark = false
	preset.window_background = Color(0.945, 0.957, 0.973)
	preset.sidebar_surface = Color(0.976, 0.984, 0.992)
	preset.card_surface = Color(1.0, 1.0, 1.0)
	preset.editor_surface = Color(0.969, 0.976, 0.988)
	preset.border_color = Color(0.867, 0.894, 0.929)
	preset.chrome_text = Color(0.094, 0.118, 0.161)
	preset.muted_text = Color(0.400, 0.447, 0.522)
	preset.accent_color = Color(0.106, 0.435, 0.863)

	var theme: MarkdownTheme = MarkdownTheme.create_light()
	theme.base_font_size = 15
	theme.line_spacing = 5.0
	theme.paragraph_spacing_em = 0.8
	theme.heading_top_spacing_em = 1.15
	theme.heading_bottom_spacing_em = 0.4
	theme.corner_radius = 6.0
	theme.code_bg_color = Color(0.957, 0.969, 0.984)
	theme.inline_code_bg_color = Color(0.918, 0.937, 0.965)
	theme.table_header_bg_color = Color(0.945, 0.961, 0.980)
	theme.table_alt_row_bg_color = Color(0.976, 0.984, 0.996, 0.9)

	var quest_color: Color = Color(0.745, 0.482, 0.055)
	var lore_color: Color = Color(0.494, 0.278, 0.804)
	theme.set_callout_style(
			"QUEST",
			MarkdownCalloutStyle.create(
					"Quest",
					quest_color,
					quest_color,
					MarkdownDemoIcons.create_callout_icon("QUEST", quest_color)
			)
	)
	theme.set_callout_style(
			"LORE",
			MarkdownCalloutStyle.create(
					"Lore",
					lore_color,
					lore_color,
					MarkdownDemoIcons.create_callout_icon("LORE", lore_color)
			)
	)
	preset.theme = theme
	return preset


## Builds a warm fantasy codex preset with parchment surfaces, sepia ink, brass
## accents, and custom [MarkdownCalloutStyle] palettes.
static func parchment_rpg() -> Preset:
	var preset: Preset = Preset.new()
	preset.title = "Parchment RPG"
	preset.is_dark = false
	preset.window_background = Color(0.902, 0.863, 0.784)
	preset.sidebar_surface = Color(0.941, 0.910, 0.843)
	preset.card_surface = Color(0.969, 0.945, 0.890)
	preset.editor_surface = Color(0.933, 0.898, 0.824)
	preset.border_color = Color(0.808, 0.749, 0.635)
	preset.chrome_text = Color(0.173, 0.129, 0.094)
	preset.muted_text = Color(0.439, 0.365, 0.286)
	preset.accent_color = Color(0.549, 0.176, 0.176)

	var theme: MarkdownTheme = MarkdownTheme.create_light()
	theme.base_font_size = 15
	theme.line_spacing = 5.5
	theme.paragraph_spacing_em = 0.85
	theme.heading_top_spacing_em = 1.15
	theme.heading_bottom_spacing_em = 0.4
	theme.corner_radius = 5.0
	theme.text_color = Color(0.180, 0.137, 0.098)
	theme.heading_color = Color(0.467, 0.129, 0.145)
	theme.muted_color = Color(0.447, 0.369, 0.290)
	theme.link_color = Color(0.580, 0.216, 0.157)
	theme.link_hover_color = Color(0.741, 0.294, 0.176)
	theme.code_text_color = Color(0.361, 0.200, 0.102)
	theme.code_bg_color = Color(0.922, 0.882, 0.792)
	theme.inline_code_bg_color = Color(0.894, 0.843, 0.733, 0.95)
	theme.strikethrough_color = Color(0.482, 0.400, 0.318, 0.85)
	theme.rule_color = Color(0.737, 0.647, 0.498)
	theme.blockquote_accent_color = Color(0.675, 0.498, 0.212)
	theme.blockquote_bg_color = Color(0.925, 0.878, 0.776, 0.78)
	theme.table_border_color = Color(0.757, 0.675, 0.533)
	theme.table_header_bg_color = Color(0.902, 0.851, 0.741)
	theme.table_alt_row_bg_color = Color(0.945, 0.914, 0.839, 0.85)
	theme.task_checked_color = Color(0.204, 0.478, 0.278)
	theme.task_unchecked_color = Color(0.557, 0.471, 0.353)

	theme.set_callout_style("NOTE", MarkdownCalloutStyle.create("Note", Color(0.188, 0.380, 0.580)))
	theme.set_callout_style("TIP", MarkdownCalloutStyle.create("Tip", Color(0.204, 0.478, 0.278)))
	theme.set_callout_style(
			"IMPORTANT",
			MarkdownCalloutStyle.create("Important", Color(0.494, 0.227, 0.569))
	)
	theme.set_callout_style(
			"WARNING",
			MarkdownCalloutStyle.create("Warning", Color(0.710, 0.431, 0.082))
	)
	theme.set_callout_style(
			"CAUTION",
			MarkdownCalloutStyle.create("Caution", Color(0.686, 0.161, 0.176))
	)

	var quest_color: Color = Color(0.690, 0.455, 0.082)
	var lore_color: Color = Color(0.447, 0.220, 0.553)
	theme.set_callout_style(
			"QUEST",
			MarkdownCalloutStyle.create(
					"Quest",
					quest_color,
					quest_color,
					MarkdownDemoIcons.create_callout_icon("QUEST", quest_color)
			)
	)
	theme.set_callout_style(
			"LORE",
			MarkdownCalloutStyle.create(
					"Lore",
					lore_color,
					lore_color,
					MarkdownDemoIcons.create_callout_icon("LORE", lore_color)
			)
	)
	preset.theme = theme
	return preset


## Builds a phosphor-green and amber developer console preset on dark obsidian
## panels with [code]Mission[/code] and [code]Archive[/code] callout labels.
static func terminal() -> Preset:
	var preset: Preset = Preset.new()
	preset.title = "Terminal"
	preset.is_dark = true
	preset.window_background = Color(0.024, 0.035, 0.031)
	preset.sidebar_surface = Color(0.039, 0.063, 0.055)
	preset.card_surface = Color(0.051, 0.078, 0.071)
	preset.editor_surface = Color(0.031, 0.051, 0.045)
	preset.border_color = Color(0.114, 0.188, 0.161)
	preset.chrome_text = Color(0.784, 0.945, 0.839)
	preset.muted_text = Color(0.392, 0.569, 0.467)
	preset.accent_color = Color(0.224, 0.902, 0.549)

	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	theme.base_font_size = 15
	theme.line_spacing = 4.5
	theme.paragraph_spacing_em = 0.75
	theme.heading_top_spacing_em = 1.1
	theme.heading_bottom_spacing_em = 0.38
	theme.corner_radius = 3.0
	theme.text_color = Color(0.784, 0.945, 0.839)
	theme.heading_color = Color(0.282, 0.945, 0.596)
	theme.muted_color = Color(0.420, 0.620, 0.510)
	theme.link_color = Color(0.349, 0.882, 0.941)
	theme.link_hover_color = Color(0.620, 0.961, 1.0)
	theme.code_text_color = Color(0.980, 0.780, 0.361)
	theme.code_bg_color = Color(0.031, 0.051, 0.045)
	theme.inline_code_bg_color = Color(0.090, 0.161, 0.129, 0.95)
	theme.strikethrough_color = Color(0.392, 0.569, 0.467, 0.85)
	theme.rule_color = Color(0.157, 0.278, 0.231)
	theme.blockquote_accent_color = Color(0.278, 0.780, 0.522)
	theme.blockquote_bg_color = Color(0.071, 0.122, 0.102, 0.8)
	theme.table_border_color = Color(0.157, 0.278, 0.231)
	theme.table_header_bg_color = Color(0.082, 0.149, 0.122)
	theme.table_alt_row_bg_color = Color(0.063, 0.106, 0.090, 0.72)
	theme.task_checked_color = Color(0.298, 0.922, 0.580)
	theme.task_unchecked_color = Color(0.341, 0.522, 0.431)

	var quest_color: Color = Color(0.980, 0.761, 0.282)
	var lore_color: Color = Color(0.341, 0.863, 0.941)
	theme.set_callout_style(
			"QUEST",
			MarkdownCalloutStyle.create(
					"Mission",
					quest_color,
					quest_color,
					MarkdownDemoIcons.create_callout_icon("QUEST", quest_color)
			)
	)
	theme.set_callout_style(
			"LORE",
			MarkdownCalloutStyle.create(
					"Archive",
					lore_color,
					lore_color,
					MarkdownDemoIcons.create_callout_icon("LORE", lore_color)
			)
	)
	preset.theme = theme
	return preset


## Pairs a configured [MarkdownTheme] with harmonized showcase window chrome colors.
class Preset:
	extends RefCounted

	var title: String = ""
	var is_dark: bool = true
	var window_background: Color = Color.BLACK
	var sidebar_surface: Color = Color.BLACK
	var card_surface: Color = Color.BLACK
	var editor_surface: Color = Color.BLACK
	var border_color: Color = Color.GRAY
	var chrome_text: Color = Color.WHITE
	var muted_text: Color = Color.GRAY
	var accent_color: Color = Color.CORNFLOWER_BLUE
	var theme: MarkdownTheme = null


	## Applies this preset's [MarkdownTheme] to [param view].
	func apply_to(view: MarkdownView) -> void:
		view.markdown_theme = theme
