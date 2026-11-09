# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends GDTest
## Inline parsing, asserted as an indented rendering of the fully expanded tree.


func get_suite_name() -> String:
	return "MarkdownInlineParser"


func test_expands_inline_nodes(source: String, expected: String) -> void:
	var document: MarkdownDocument = Markdown.parse(source)
	expect_equal(document.root.to_debug_string(), expected)


func test_expands_inline_nodes__cases() -> Dictionary[String, Variant]:
	return {
		"plain text": ["words", _tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=words"])],
		"asterisk emphasis": [
			"*emphasis*",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tEMPHASIS", "\t\t\tTEXT text=emphasis"]),
		],
		"underscore emphasis": [
			"_emphasis_",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tEMPHASIS", "\t\t\tTEXT text=emphasis"]),
		],
		"strong emphasis": [
			"**strong**",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tSTRONG", "\t\t\tTEXT text=strong"]),
		],
		"double underscore is strong": [
			"__strong__",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tSTRONG", "\t\t\tTEXT text=strong"]),
		],
		"triple delimiter nests strong and emphasis": [
			"***both***",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tSTRONG",
						"\t\t\tEMPHASIS",
						"\t\t\t\tTEXT text=both",
					]
			),
		],
		"nested emphasis of the same character": [
			"*outer *inner* tail*",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tEMPHASIS",
						"\t\t\tTEXT text=outer ",
						"\t\t\tEMPHASIS",
						"\t\t\t\tTEXT text=inner",
						"\t\t\tTEXT text= tail",
					]
			),
		],
		"strikethrough": [
			"~~struck~~",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tSTRIKETHROUGH", "\t\t\tTEXT text=struck"]),
		],
		"a single tilde is literal": [
			"~single~",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=~single~"]),
		],
		"underscores inside a word are literal": [
			"snake_case_name",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=snake_case_name"]),
		],
		"unclosed emphasis is literal": [
			"*unclosed",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=*unclosed"]),
		],
		"emphasis needs a non space opener": [
			"a * b * c",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=a * b * c"]),
		],
		"code span": ["`code`", _tree(["DOCUMENT", "\tPARAGRAPH", "\t\tCODE_SPAN text=code"])],
		"code span keeps markdown literal": [
			"`*not emphasis*`",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tCODE_SPAN text=*not emphasis*"]),
		],
		"double backticks hold a backtick": [
			"`` a ` b ``",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tCODE_SPAN text=a ` b"]),
		],
		"unclosed code span is literal": [
			"`unclosed",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=`unclosed"]),
		],
		"inline link": [
			"[text](https://example.com)",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tLINK destination=https://example.com",
						"\t\t\tTEXT text=text",
					]
			),
		],
		"inline link with a title": [
			'[text](https://example.com "Title")',
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tLINK destination=https://example.com title=Title",
						"\t\t\tTEXT text=text",
					]
			),
		],
		"link destination in angle brackets may hold spaces": [
			"[text](<https://example.com/a b>)",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tLINK destination=https://example.com/a b",
						"\t\t\tTEXT text=text",
					]
			),
		],
		"link label is parsed as inline markdown": [
			"[**bold** link](https://example.com)",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tLINK destination=https://example.com",
						"\t\t\tSTRONG",
						"\t\t\t\tTEXT text=bold",
						"\t\t\tTEXT text= link",
					]
			),
		],
		"brackets without a target stay literal": [
			"[brackets]",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=[brackets]"]),
		],
		"angle autolink": [
			"<https://example.com>",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tLINK destination=https://example.com",
						"\t\t\tTEXT text=https://example.com",
					]
			),
		],
		"email autolink": [
			"<someone@example.com>",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tLINK destination=mailto:someone@example.com",
						"\t\t\tTEXT text=someone@example.com",
					]
			),
		],
		"bare url autolink": [
			"see https://example.com now",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tTEXT text=see ",
						"\t\tLINK destination=https://example.com",
						"\t\t\tTEXT text=https://example.com",
						"\t\tTEXT text= now",
					]
			),
		],
		"bare www autolink gains a scheme": [
			"see www.example.com now",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tTEXT text=see ",
						"\t\tLINK destination=https://www.example.com",
						"\t\t\tTEXT text=www.example.com",
						"\t\tTEXT text= now",
					]
			),
		],
		"trailing punctuation leaves the autolink": [
			"see https://example.com.",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tTEXT text=see ",
						"\t\tLINK destination=https://example.com",
						"\t\t\tTEXT text=https://example.com",
						"\t\tTEXT text=.",
					]
			),
		],
		"a word starting with h is not an autolink": [
			"however",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=however"]),
		],
		"image": [
			"![alt](res://missing.png)",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tIMAGE destination=res://missing.png text=alt"]),
		],
		"backslash escape": [
			"\\*literal\\*",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=*literal*"]),
		],
		"backslash before a letter stays literal": [
			"\\n",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=\\\\n"]),
		],
		"two trailing spaces are a hard break": [
			"one  \ntwo",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tTEXT text=one",
						"\t\tLINE_BREAK",
						"\t\tTEXT text=two",
					]
			),
		],
		"trailing backslash is a hard break": [
			"one\\\ntwo",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tTEXT text=one",
						"\t\tLINE_BREAK",
						"\t\tTEXT text=two",
					]
			),
		],
		"a plain newline is a soft break": [
			"one\ntwo",
			_tree(
					[
						"DOCUMENT",
						"\tPARAGRAPH",
						"\t\tTEXT text=one",
						"\t\tSOFT_BREAK",
						"\t\tTEXT text=two",
					]
			),
		],
		"named entity": ["&hellip;", _tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=…"])],
		"decimal entity": ["&#65;", _tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=A"])],
		"hexadecimal entity": ["&#x42;", _tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=B"])],
		"unknown entity stays literal": [
			"&nope;",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=&nope;"]),
		],
		"raw html is literal text": [
			"a <b>tag</b> here",
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=a <b>tag</b> here"]),
		],
		"headings expand inline markdown": [
			"# A *title*",
			_tree(
					[
						"DOCUMENT",
						"\tHEADING level=1",
						"\t\tTEXT text=A ",
						"\t\tEMPHASIS",
						"\t\t\tTEXT text=title",
					]
			),
		],
		"code blocks are left alone": [
			"```\n*not emphasis*\n```",
			_tree(["DOCUMENT", "\tCODE_BLOCK text=*not emphasis*"]),
		],
	}


func test_resolves_a_full_reference_link() -> void:
	var document: MarkdownDocument = Markdown.parse(
			'See [the docs][docs].\n\n[docs]: https://example.com "Docs"'
	)
	var link: MarkdownNode = document.root.children[0].children[1]

	expect_equal(link.kind, MarkdownNode.Kind.LINK)
	expect_equal(link.link_destination, "https://example.com")
	expect_equal(link.link_title, "Docs")


func test_resolves_a_collapsed_reference_link() -> void:
	var document: MarkdownDocument = Markdown.parse("See [docs][].\n\n[docs]: https://example.com")
	var link: MarkdownNode = document.root.children[0].children[1]

	expect_equal(link.link_destination, "https://example.com")


func test_resolves_a_shortcut_reference_link() -> void:
	var document: MarkdownDocument = Markdown.parse("See [docs].\n\n[docs]: https://example.com")
	var link: MarkdownNode = document.root.children[0].children[1]

	expect_equal(link.link_destination, "https://example.com")


func test_matches_a_reference_label_case_insensitively() -> void:
	var document: MarkdownDocument = Markdown.parse("See [DOCS].\n\n[docs]: https://example.com")
	var link: MarkdownNode = document.root.children[0].children[1]

	expect_equal(link.link_destination, "https://example.com")


func test_leaves_an_undefined_reference_as_text() -> void:
	var document: MarkdownDocument = Markdown.parse("See [missing].")

	expect_equal(
			document.root.to_debug_string(),
			_tree(["DOCUMENT", "\tPARAGRAPH", "\t\tTEXT text=See [missing]."])
	)


func test_expands_table_cells() -> void:
	var document: MarkdownDocument = Markdown.parse("| *a* |\n| --- |\n| `b` |")
	var table: MarkdownNode = document.root.children[0]

	expect_equal(table.children[0].children[0].children[0].kind, MarkdownNode.Kind.EMPHASIS)
	expect_equal(table.children[1].children[0].children[0].kind, MarkdownNode.Kind.CODE_SPAN)


## Joins the lines of an expected tree into the form
## [method MarkdownNode.to_debug_string] returns.
func _tree(lines: Array[String]) -> String:
	return MarkdownScanner.join("\n", lines)
