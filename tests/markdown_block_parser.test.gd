extends GDTest
## Block structure, asserted as an indented rendering of the parsed tree. Leaf
## blocks still hold raw inline Markdown at this stage.


func get_suite_name() -> String:
	return "MarkdownBlockParser"


func test_builds_block_tree(source: String, expected: String) -> void:
	var document: MarkdownDocument = MarkdownBlockParser.parse(source)
	expect_equal(document.root.to_debug_string(), expected)


func test_builds_block_tree__cases() -> Dictionary[String, Variant]:
	return {
		"atx heading": ["# Heading one", _tree(["DOCUMENT", "\tHEADING level=1 text=Heading one"])],
		"atx heading with closing hashes": [
			"### Deep ###",
			_tree(["DOCUMENT", "\tHEADING level=3 text=Deep"]),
		],
		"atx heading needs a space": [
			"#NotAHeading",
			_tree(["DOCUMENT", "\tPARAGRAPH text=#NotAHeading"]),
		],
		"atx heading stops at seven hashes": [
			"####### Seven",
			_tree(["DOCUMENT", "\tPARAGRAPH text=####### Seven"]),
		],
		"setext heading level one": [
			"Title\n===",
			_tree(["DOCUMENT", "\tHEADING level=1 text=Title"]),
		],
		"setext heading level two": [
			"Title\n---",
			_tree(["DOCUMENT", "\tHEADING level=2 text=Title"]),
		],
		"paragraph keeps its wrapping": [
			"A paragraph\nwrapped over lines.",
			_tree(["DOCUMENT", "\tPARAGRAPH text=A paragraph\\nwrapped over lines."]),
		],
		"blank line separates paragraphs": [
			"first\n\nsecond",
			_tree(["DOCUMENT", "\tPARAGRAPH text=first", "\tPARAGRAPH text=second"]),
		],
		"thematic break": ["***", _tree(["DOCUMENT", "\tTHEMATIC_BREAK"])],
		"thematic break with spaces": ["- - -", _tree(["DOCUMENT", "\tTHEMATIC_BREAK"])],
		"unordered list": [
			"- one\n- two",
			_tree(
					[
						"DOCUMENT",
						"\tLIST ordered=false tight=true",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=one",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=two",
					]
			),
		],
		"ordered list": [
			"1. first\n2. second",
			_tree(
					[
						"DOCUMENT",
						"\tLIST ordered=true tight=true start=1",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=first",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=second",
					]
			),
		],
		"ordered list keeps its start number": [
			"3) third\n4) fourth",
			_tree(
					[
						"DOCUMENT",
						"\tLIST ordered=true tight=true start=3",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=third",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=fourth",
					]
			),
		],
		"changing the bullet starts a new list": [
			"- dash\n* star",
			_tree(
					[
						"DOCUMENT",
						"\tLIST ordered=false tight=true",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=dash",
						"\tLIST ordered=false tight=true",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=star",
					]
			),
		],
		"task list": [
			"- [ ] todo\n- [x] done",
			_tree(
					[
						"DOCUMENT",
						"\tLIST ordered=false tight=true",
						"\t\tLIST_ITEM task=UNCHECKED",
						"\t\t\tPARAGRAPH text=todo",
						"\t\tLIST_ITEM task=CHECKED",
						"\t\t\tPARAGRAPH text=done",
					]
			),
		],
		"nested list": [
			"- outer\n\t- inner",
			_tree(
					[
						"DOCUMENT",
						"\tLIST ordered=false tight=true",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=outer",
						"\t\t\tLIST ordered=false tight=true",
						"\t\t\t\tLIST_ITEM",
						"\t\t\t\t\tPARAGRAPH text=inner",
					]
			),
		],
		"blank line between items makes the list loose": [
			"- one\n\n- two",
			_tree(
					[
						"DOCUMENT",
						"\tLIST ordered=false tight=false",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=one",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=two",
					]
			),
		],
		"item with two paragraphs makes the list loose": [
			"- item with\n\n  two paragraphs",
			_tree(
					[
						"DOCUMENT",
						"\tLIST ordered=false tight=false",
						"\t\tLIST_ITEM",
						"\t\t\tPARAGRAPH text=item with",
						"\t\t\tPARAGRAPH text=two paragraphs",
					]
			),
		],
		"block quote": [
			"> quoted\n> more",
			_tree(["DOCUMENT", "\tBLOCK_QUOTE", "\t\tPARAGRAPH text=quoted\\nmore"]),
		],
		"nested block quote": [
			"> outer\n> > inner",
			_tree(
					[
						"DOCUMENT",
						"\tBLOCK_QUOTE",
						"\t\tPARAGRAPH text=outer",
						"\t\tBLOCK_QUOTE",
						"\t\t\tPARAGRAPH text=inner",
					]
			),
		],
		"list inside a block quote": [
			"> - one\n> - two",
			_tree(
					[
						"DOCUMENT",
						"\tBLOCK_QUOTE",
						"\t\tLIST ordered=false tight=true",
						"\t\t\tLIST_ITEM",
						"\t\t\t\tPARAGRAPH text=one",
						"\t\t\tLIST_ITEM",
						"\t\t\t\tPARAGRAPH text=two",
					]
			),
		],
		"fenced code block with a language": [
			"```gdscript\nvar count: int = 1\n```",
			_tree(["DOCUMENT", "\tCODE_BLOCK language=gdscript text=var count: int = 1"]),
		],
		"tilde fenced code block": [
			"~~~\nplain\n~~~",
			_tree(["DOCUMENT", "\tCODE_BLOCK text=plain"]),
		],
		"unclosed fence runs to the end": [
			"```\nstill code",
			_tree(["DOCUMENT", "\tCODE_BLOCK text=still code"]),
		],
		"fenced code keeps markdown literal": [
			"```\n# not a heading\n```",
			_tree(["DOCUMENT", "\tCODE_BLOCK text=# not a heading"]),
		],
		"indented code block": [
			"    indented\n    lines",
			_tree(["DOCUMENT", "\tCODE_BLOCK text=indented\\nlines"]),
		],
		"table with alignments": [
			"| a | b |\n| :-- | --: |\n| 1 | 2 |",
			_tree(
					[
						"DOCUMENT",
						"\tTABLE alignments=[LEFT,RIGHT]",
						"\t\tTABLE_ROW",
						"\t\t\tTABLE_CELL text=a",
						"\t\t\tTABLE_CELL text=b",
						"\t\tTABLE_ROW",
						"\t\t\tTABLE_CELL text=1",
						"\t\t\tTABLE_CELL text=2",
					]
			),
		],
		"short table row is padded": [
			"| a | b |\n| --- | --- |\n| 1 |",
			_tree(
					[
						"DOCUMENT",
						"\tTABLE alignments=[NONE,NONE]",
						"\t\tTABLE_ROW",
						"\t\t\tTABLE_CELL text=a",
						"\t\t\tTABLE_CELL text=b",
						"\t\tTABLE_ROW",
						"\t\t\tTABLE_CELL text=1",
						"\t\t\tTABLE_CELL",
					]
			),
		],
		"pipe table needs a delimiter row": [
			"| a | b |\njust text",
			_tree(["DOCUMENT", "\tPARAGRAPH text=| a | b |\\njust text"]),
		],
		"link reference definition leaves no block": [
			"[label]: https://example.com\n\nSee it.",
			_tree(["DOCUMENT", "\tPARAGRAPH text=See it."]),
		],
		"heading interrupts a paragraph": [
			"text\n# heading",
			_tree(["DOCUMENT", "\tPARAGRAPH text=text", "\tHEADING level=1 text=heading"]),
		],
		"a numbered line does not interrupt a paragraph": [
			"published\n2024. a good year",
			_tree(["DOCUMENT", "\tPARAGRAPH text=published\\n2024. a good year"]),
		],
		"carriage returns are normalized": [
			"one\r\n\r\ntwo",
			_tree(["DOCUMENT", "\tPARAGRAPH text=one", "\tPARAGRAPH text=two"]),
		],
		"empty input yields an empty document": ["", _tree(["DOCUMENT"])],
		"whitespace only input yields an empty document": ["   \n\n  ", _tree(["DOCUMENT"])],
	}


func test_collects_link_reference_definitions() -> void:
	var source: String = '[Label One]: https://example.com "The Title"\n\ntext'
	var document: MarkdownDocument = MarkdownBlockParser.parse(source)
	var definition: MarkdownNode = document.find_link_reference("label one")

	expect_not_null(definition)
	expect_equal(definition.link_destination, "https://example.com")
	expect_equal(definition.link_title, "The Title")


func test_first_definition_of_a_label_wins() -> void:
	var source: String = "[a]: https://first.example\n[a]: https://second.example"
	var document: MarkdownDocument = MarkdownBlockParser.parse(source)

	expect_equal(document.find_link_reference("a").link_destination, "https://first.example")


func test_unwraps_an_angle_bracketed_definition() -> void:
	var document: MarkdownDocument = MarkdownBlockParser.parse("[a]: <https://example.com>")

	expect_equal(document.find_link_reference("a").link_destination, "https://example.com")


## Joins the lines of an expected tree into the form
## [method MarkdownNode.to_debug_string] returns.
func _tree(lines: Array[String]) -> String:
	return MarkdownScanner.join("\n", lines)
