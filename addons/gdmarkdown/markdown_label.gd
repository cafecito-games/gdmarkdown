@tool
@icon("res://addons/gdmarkdown/icons/markdown_label.svg")
class_name MarkdownLabel
extends RichTextLabel
## A [RichTextLabel] that takes Markdown instead of BBCode.
##
## Assign [member markdown] and the label renders it. The conversion runs in the
## editor too, so the inspector shows the result as it is typed. The generated
## BBCode is written to [member RichTextLabel.text]; setting that property
## directly still works and simply replaces the rendered output until
## [member markdown] changes again.

## Emitted when the reader clicks a link. [param target] is the link
## destination exactly as the Markdown wrote it.
signal link_clicked(target: String)

## Markdown source to render.
@export_multiline var markdown: String = "":
	set(value):
		if markdown == value:
			return
		markdown = value
		_render()

## Visual settings for the conversion. Leaving it empty uses the defaults of a
## fresh [MarkdownStyle].
@export var markdown_style: MarkdownStyle = null:
	set(value):
		if markdown_style == value:
			return
		_disconnect_style()
		markdown_style = value
		_connect_style()
		_render()


func _init() -> void:
	bbcode_enabled = true
	if not meta_clicked.is_connected(_on_meta_clicked):
		var status: int = meta_clicked.connect(_on_meta_clicked)
		assert(status == OK, "could not connect meta_clicked")


func _ready() -> void:
	_connect_style()
	_render()


## Returns the parsed document behind the current [member markdown], which is
## useful for building a table of contents or an outline.
func get_document() -> MarkdownDocument:
	return Markdown.parse(markdown)


func _render() -> void:
	bbcode_enabled = true
	text = Markdown.to_bbcode(markdown, markdown_style)


func _connect_style() -> void:
	if markdown_style == null or markdown_style.changed.is_connected(_on_style_changed):
		return
	var status: int = markdown_style.changed.connect(_on_style_changed)
	assert(status == OK, "could not connect to the style's changed signal")


func _disconnect_style() -> void:
	if markdown_style != null and markdown_style.changed.is_connected(_on_style_changed):
		markdown_style.changed.disconnect(_on_style_changed)


func _on_style_changed() -> void:
	_render()


func _on_meta_clicked(meta: Variant) -> void:
	link_clicked.emit(str(meta))
