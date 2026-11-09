extends GDTest
## End-to-end conversion, from Markdown source to the BBCode a RichTextLabel
## receives. Every case uses the same deterministic style so the expected
## strings stay readable.

## The rendering of a one-line code block under [method _style], spelled out
## once because several cases assert on it.
const CODE_BLOCK: String = (
		"[table=1][cell bg=#000000ff,#000000ff padding=10,10,10,10]"
		+ "[color=#ffffffff][code]a[/code][/color][/cell][/table]"
)


func get_suite_name() -> String:
	return "MarkdownBbcodeEmitter"


func test_converts_to_bbcode(source: String, expected: String) -> void:
	expect_equal(Markdown.to_bbcode(source, _style()), expected)


func test_converts_to_bbcode__cases() -> Dictionary[String, Variant]:
	return {
		"paragraph": ["words", "words"],
		"heading": ["# Title", "[font_size=32][b]Title[/b][/font_size]"],
		"heading level three": ["### Title", "[font_size=24][b]Title[/b][/font_size]"],
		"emphasis": ["*a*", "[i]a[/i]"],
		"strong": ["**a**", "[b]a[/b]"],
		"strong emphasis": ["***a***", "[b][i]a[/i][/b]"],
		"strikethrough": ["~~a~~", "[s]a[/s]"],
		"code span": [
			"`a`",
			"[bgcolor=#000000ff][color=#ffffffff][code]a[/code][/color][/bgcolor]",
		],
		"link": ["[a](https://e.com)", "[url=https://e.com]a[/url]"],
		"link with a title becomes a tooltip": [
			'[a](https://e.com "Tip")',
			"[url=https://e.com][hint=Tip]a[/hint][/url]",
		],
		"a space in a destination is encoded": [
			"[a](<https://e.com/x y>)",
			"[url=https://e.com/x%20y]a[/url]",
		],
		"unordered list": ["- a\n- b", "[indent]• a[/indent]\n[indent]• b[/indent]"],
		"ordered list numbers its items": [
			"1. a\n2. b",
			"[indent]1. a[/indent]\n[indent]2. b[/indent]",
		],
		"ordered list honors its start number": [
			"7. a\n8. b",
			"[indent]7. a[/indent]\n[indent]8. b[/indent]",
		],
		"task list uses glyphs": ["- [ ] a\n- [x] b", "[indent]☐ a[/indent]\n[indent]☑ b[/indent]"],
		"loose list separates its items": [
			"- a\n\n- b",
			"[indent]• a[/indent]\n\n[indent]• b[/indent]",
		],
		"nested list indents twice": ["- a\n\t- b", "[indent]• a\n[indent]• b[/indent][/indent]"],
		"block quote": ["> a", "[indent][color=#808080ff]a[/color][/indent]"],
		"thematic break": ["---", "[center][color=#808080ff]--[/color][/center]"],
		"fenced code block": ["```\na\n```", CODE_BLOCK],
		"indented code block": ["    a", CODE_BLOCK],
		"blocks are separated by a blank line": [
			"# Title\n\ntext",
			"[font_size=32][b]Title[/b][/font_size]\n\ntext",
		],
		"soft break becomes a space": ["a\nb", "a b"],
		"hard break becomes a newline": ["a  \nb", "a\nb"],
		"a bracket in text is escaped": ["a [b] c", "a [lb]b] c"],
		"a bracket in code is escaped": [
			"`[b]`",
			"[bgcolor=#000000ff][color=#ffffffff][code][lb]b][/code][/color][/bgcolor]",
		],
		"a missing local image falls back to its alternate text": [
			"![alt](res://missing.png)",
			"alt",
		],
		"a remote image links its alternate text": [
			"![alt](https://e.com/a.png)",
			"[url=https://e.com/a.png]alt[/url]",
		],
		"table": [
			"| a | b |\n| --- | --: |\n| 1 | 2 |",
			(
					"[table=2][cell padding=10,4,10,4][b]a[/b][/cell]"
					+ "[cell padding=10,4,10,4][right][b]b[/b][/right][/cell]"
					+ "[cell padding=10,4,10,4]1[/cell]"
					+ "[cell padding=10,4,10,4][right]2[/right][/cell][/table]"
			),
		],
		"empty source": ["", ""],
	}


func test_renders_a_local_image_that_resolves() -> void:
	var source: String = "![icon](res://addons/gdmarkdown/icons/markdown_label.svg)"

	expect_equal(
			Markdown.to_bbcode(source, _style()),
			"[img]res://addons/gdmarkdown/icons/markdown_label.svg[/img]"
	)


func test_scales_an_image_to_the_configured_width() -> void:
	var style: MarkdownStyle = _style()
	style.image_width = 64

	expect_equal(
			Markdown.to_bbcode("![icon](res://addons/gdmarkdown/icons/markdown_label.svg)", style),
			"[img=64]res://addons/gdmarkdown/icons/markdown_label.svg[/img]"
	)


func test_colors_links_when_the_style_asks_for_it() -> void:
	var style: MarkdownStyle = _style()
	style.color_links = true
	style.link_color = Color(0.0, 0.0, 1.0, 1.0)

	expect_equal(
			Markdown.to_bbcode("[a](https://e.com)", style),
			"[url=https://e.com][color=#0000ffff]a[/color][/url]"
	)


func test_preserves_soft_breaks_when_the_style_asks_for_it() -> void:
	var style: MarkdownStyle = _style()
	style.preserve_soft_breaks = true

	expect_equal(Markdown.to_bbcode("a\nb", style), "a\nb")


func test_shows_the_code_language_when_the_style_asks_for_it() -> void:
	var style: MarkdownStyle = _style()
	style.show_code_language = true

	expect_equal(
			Markdown.to_bbcode("```gdscript\na\n```", style),
			"[color=#808080ff][i]gdscript[/i][/color]\n" + CODE_BLOCK
	)


func test_falls_back_to_a_wrapping_code_background() -> void:
	var style: MarkdownStyle = _style()
	style.solid_code_background = false

	expect_equal(
			Markdown.to_bbcode("```\na\n```", style),
			"[bgcolor=#000000ff][color=#ffffffff][code]a[/code][/color][/bgcolor]"
	)


func test_renders_a_table_as_plain_lines_when_tables_are_disabled() -> void:
	var style: MarkdownStyle = _style()
	style.render_tables = false

	expect_equal(
			Markdown.to_bbcode("| a | b |\n| --- | --- |\n| 1 | 2 |", style),
			"[b]a[/b] | [b]b[/b]\n1 | 2"
	)


func test_uses_default_style_when_none_is_given() -> void:
	expect_equal(Markdown.to_bbcode("- a"), "[indent]• a[/indent]")


func test_escapes_only_the_opening_bracket() -> void:
	expect_equal(MarkdownBbcodeEmitter.escape("a[b]c"), "a[lb]b]c")


## A style whose colors and glyphs make the expected BBCode above readable:
## black on white for code, no link coloring, and a two-character break.
func _style() -> MarkdownStyle:
	var style: MarkdownStyle = MarkdownStyle.new()
	style.code_background_color = Color(0.0, 0.0, 0.0, 1.0)
	style.code_text_color = Color(1.0, 1.0, 1.0, 1.0)
	style.quote_color = Color(0.5, 0.5, 0.5, 1.0)
	style.color_links = false
	style.thematic_break_text = "--"
	return style
