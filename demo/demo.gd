# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends Control
## Interactive showcase for the native [MarkdownView], [MarkdownTheme], and
## [MarkdownCalloutStyle] API.
##
## Pairs a four-preset theme switcher with custom procedural inline and block
## icons via [member MarkdownView.image_resolver], interactive task lists,
## code-block wrapping toggle, live split-pane Markdown editor, typewriter
## reveal controls, and real-time signal diagnostics.

const SAMPLE_DIRECTORY: String = "res://demo/samples"
const TYPEWRITER_CHARACTERS_PER_SECOND: float = 240.0

const DEFAULT_EVENT_TEXT: String = "Ready — click links, toggle tasks, or open Live Editor."

var _presets: Array[MarkdownDemoPresets.Preset] = []
var _sample_paths: Array[String] = []
var _sample_file_names: Array[String] = []
var _syncing_editor: bool = false
var _syncing_slider: bool = false
var _typewriter_playing: bool = false
var _typewriter_elapsed_characters: float = 0.0
var _last_event_text: String = DEFAULT_EVENT_TEXT

@onready var background: ColorRect = %Background
@onready var toolbar_panel: PanelContainer = %ToolbarPanel
@onready var wordmark: Label = %Wordmark
@onready var version_badge: PanelContainer = %VersionBadge
@onready var version_label: Label = %VersionLabel
@onready var divider_primary: ColorRect = %DividerPrimary
@onready var theme_label: Label = %ThemeLabel
@onready var theme_picker: OptionButton = %ThemePicker
@onready var divider_secondary: ColorRect = %DividerSecondary
@onready var interactive_tasks_toggle: CheckButton = %InteractiveTasksToggle
@onready var wrap_code_toggle: CheckButton = %WrapCodeToggle
@onready var live_editor_toggle: CheckButton = %LiveEditorToggle
@onready var divider_tertiary: ColorRect = %DividerTertiary
@onready var typewriter_button: Button = %TypewriterButton
@onready var typewriter_slider: HSlider = %TypewriterSlider
@onready var typewriter_value: Label = %TypewriterValue
@onready var sidebar_panel: PanelContainer = %SidebarPanel
@onready var sidebar_heading: Label = %SidebarHeading
@onready var document_list: ItemList = %DocumentList
@onready var sidebar_footer: Label = %SidebarFooter
@onready var editor_panel: PanelContainer = %EditorPanel
@onready var editor_heading: Label = %EditorHeading
@onready var editor_stats: Label = %EditorStats
@onready var source_editor: CodeEdit = %SourceEditor
@onready var preview_panel: PanelContainer = %PreviewPanel
@onready var preview_scroll: ScrollContainer = %PreviewScroll
@onready var preview: MarkdownView = %Preview
@onready var status_bar: PanelContainer = %StatusBar
@onready var status_dot: Panel = %StatusDot
@onready var status_event: Label = %StatusEvent
@onready var status_stats: Label = %StatusStats


## Wires the procedural image resolver, loads presets and sample documents, and
## renders the initial showcase state.
func _ready() -> void:
	preview.image_resolver = Callable(MarkdownDemoIcons, "resolve")
	preview.code_block_wrap = wrap_code_toggle.button_pressed
	_build_presets()
	_load_samples()
	_connect_controls()
	_apply_preset(0)
	if not _sample_paths.is_empty():
		document_list.select(0)
		_show_document(0)


## Advances the typewriter reveal animation by updating
## [member MarkdownView.visible_ratio] each frame while playback is active.
func _process(delta: float) -> void:
	if not _typewriter_playing:
		return

	var total_characters: int = preview.get_total_character_count()
	if total_characters <= 0:
		_stop_typewriter(true)
		return

	_typewriter_elapsed_characters += delta * TYPEWRITER_CHARACTERS_PER_SECOND
	var next_ratio: float = clampf(
			_typewriter_elapsed_characters / float(total_characters),
			0.0,
			1.0
	)
	_apply_typewriter_ratio(next_ratio)
	if next_ratio >= 1.0:
		_stop_typewriter(true)


## Selects and displays the sample document at [param index].
func select_document(index: int) -> void:
	if index < 0 or index >= _sample_paths.size():
		return
	document_list.select(index)
	_show_document(index)


## Selects and applies the harmonized theme preset at [param index].
func select_preset(index: int) -> void:
	if index < 0 or index >= _presets.size():
		return
	theme_picker.select(index)
	_apply_preset(index)


## Populates the toolbar theme picker from [method MarkdownDemoPresets.all].
func _build_presets() -> void:
	_presets = MarkdownDemoPresets.all()
	for preset: MarkdownDemoPresets.Preset in _presets:
		theme_picker.add_item(preset.title)


## Discovers sorted [code].md[/code] sample documents in [constant SAMPLE_DIRECTORY].
func _load_samples() -> void:
	var file_names: PackedStringArray = DirAccess.get_files_at(SAMPLE_DIRECTORY)
	file_names.sort()
	for file_name: String in file_names:
		if not file_name.ends_with(".md"):
			continue
		_sample_file_names.append(file_name)
		_sample_paths.append("%s/%s" % [SAMPLE_DIRECTORY, file_name])
		var added_index: int = document_list.add_item(_title_for(file_name))
		assert(added_index >= 0, "could not add a document to the list")


## Maps a sample filename to a display title for the sidebar list.
func _title_for(file_name: String) -> String:
	match file_name:
		"01_release_notes.md":
			return "Patch Notes"
		"02_quest_log.md":
			return "RPG Quest Log"
		"03_alerts_and_formatting.md":
			return "Alerts & Formatting"
		"04_live_playground.md":
			return "Live Playground"
		_:
			var stem: String = file_name.trim_suffix(".md")
			var underscore_index: int = stem.find("_")
			if underscore_index >= 0 and stem.substr(0, underscore_index).is_valid_int():
				stem = stem.substr(underscore_index + 1)
			return stem.replace("_", " ").capitalize()


## Connects all toolbar, editor, sidebar, and [MarkdownView] signals.
func _connect_controls() -> void:
	var document_connected: int = document_list.item_selected.connect(_on_document_selected)
	var theme_connected: int = theme_picker.item_selected.connect(_on_theme_selected)
	var tasks_connected: int = interactive_tasks_toggle.toggled.connect(
			_on_interactive_tasks_toggled
	)
	var wrap_connected: int = wrap_code_toggle.toggled.connect(_on_wrap_code_toggled)
	var editor_toggle_connected: int = live_editor_toggle.toggled.connect(_on_live_editor_toggled)
	var typewriter_button_connected: int = typewriter_button.pressed.connect(_on_typewriter_pressed)
	var typewriter_slider_connected: int = typewriter_slider.value_changed.connect(
			_on_typewriter_slider_changed
	)
	var editor_changed_connected: int = source_editor.text_changed.connect(
			_on_source_editor_changed
	)
	var link_clicked_connected: int = preview.link_clicked.connect(_on_link_clicked)
	var link_hovered_connected: int = preview.link_hovered.connect(_on_link_hovered)
	var task_toggled_connected: int = preview.task_toggled.connect(_on_task_toggled)
	var layout_updated_connected: int = preview.layout_updated.connect(_on_layout_updated)
	assert(
			(
					document_connected == OK
					and theme_connected == OK
					and tasks_connected == OK
					and wrap_connected == OK
					and editor_toggle_connected == OK
					and typewriter_button_connected == OK
					and typewriter_slider_connected == OK
					and editor_changed_connected == OK
					and link_clicked_connected == OK
					and link_hovered_connected == OK
					and task_toggled_connected == OK
					and layout_updated_connected == OK
			),
			"could not wire demo controls"
	)


## Loads the sample document at [param index] into both the live editor and
## [MarkdownView], resetting scroll position and typewriter state.
func _show_document(index: int) -> void:
	_stop_typewriter(true)
	var file_name: String = _sample_file_names[index]
	var source: String = FileAccess.get_file_as_string(_sample_paths[index])

	var enable_tasks: bool = file_name.contains("quest") or file_name.contains("playground")
	interactive_tasks_toggle.set_pressed_no_signal(enable_tasks)
	preview.interactive_tasks = enable_tasks

	var show_editor: bool = file_name.contains("playground")
	if show_editor and not live_editor_toggle.button_pressed:
		live_editor_toggle.set_pressed_no_signal(true)
		editor_panel.visible = true

	_syncing_editor = true
	source_editor.text = source
	_syncing_editor = false
	_update_editor_stats()

	preview.markdown = source
	preview_scroll.scroll_vertical = 0
	_set_event_message("Loaded %s — hover or click links, toggle tasks, or edit live." % file_name)
	_update_document_stats()


## Applies the [MarkdownTheme] and harmonized window chrome colors of the preset
## at [param index].
func _apply_preset(index: int) -> void:
	var preset: MarkdownDemoPresets.Preset = _presets[index]
	background.color = preset.window_background
	preset.apply_to(preview)

	# Panel surfaces (borders-only depth system on a 4px grid)
	toolbar_panel.add_theme_stylebox_override(
			"panel",
			_make_surface_box(preset.sidebar_surface, preset.border_color, 8)
	)
	sidebar_panel.add_theme_stylebox_override(
			"panel",
			_make_surface_box(preset.sidebar_surface, preset.border_color, 8)
	)
	editor_panel.add_theme_stylebox_override(
			"panel",
			_make_surface_box(preset.editor_surface, preset.border_color, 8)
	)
	preview_panel.add_theme_stylebox_override(
			"panel",
			_make_surface_box(preset.card_surface, preset.border_color, 8)
	)
	status_bar.add_theme_stylebox_override(
			"panel",
			_make_surface_box(preset.sidebar_surface, preset.border_color, 6)
	)

	# Version badge & status indicator
	var badge_box: StyleBoxFlat = _make_surface_box(
			Color(preset.accent_color.r, preset.accent_color.g, preset.accent_color.b, 0.16),
			Color(preset.accent_color.r, preset.accent_color.g, preset.accent_color.b, 0.40),
			4
	)
	badge_box.content_margin_left = 6.0
	badge_box.content_margin_right = 6.0
	badge_box.content_margin_top = 1.0
	badge_box.content_margin_bottom = 2.0
	version_badge.add_theme_stylebox_override("panel", badge_box)
	version_label.add_theme_color_override("font_color", preset.accent_color)

	var dot_box: StyleBoxFlat = StyleBoxFlat.new()
	dot_box.bg_color = preset.accent_color
	dot_box.set_corner_radius_all(4)
	status_dot.add_theme_stylebox_override("panel", dot_box)

	# Dividers
	var divider_color: Color = preset.border_color
	divider_primary.color = divider_color
	divider_secondary.color = divider_color
	divider_tertiary.color = divider_color

	# Typography colors
	wordmark.add_theme_color_override("font_color", preset.chrome_text)
	theme_label.add_theme_color_override("font_color", preset.muted_text)
	typewriter_value.add_theme_color_override("font_color", preset.muted_text)
	sidebar_heading.add_theme_color_override("font_color", preset.muted_text)
	sidebar_footer.add_theme_color_override("font_color", preset.muted_text)
	editor_heading.add_theme_color_override("font_color", preset.muted_text)
	editor_stats.add_theme_color_override("font_color", preset.muted_text)
	status_event.add_theme_color_override("font_color", preset.chrome_text)
	status_stats.add_theme_color_override("font_color", preset.muted_text)

	# Interactive toolbar controls
	_style_button_control(theme_picker, preset)
	_style_popup_menu(theme_picker.get_popup(), preset)
	_style_button_control(typewriter_button, preset)
	_style_slider_control(typewriter_slider, preset)
	_style_toggle_control(interactive_tasks_toggle, preset)
	_style_toggle_control(wrap_code_toggle, preset)
	_style_toggle_control(live_editor_toggle, preset)

	# Sidebar ItemList
	var transparent_panel: StyleBoxEmpty = StyleBoxEmpty.new()
	var selected_item_box: StyleBoxFlat = _make_surface_box(
			Color(preset.accent_color.r, preset.accent_color.g, preset.accent_color.b, 0.16),
			Color(preset.accent_color.r, preset.accent_color.g, preset.accent_color.b, 0.45),
			6
	)
	selected_item_box.content_margin_left = 10.0
	selected_item_box.content_margin_right = 10.0
	selected_item_box.content_margin_top = 6.0
	selected_item_box.content_margin_bottom = 6.0
	var hovered_item_box: StyleBoxFlat = _make_surface_box(
			Color(preset.chrome_text.r, preset.chrome_text.g, preset.chrome_text.b, 0.06),
			Color(0.0, 0.0, 0.0, 0.0),
			6
	)
	hovered_item_box.content_margin_left = 10.0
	hovered_item_box.content_margin_right = 10.0
	hovered_item_box.content_margin_top = 6.0
	hovered_item_box.content_margin_bottom = 6.0

	document_list.add_theme_stylebox_override("panel", transparent_panel)
	document_list.add_theme_stylebox_override("focus", transparent_panel)
	document_list.add_theme_stylebox_override("selected", selected_item_box)
	document_list.add_theme_stylebox_override("selected_focus", selected_item_box)
	document_list.add_theme_stylebox_override("hovered", hovered_item_box)
	document_list.add_theme_color_override("guide_color", Color(0.0, 0.0, 0.0, 0.0))
	document_list.add_theme_color_override("font_color", preset.chrome_text)
	document_list.add_theme_color_override("font_hovered_color", preset.chrome_text)
	document_list.add_theme_color_override("font_selected_color", preset.chrome_text)

	# Live source editor
	var editor_box: StyleBoxFlat = _make_surface_box(
			preset.editor_surface,
			Color(preset.border_color.r, preset.border_color.g, preset.border_color.b, 0.6),
			6
	)
	editor_box.content_margin_left = 10.0
	editor_box.content_margin_right = 10.0
	editor_box.content_margin_top = 8.0
	editor_box.content_margin_bottom = 8.0
	source_editor.add_theme_stylebox_override("normal", editor_box)
	source_editor.add_theme_stylebox_override("focus", editor_box)
	source_editor.add_theme_color_override("font_color", preset.chrome_text)
	source_editor.add_theme_color_override("line_number_color", preset.muted_text)
	source_editor.add_theme_color_override("caret_color", preset.accent_color)
	source_editor.add_theme_color_override(
			"selection_color",
			Color(preset.accent_color.r, preset.accent_color.g, preset.accent_color.b, 0.25)
	)

	sidebar_footer.text = "Theme: %s\nNative C++ MarkdownView" % preset.title


## Creates a flat panel [StyleBoxFlat] with a 1px border (when opaque) and
## uniform corner radius.
func _make_surface_box(fill: Color, border: Color, radius: int) -> StyleBoxFlat:
	var box: StyleBoxFlat = StyleBoxFlat.new()
	box.bg_color = fill
	box.border_color = border
	if border.a > 0.0:
		box.set_border_width_all(1)
	box.set_corner_radius_all(radius)
	return box


## Applies harmonized normal, hover, pressed, and focus theme overrides to a
## toolbar [Button].
func _style_button_control(button: Button, preset: MarkdownDemoPresets.Preset) -> void:
	var normal_box: StyleBoxFlat = _make_surface_box(preset.card_surface, preset.border_color, 6)
	normal_box.content_margin_left = 10.0
	normal_box.content_margin_right = 10.0
	normal_box.content_margin_top = 4.0
	normal_box.content_margin_bottom = 4.0

	var hover_box: StyleBoxFlat = _make_surface_box(
			preset.card_surface.lerp(preset.accent_color, 0.10),
			preset.accent_color,
			6
	)
	hover_box.content_margin_left = 10.0
	hover_box.content_margin_right = 10.0
	hover_box.content_margin_top = 4.0
	hover_box.content_margin_bottom = 4.0

	var pressed_box: StyleBoxFlat = _make_surface_box(
			preset.card_surface.lerp(preset.accent_color, 0.18),
			preset.accent_color,
			6
	)
	pressed_box.content_margin_left = 10.0
	pressed_box.content_margin_right = 10.0
	pressed_box.content_margin_top = 4.0
	pressed_box.content_margin_bottom = 4.0

	button.add_theme_stylebox_override("normal", normal_box)
	button.add_theme_stylebox_override("hover", hover_box)
	button.add_theme_stylebox_override("pressed", pressed_box)
	button.add_theme_stylebox_override("focus", hover_box)
	button.add_theme_color_override("font_color", preset.chrome_text)
	button.add_theme_color_override("font_hover_color", preset.chrome_text)
	button.add_theme_color_override("font_pressed_color", preset.chrome_text)
	button.add_theme_color_override("font_focus_color", preset.chrome_text)


## Styles a toolbar [CheckButton] toggle without a surrounding box frame.
func _style_toggle_control(toggle: CheckButton, preset: MarkdownDemoPresets.Preset) -> void:
	var empty_style: StyleBoxEmpty = StyleBoxEmpty.new()
	toggle.add_theme_stylebox_override("normal", empty_style)
	toggle.add_theme_stylebox_override("hover", empty_style)
	toggle.add_theme_stylebox_override("pressed", empty_style)
	toggle.add_theme_stylebox_override("hover_pressed", empty_style)
	toggle.add_theme_stylebox_override("focus", empty_style)
	toggle.add_theme_color_override("font_color", preset.muted_text)
	toggle.add_theme_color_override("font_hover_color", preset.chrome_text)
	toggle.add_theme_color_override("font_pressed_color", preset.chrome_text)
	toggle.add_theme_color_override("font_hover_pressed_color", preset.chrome_text)
	toggle.add_theme_color_override("font_focus_color", preset.chrome_text)


## Applies theme overrides to the theme picker's dropdown [PopupMenu].
func _style_popup_menu(popup: PopupMenu, preset: MarkdownDemoPresets.Preset) -> void:
	var panel_box: StyleBoxFlat = _make_surface_box(preset.card_surface, preset.border_color, 6)
	panel_box.content_margin_left = 6.0
	panel_box.content_margin_right = 6.0
	panel_box.content_margin_top = 6.0
	panel_box.content_margin_bottom = 6.0
	var hover_box: StyleBoxFlat = _make_surface_box(
			Color(preset.accent_color.r, preset.accent_color.g, preset.accent_color.b, 0.18),
			Color(0.0, 0.0, 0.0, 0.0),
			4
	)
	popup.add_theme_stylebox_override("panel", panel_box)
	popup.add_theme_stylebox_override("hover", hover_box)
	popup.add_theme_color_override("font_color", preset.chrome_text)
	popup.add_theme_color_override("font_hover_color", preset.chrome_text)


## Applies track and filled-grabber styling to the typewriter [HSlider].
func _style_slider_control(slider: HSlider, preset: MarkdownDemoPresets.Preset) -> void:
	var track_box: StyleBoxFlat = StyleBoxFlat.new()
	track_box.bg_color = preset.border_color
	track_box.set_corner_radius_all(3)
	track_box.content_margin_top = 3.0
	track_box.content_margin_bottom = 3.0

	var fill_box: StyleBoxFlat = StyleBoxFlat.new()
	fill_box.bg_color = preset.accent_color
	fill_box.set_corner_radius_all(3)
	fill_box.content_margin_top = 3.0
	fill_box.content_margin_bottom = 3.0

	slider.add_theme_stylebox_override("slider", track_box)
	slider.add_theme_stylebox_override("grabber_area", fill_box)
	slider.add_theme_stylebox_override("grabber_area_highlight", fill_box)


## Updates [member MarkdownView.visible_ratio], the slider position, and the
## percentage readout without triggering recursive slider callbacks.
func _apply_typewriter_ratio(ratio: float) -> void:
	var clamped_ratio: float = clampf(ratio, 0.0, 1.0)
	preview.visible_ratio = clamped_ratio
	_syncing_slider = true
	typewriter_slider.value = clamped_ratio
	_syncing_slider = false
	typewriter_value.text = "%d%%" % roundi(clamped_ratio * 100.0)


## Stops typewriter playback and optionally restores the full document reveal.
func _stop_typewriter(restore_full: bool) -> void:
	_typewriter_playing = false
	typewriter_button.text = "Typewriter"
	if restore_full:
		_apply_typewriter_ratio(1.0)


## Updates the persistent status bar event text shown when no link is hovered.
func _set_event_message(message: String) -> void:
	_last_event_text = message
	status_event.text = message


## Refreshes the line count indicator above the live Markdown editor.
func _update_editor_stats() -> void:
	editor_stats.text = "%d lines" % source_editor.get_line_count()


## Refreshes character count, task count, and laid-out height metrics in the
## status bar.
func _update_document_stats() -> void:
	var character_count: int = preview.get_total_character_count()
	var task_count: int = preview.get_task_count()
	var height_pixels: int = roundi(preview.get_combined_minimum_size().y)
	status_stats.text = "%d chars · %d tasks · %d px" % [character_count, task_count, height_pixels]


func _on_document_selected(index: int) -> void:
	_show_document(index)


func _on_theme_selected(index: int) -> void:
	_apply_preset(index)
	_set_event_message("Switched theme preset to %s." % _presets[index].title)


func _on_interactive_tasks_toggled(enabled: bool) -> void:
	preview.interactive_tasks = enabled
	_set_event_message("interactive_tasks = %s" % ("true" if enabled else "false"))


func _on_wrap_code_toggled(enabled: bool) -> void:
	preview.code_block_wrap = enabled
	_set_event_message("code_block_wrap = %s" % ("true" if enabled else "false"))


func _on_live_editor_toggled(enabled: bool) -> void:
	editor_panel.visible = enabled
	_set_event_message("Live Markdown editor %s." % ("opened" if enabled else "closed"))


func _on_typewriter_pressed() -> void:
	if _typewriter_playing:
		_stop_typewriter(true)
		_set_event_message("Typewriter reveal reset to 100%.")
		return

	_typewriter_elapsed_characters = 0.0
	_typewriter_playing = true
	typewriter_button.text = "Reset"
	_apply_typewriter_ratio(0.0)
	_set_event_message("Playing typewriter reveal via MarkdownView.visible_ratio...")


func _on_typewriter_slider_changed(value: float) -> void:
	if _syncing_slider:
		return
	_typewriter_playing = false
	typewriter_button.text = "Typewriter"
	preview.visible_ratio = value
	typewriter_value.text = "%d%%" % roundi(value * 100.0)


func _on_source_editor_changed() -> void:
	if _syncing_editor:
		return
	preview.markdown = source_editor.text
	_update_editor_stats()
	_update_document_stats()


func _on_link_clicked(url: String) -> void:
	_set_event_message('link_clicked("%s")' % url)


func _on_link_hovered(url: String) -> void:
	if url.is_empty():
		status_event.text = _last_event_text
	else:
		status_event.text = 'link_hovered("%s")' % url


func _on_task_toggled(task_index: int, checked: bool) -> void:
	_set_event_message(
			"task_toggled(index=%d, checked=%s)" % [task_index, "true" if checked else "false"]
	)


func _on_layout_updated() -> void:
	_update_document_stats()
