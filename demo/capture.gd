# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends SceneTree
## Renders the README and documentation screenshots into [code]docs/images/[/code].
##
## Run through [code]scripts/capture-screenshots[/code], which requires a real
## rendering context; headless execution produces blank images.

const OUTPUT_DIRECTORY: String = "res://docs/images"
const SAMPLE_DIRECTORY: String = "res://demo/samples"
const FRAMES_BEFORE_CAPTURE: int = 8
const PAGE_PADDING: int = 28
const MEASURING_HEIGHT: int = 3600
const STYLE_COMPARISON_MARKDOWN: String = """# The Drowned Bell

> [!QUEST] Active Objective
> Strike the **Seventh Bell** at low tide with `sluice_key` ready.

- [x] Recover the tide charts
- [ ] Carry ![Potion](icon://potion) **Crimson Tonic**

| Reward | Qty |
| :----- | --: |
| ![Gold](icon://gold) Sovereigns | `240` |
| ![Sword](icon://sword) Sabre | `1` |

> [!LORE] Archivist's Note
> *Six* bells appear in the charter; the seventh has no mark.
"""
const TERMINAL_SHOWCASE_MARKDOWN: String = """# Harbor Telemetry Console

*Node `bellwatch-07` · Link online*

> [!QUEST] Active Uplink
> Sync tide sensors before **04:00 UTC** and verify `checksum_ok`.

- [x] Mount native `gdmarkdown` GDExtension
- [x] Bind `MarkdownTheme` live reload signal
- [ ] Flush harbor archive snapshots

| Sensor | Channel | Reading |
| :----- | :-----: | ------: |
| Tide Gauge | `CH-01` | `-1.42 m` |
| Bell Relay | `CH-07` | `READY` |

> [!LORE] Archive Log
> Last verified transmission logged `1794-11-09` from the drydock sill.

```gdscript
var theme: MarkdownTheme = MarkdownDemoPresets.terminal().theme
view.markdown_theme = theme
```
"""


## Entry point invoked by Godot when running this script via [code]--script[/code].
func _initialize() -> void:
	await _capture_all()


## Captures all README showcase PNGs sequentially and exits the tree.
func _capture_all() -> void:
	await _capture_demo_window()
	await _capture_style_comparison()
	await _capture_constructs_board()
	print("Wrote screenshots to %s" % OUTPUT_DIRECTORY)
	quit()


## Captures the full interactive showcase window in Slate Dark displaying Patch Notes.
func _capture_demo_window() -> void:
	var scene: PackedScene = load("res://demo/demo.tscn")
	var demo: Control = scene.instantiate()
	var viewport: SubViewport = _open_viewport(demo, 1280, 990)
	await _settle()
	_write("demo", await _grab(viewport))
	viewport.queue_free()
	await process_frame


## Side-by-side 3-theme comparison: Slate Dark vs Paper Light vs Parchment RPG.
func _capture_style_comparison() -> void:
	var total_width: int = 1320
	var column_width: int = 420
	var backdrop: PanelContainer = PanelContainer.new()
	var backdrop_style: StyleBoxFlat = StyleBoxFlat.new()
	backdrop_style.bg_color = Color(0.035, 0.043, 0.063)
	backdrop.add_theme_stylebox_override("panel", backdrop_style)

	var outer_margin: MarginContainer = MarginContainer.new()
	outer_margin.add_theme_constant_override("margin_left", 16)
	outer_margin.add_theme_constant_override("margin_top", 16)
	outer_margin.add_theme_constant_override("margin_right", 16)
	outer_margin.add_theme_constant_override("margin_bottom", 16)
	backdrop.add_child(outer_margin)

	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 14)
	outer_margin.add_child(row)

	var presets: Array[MarkdownDemoPresets.Preset] = [
		MarkdownDemoPresets.slate_dark(),
		MarkdownDemoPresets.paper_light(),
		MarkdownDemoPresets.parchment_rpg(),
	]
	var cards: Array[_DocumentCard] = []
	for preset: MarkdownDemoPresets.Preset in presets:
		var card: _DocumentCard = _build_document_card(
				STYLE_COMPARISON_MARKDOWN,
				preset,
				column_width,
				preset.title.to_upper()
		)
		card.panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		cards.append(card)
		row.add_child(card.panel)

	var viewport: SubViewport = _open_viewport(backdrop, total_width, MEASURING_HEIGHT)
	await _settle()
	var height: int = 0
	for card: _DocumentCard in cards:
		height = maxi(height, card.content_height())
	viewport.size = Vector2i(total_width, height + 32)
	await _settle()
	_write("styles", await _grab(viewport))
	viewport.queue_free()
	await process_frame


## Two-column showcase covering Quest Log, Terminal preset, and Alerts & Formatting.
func _capture_constructs_board() -> void:
	var total_width: int = 1320
	var column_width: int = 636
	var dark_preset: MarkdownDemoPresets.Preset = MarkdownDemoPresets.slate_dark()
	var terminal_preset: MarkdownDemoPresets.Preset = MarkdownDemoPresets.terminal()

	var backdrop: PanelContainer = PanelContainer.new()
	var backdrop_style: StyleBoxFlat = StyleBoxFlat.new()
	backdrop_style.bg_color = dark_preset.window_background
	backdrop.add_theme_stylebox_override("panel", backdrop_style)

	var outer_margin: MarginContainer = MarginContainer.new()
	outer_margin.add_theme_constant_override("margin_left", 16)
	outer_margin.add_theme_constant_override("margin_top", 16)
	outer_margin.add_theme_constant_override("margin_right", 16)
	outer_margin.add_theme_constant_override("margin_bottom", 16)
	backdrop.add_child(outer_margin)

	var columns: HBoxContainer = HBoxContainer.new()
	columns.add_theme_constant_override("separation", 16)
	outer_margin.add_child(columns)

	var left_stack: VBoxContainer = VBoxContainer.new()
	left_stack.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	left_stack.add_theme_constant_override("separation", 16)
	columns.add_child(left_stack)

	var quest_source: String = FileAccess.get_file_as_string(
			"%s/02_quest_log.md" % SAMPLE_DIRECTORY
	)
	var alerts_source: String = FileAccess.get_file_as_string(
			"%s/03_alerts_and_formatting.md" % SAMPLE_DIRECTORY
	)

	var quest_card: _DocumentCard = _build_document_card(
			quest_source,
			dark_preset,
			column_width,
			"RPG QUEST LOG · CUSTOM CALLOUTS & INLINE ICONS"
	)
	quest_card.panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	left_stack.add_child(quest_card.panel)

	var terminal_card: _DocumentCard = _build_document_card(
			TERMINAL_SHOWCASE_MARKDOWN,
			terminal_preset,
			column_width,
			"TERMINAL PRESET · CUSTOM THEME & CONSOLE PALETTE"
	)
	terminal_card.panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	terminal_card.panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	left_stack.add_child(terminal_card.panel)

	var right_card: _DocumentCard = _build_document_card(
			alerts_source,
			dark_preset,
			column_width,
			"GITHUB ALERTS · CODE BLOCKS · TABLES · BANNERS"
	)
	right_card.panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	columns.add_child(right_card.panel)

	var viewport: SubViewport = _open_viewport(backdrop, total_width, MEASURING_HEIGHT)
	await _settle()
	var left_height: int = quest_card.content_height() + 16 + terminal_card.content_height()
	var height: int = maxi(left_height, right_card.content_height()) + 32
	viewport.size = Vector2i(total_width, height)
	await _settle()
	_write("constructs", await _grab(viewport))
	viewport.queue_free()
	await process_frame


## Builds a framed preview card containing a [MarkdownView] configured with
## [param preset] and an optional uppercase section header bar.
func _build_document_card(
		markdown_source: String,
		preset: MarkdownDemoPresets.Preset,
		width: int,
		header_text: String
) -> _DocumentCard:
	var panel: PanelContainer = PanelContainer.new()
	var panel_style: StyleBoxFlat = StyleBoxFlat.new()
	panel_style.bg_color = preset.card_surface
	panel_style.border_color = preset.border_color
	panel_style.set_border_width_all(1)
	panel_style.set_corner_radius_all(8)
	panel.add_theme_stylebox_override("panel", panel_style)
	panel.custom_minimum_size = Vector2(float(width), 0.0)

	var layout: VBoxContainer = VBoxContainer.new()
	layout.add_theme_constant_override("separation", 0)
	panel.add_child(layout)

	var header_height: int = 0
	if not header_text.is_empty():
		var header_bar: PanelContainer = PanelContainer.new()
		var header_style: StyleBoxFlat = StyleBoxFlat.new()
		header_style.bg_color = preset.sidebar_surface
		header_style.border_color = preset.border_color
		header_style.border_width_bottom = 1
		header_style.corner_radius_top_left = 7
		header_style.corner_radius_top_right = 7
		header_bar.add_theme_stylebox_override("panel", header_style)
		layout.add_child(header_bar)

		var header_margin: MarginContainer = MarginContainer.new()
		header_margin.add_theme_constant_override("margin_left", PAGE_PADDING)
		header_margin.add_theme_constant_override("margin_right", PAGE_PADDING)
		header_margin.add_theme_constant_override("margin_top", 10)
		header_margin.add_theme_constant_override("margin_bottom", 10)
		header_bar.add_child(header_margin)

		var header_label: Label = Label.new()
		header_label.text = header_text
		header_label.add_theme_font_size_override("font_size", 11)
		header_label.add_theme_color_override("font_color", preset.muted_text)
		header_margin.add_child(header_label)
		header_height = 38

	var margin: MarginContainer = MarginContainer.new()
	margin.add_theme_constant_override("margin_left", PAGE_PADDING)
	margin.add_theme_constant_override("margin_top", 22)
	margin.add_theme_constant_override("margin_right", PAGE_PADDING)
	margin.add_theme_constant_override("margin_bottom", PAGE_PADDING)
	layout.add_child(margin)

	var view: MarkdownView = MarkdownView.new()
	view.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	view.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	view.interactive_tasks = true
	view.image_resolver = Callable(MarkdownDemoIcons, "resolve")
	preset.apply_to(view)
	view.markdown = markdown_source
	margin.add_child(view)

	var card: _DocumentCard = _DocumentCard.new()
	card.panel = panel
	card.view = view
	card.extra_height = header_height + 22 + PAGE_PADDING + 4
	return card


## Mounts [param content] inside an offscreen [SubViewport] of exact pixel
## dimensions so captures are independent of the host display size.
func _open_viewport(content: Control, width: int, height: int) -> SubViewport:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(width, height)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	content.set_anchors_preset(Control.PRESET_FULL_RECT)
	viewport.add_child(content)
	root.add_child(viewport)
	return viewport


## Waits [constant FRAMES_BEFORE_CAPTURE] process frames so layout and font
## shaping settle before grabbing pixels.
func _settle() -> void:
	var frame: int = 0
	while frame < FRAMES_BEFORE_CAPTURE:
		await process_frame
		frame += 1


## Captures the rendered [Image] from [param viewport] right after a frame draws.
func _grab(viewport: SubViewport) -> Image:
	await RenderingServer.frame_post_draw
	return viewport.get_texture().get_image()


## Saves [param image] as a PNG inside [constant OUTPUT_DIRECTORY].
func _write(image_name: String, image: Image) -> void:
	var path: String = "%s/%s.png" % [OUTPUT_DIRECTORY, image_name]
	var status: int = image.save_png(path)
	assert(status == OK, "could not write " + path)
	print("  %s  %dx%d" % [path, image.get_width(), image.get_height()])


## Pairs a rendered card [PanelContainer] with its [MarkdownView] so the
## capture harness can resize the [SubViewport] to the exact measured height.
class _DocumentCard:
	extends RefCounted

	var panel: PanelContainer = null
	var view: MarkdownView = null
	var extra_height: int = 0


	## Returns the total card height in pixels from the laid-out [MarkdownView]
	## minimum size plus chrome margins.
	func content_height() -> int:
		return ceili(view.get_combined_minimum_size().y) + extra_height
