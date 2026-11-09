class_name Markdown
extends RefCounted
## Converts Markdown to BBCode.
##
## Use [MarkdownLabel] to render Markdown in a scene. Use this class directly to
## feed BBCode to a [RichTextLabel] you already own, or to inspect the parsed
## document:
## [codeblock]
## label.bbcode_enabled = true
## label.text = Markdown.to_bbcode("# Title\n\nSome **bold** text.")
## [/codeblock]
##
## Both CommonMark and the GitHub Flavored Markdown extensions that a
## [RichTextLabel] can express are supported: headings, emphasis, strong
## emphasis, strikethrough, code spans and fenced code blocks, block quotes,
## ordered, unordered, nested and task lists, links, reference links,
## autolinks, images, thematic breaks, hard line breaks, tables, HTML entities
## and backslash escapes. Raw HTML is rendered as literal text, since a
## [RichTextLabel] cannot lay it out.


## Parses [param markdown_text] into a fully expanded document tree.
static func parse(markdown_text: String) -> MarkdownDocument:
	var document: MarkdownDocument = MarkdownBlockParser.parse(markdown_text)
	MarkdownInlineParser.parse_document(document)
	return document


## Converts [param markdown_text] to BBCode. Passing no [param style] uses the
## defaults of a fresh [MarkdownStyle].
static func to_bbcode(markdown_text: String, style: MarkdownStyle = null) -> String:
	return MarkdownBbcodeEmitter.emit(parse(markdown_text), style)
