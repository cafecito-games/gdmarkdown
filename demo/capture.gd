# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends SceneTree
## Renders the README screenshots.
##
## Run through [code]scripts/capture-screenshots[/code], which needs a real
## rendering context: a headless run produces blank images. Each shot renders
## into a [SubViewport] of an exact size rather than into the window, so the
## results do not depend on the display the capture happened to run on, and so
## a document shot can be resized to the height its content actually needs.

const OUTPUT_DIRECTORY: String = "res://docs/images"
const SAMPLE_DIRECTORY: String = "res://demo/samples"
const FRAMES_BEFORE_CAPTURE: int = 6
const PAGE_MARGIN: int = 28
const MEASURING_HEIGHT: int = 3000


func _initialize() -> void:
	await _capture_all()


func _capture_all() -> void:
	await _capture_demo_window()
	await _capture_document("02_quest_log.md", MarkdownDemoPresets.dusk(), "constructs", 780)
	await _capture_style_comparison()
	print("Wrote screenshots to %s" % OUTPUT_DIRECTORY)
	quit()


## The interactive demo itself, chrome included.
func _capture_demo_window() -> void:
	var scene: PackedScene = load("res://demo/demo.tscn")
	var demo: Control = scene.instantiate()
	var viewport: SubViewport = _open_viewport(demo, 1280, 820)
	await _settle()
	_write("demo", await _grab(viewport))
	viewport.queue_free()
	await process_frame


## One document rendered on its own, cropped to its content.
func _capture_document(
		file_name: String,
		preset: MarkdownDemoPresets.Preset,
		image_name: String,
		width: int
) -> void:
	var page: _Page = _build_page(file_name, preset, width)
	var viewport: SubViewport = _open_viewport(page.page, width, MEASURING_HEIGHT)
	await _settle()
	viewport.size = Vector2i(width, page.content_height())
	await _settle()
	_write(image_name, await _grab(viewport))
	viewport.queue_free()
	await process_frame


## The same document under two presets, side by side, to show that styling is a
## resource rather than a hard-coded appearance.
func _capture_style_comparison() -> void:
	var width: int = 1180
	var column_width: int = roundi(width / 2.0)
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 0)

	var pages: Array[_Page] = []
	for preset: MarkdownDemoPresets.Preset in [
		MarkdownDemoPresets.parchment(),
		MarkdownDemoPresets.terminal(),
	]:
		var page: _Page = _build_page("03_formatting.md", preset, column_width)
		page.page.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		pages.append(page)
		row.add_child(page.page)

	var viewport: SubViewport = _open_viewport(row, width, MEASURING_HEIGHT)
	await _settle()
	var height: int = 0
	for page: _Page in pages:
		height = maxi(height, page.content_height())
	viewport.size = Vector2i(width, height)
	await _settle()
	_write("styles", await _grab(viewport))
	viewport.queue_free()
	await process_frame


func _build_page(file_name: String, preset: MarkdownDemoPresets.Preset, width: int) -> _Page:
	var panel: PanelContainer = PanelContainer.new()
	var background: StyleBoxFlat = StyleBoxFlat.new()
	background.bg_color = preset.background
	panel.add_theme_stylebox_override("panel", background)
	panel.custom_minimum_size = Vector2(width, 0)

	var margin: MarginContainer = MarginContainer.new()
	for side: String in ["left", "top", "right", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, PAGE_MARGIN)
	panel.add_child(margin)

	var label: MarkdownLabel = MarkdownLabel.new()
	label.fit_content = true
	label.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	preset.apply_to(label)
	label.markdown = FileAccess.get_file_as_string("%s/%s" % [SAMPLE_DIRECTORY, file_name])
	margin.add_child(label)

	var page: _Page = _Page.new()
	page.page = panel
	page.label = label
	return page


func _open_viewport(content: Control, width: int, height: int) -> SubViewport:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(width, height)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	content.set_anchors_preset(Control.PRESET_FULL_RECT)
	viewport.add_child(content)
	root.add_child(viewport)
	return viewport


## Waits long enough for layout, text shaping, and a draw to finish.
func _settle() -> void:
	var frame: int = 0
	while frame < FRAMES_BEFORE_CAPTURE:
		await process_frame
		frame += 1


func _grab(viewport: SubViewport) -> Image:
	await RenderingServer.frame_post_draw
	return viewport.get_texture().get_image()


func _write(image_name: String, image: Image) -> void:
	var path: String = "%s/%s.png" % [OUTPUT_DIRECTORY, image_name]
	var status: int = image.save_png(path)
	assert(status == OK, "could not write " + path)
	print("  %s  %dx%d" % [path, image.get_width(), image.get_height()])


class _Page:
	extends RefCounted

	var page: PanelContainer = null
	var label: MarkdownLabel = null


	## Height the rendered document needs, including the page margins.
	func content_height() -> int:
		return ceili(label.size.y) + PAGE_MARGIN * 2
