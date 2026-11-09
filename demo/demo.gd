extends Control
## Interactive showcase for the gdmarkdown addon.
##
## Lists the documents in [code]demo/samples[/code] on the left and renders the
## selected one with a [MarkdownLabel] on the right. The style picker swaps a
## [MarkdownStyle] at runtime, which is what makes the label re-render without
## the document being reassigned.

const SAMPLE_DIRECTORY: String = "res://demo/samples"

var _presets: Array[MarkdownDemoPresets.Preset] = []
var _sample_paths: Array[String] = []

@onready var background: ColorRect = $Background
@onready var style_picker: OptionButton = $Margin/Layout/Header/StylePicker
@onready var status: Label = $Margin/Layout/Status
@onready var document_list: ItemList = $Margin/Layout/Body/Sidebar/SidebarMargin/DocumentList
@onready
var preview: MarkdownLabel = $Margin/Layout/Body/PreviewPanel/PreviewMargin/Scroll/Preview


func _ready() -> void:
	_build_presets()
	_load_samples()
	_connect_controls()
	if not _sample_paths.is_empty():
		document_list.select(0)
		_show_document(0)
	_apply_preset(0)


func _build_presets() -> void:
	_presets = MarkdownDemoPresets.all()
	for preset: MarkdownDemoPresets.Preset in _presets:
		style_picker.add_item(preset.title)


func _load_samples() -> void:
	var file_names: PackedStringArray = DirAccess.get_files_at(SAMPLE_DIRECTORY)
	file_names.sort()
	for file_name: String in file_names:
		if not file_name.ends_with(".md"):
			continue
		_sample_paths.append("%s/%s" % [SAMPLE_DIRECTORY, file_name])
		var added: int = document_list.add_item(_title_for(file_name))
		assert(added >= 0, "could not add a document to the list")


## Turns "01_release_notes.md" into "Release notes".
func _title_for(file_name: String) -> String:
	var stem: String = file_name.trim_suffix(".md")
	var underscore: int = stem.find("_")
	if underscore >= 0 and stem.substr(0, underscore).is_valid_int():
		stem = stem.substr(underscore + 1)
	return stem.replace("_", " ").capitalize()


func _connect_controls() -> void:
	var selected: int = document_list.item_selected.connect(_on_document_selected)
	var picked: int = style_picker.item_selected.connect(_on_style_selected)
	var clicked: int = preview.link_clicked.connect(_on_link_clicked)
	assert(selected == OK and picked == OK and clicked == OK, "could not wire the demo controls")


func _show_document(index: int) -> void:
	preview.markdown = FileAccess.get_file_as_string(_sample_paths[index])
	status.text = "Click a link to see what the host receives."


func _apply_preset(index: int) -> void:
	var preset: MarkdownDemoPresets.Preset = _presets[index]
	background.color = preset.background
	status.add_theme_color_override("font_color", preset.text * Color(1.0, 1.0, 1.0, 0.6))
	preset.apply_to(preview)


func _on_document_selected(index: int) -> void:
	_show_document(index)


func _on_style_selected(index: int) -> void:
	_apply_preset(index)


## The addon never opens a URL on its own, so the demo shows what a host does
## with the destination instead.
func _on_link_clicked(target: String) -> void:
	status.text = 'link_clicked("%s")' % target
