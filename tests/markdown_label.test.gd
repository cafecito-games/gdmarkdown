extends GDTest
## The node's own behavior: rendering on assignment, re-rendering when a shared
## style changes, and relaying link clicks.


func get_suite_name() -> String:
	return "MarkdownLabel"


func test_enables_bbcode() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())

	expect_true(label.bbcode_enabled)


func test_renders_markdown_on_assignment() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())

	label.markdown = "**bold**"

	expect_equal(label.text, "[b]bold[/b]")


func test_keeps_the_markdown_source() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())

	label.markdown = "# Title"

	expect_equal(label.markdown, "# Title")


func test_renders_an_empty_source_as_empty_text() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())

	label.markdown = "**bold**"
	label.markdown = ""

	expect_empty(label.text)


func test_applies_an_assigned_style() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())
	var style: MarkdownStyle = MarkdownStyle.new()
	style.unordered_bullet = ">>"

	label.markdown_style = style
	label.markdown = "- item"

	expect_equal(label.text, "[indent]>> item[/indent]")


func test_re_renders_when_the_style_changes() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())
	var style: MarkdownStyle = MarkdownStyle.new()
	label.markdown_style = style
	label.markdown = "- item"

	style.unordered_bullet = "+"

	expect_equal(label.text, "[indent]+ item[/indent]")


func test_stops_listening_to_a_replaced_style() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())
	var first: MarkdownStyle = MarkdownStyle.new()
	var second: MarkdownStyle = MarkdownStyle.new()
	second.unordered_bullet = "-"
	label.markdown_style = first
	label.markdown = "- item"
	label.markdown_style = second

	first.unordered_bullet = "!"

	expect_equal(label.text, "[indent]- item[/indent]")


func test_relays_a_link_click() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())
	var clicks: GDTestSignalProbe = watch_signal(label.link_clicked)
	label.markdown = "[a](https://example.com)"

	label.meta_clicked.emit("https://example.com")

	expect_equal(clicks.emission_count(), 1)
	expect_equal(clicks.arguments_at(0), ["https://example.com"])


func test_exposes_the_parsed_document() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())
	label.markdown = "# Title\n\ntext"

	var document: MarkdownDocument = label.get_document()

	expect_equal(document.root.children.size(), 2)
	expect_equal(document.root.children[0].kind, MarkdownNode.Kind.HEADING)


func test_renders_once_added_to_the_tree() -> void:
	var label: MarkdownLabel = autofree(MarkdownLabel.new())
	label.markdown = "*a*"

	get_tree().root.add_child(label)
	await sleep()

	expect_equal(label.text, "[i]a[/i]")
