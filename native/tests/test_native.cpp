// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#include "layout/table_sizing.hpp"
#include "parser/markdown_parser.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int tests_run = 0;
int tests_failed = 0;

void record_failure(const char *file, int line, const std::string &message) {
	++tests_failed;
	std::cerr << "FAIL [" << file << ":" << line << "]: " << message << "\n";
}

#define EXPECT_TRUE(condition)                                                        \
	do {                                                                              \
		if (!(condition)) {                                                           \
			record_failure(__FILE__, __LINE__, std::string("Expected true: ") + #condition); \
			return;                                                                   \
		}                                                                             \
	} while (false)

#define EXPECT_EQ(actual, expected)                                                   \
	do {                                                                              \
		const auto actual_value = (actual);                                           \
		const auto expected_value = (expected);                                       \
		if (!(actual_value == expected_value)) {                                      \
			record_failure(__FILE__, __LINE__,                                        \
					std::string("Expected equality: ") + #actual + " == " + #expected); \
			return;                                                                   \
		}                                                                             \
	} while (false)

#define EXPECT_NEAR(actual, expected, tolerance)                                      \
	do {                                                                              \
		const float actual_value = static_cast<float>(actual);                        \
		const float expected_value = static_cast<float>(expected);                    \
		if (std::fabs(actual_value - expected_value) > static_cast<float>(tolerance)) { \
			record_failure(__FILE__, __LINE__,                                        \
					std::string("Expected ") + #actual + " (" + std::to_string(actual_value) + \
					") near " + std::to_string(expected_value));                      \
			return;                                                                   \
		}                                                                             \
	} while (false)

void run_test(const char *test_name, void (*test_function)()) {
	const int failures_before = tests_failed;
	++tests_run;
	test_function();
	if (tests_failed == failures_before) {
		std::cout << "PASS: " << test_name << "\n";
	} else {
		std::cout << "FAILED: " << test_name << "\n";
	}
}

using namespace gdmarkdown;

void test_empty_document() {
	const Document document = parse_markdown("");
	EXPECT_TRUE(document.blocks.empty());
	EXPECT_TRUE(document.initial_task_states.empty());
}

void test_headings_all_levels() {
	const Document document = parse_markdown(
			"# Heading 1\n"
			"## Heading 2\n"
			"### Heading 3\n"
			"#### Heading 4\n"
			"##### Heading 5\n"
			"###### Heading **6**\n");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(6));
	for (int level = 1; level <= 6; ++level) {
		const Block &heading = document.blocks[static_cast<size_t>(level - 1)];
		EXPECT_EQ(heading.kind, BlockKind::HEADING);
		EXPECT_EQ(heading.heading_level, level);
	}

	EXPECT_EQ(document.blocks[0].inlines.size(), static_cast<size_t>(1));
	EXPECT_EQ(document.blocks[0].inlines[0].text, std::string("Heading 1"));

	const Block &heading_six = document.blocks[5];
	EXPECT_EQ(heading_six.inlines.size(), static_cast<size_t>(2));
	EXPECT_EQ(heading_six.inlines[0].text, std::string("Heading "));
	EXPECT_EQ(heading_six.inlines[0].style_flags, static_cast<uint8_t>(INLINE_STYLE_NONE));
	EXPECT_EQ(heading_six.inlines[1].text, std::string("6"));
	EXPECT_EQ(heading_six.inlines[1].style_flags, static_cast<uint8_t>(INLINE_STYLE_BOLD));
}

void test_inline_formatting_and_text_merging() {
	const Document document = parse_markdown(
			"Plain *italic* **bold** ***both*** ~~struck~~ `code` A&amp;B");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(1));
	const Block &paragraph = document.blocks[0];
	EXPECT_EQ(paragraph.kind, BlockKind::PARAGRAPH);
	EXPECT_EQ(paragraph.inlines.size(), static_cast<size_t>(11));

	EXPECT_EQ(paragraph.inlines[0].text, std::string("Plain "));
	EXPECT_EQ(paragraph.inlines[0].style_flags, static_cast<uint8_t>(INLINE_STYLE_NONE));

	EXPECT_EQ(paragraph.inlines[1].text, std::string("italic"));
	EXPECT_EQ(paragraph.inlines[1].style_flags, static_cast<uint8_t>(INLINE_STYLE_ITALIC));

	EXPECT_EQ(paragraph.inlines[3].text, std::string("bold"));
	EXPECT_EQ(paragraph.inlines[3].style_flags, static_cast<uint8_t>(INLINE_STYLE_BOLD));

	EXPECT_EQ(paragraph.inlines[5].text, std::string("both"));
	EXPECT_EQ(paragraph.inlines[5].style_flags,
			static_cast<uint8_t>(INLINE_STYLE_BOLD | INLINE_STYLE_ITALIC));

	EXPECT_EQ(paragraph.inlines[7].text, std::string("struck"));
	EXPECT_EQ(paragraph.inlines[7].style_flags, static_cast<uint8_t>(INLINE_STYLE_STRIKETHROUGH));

	EXPECT_EQ(paragraph.inlines[9].text, std::string("code"));
	EXPECT_EQ(paragraph.inlines[9].style_flags, static_cast<uint8_t>(INLINE_STYLE_CODE));

	// Adjacent plain text around a decoded entity merges into a single TEXT node.
	EXPECT_EQ(paragraph.inlines[10].text, std::string(" A&B"));
	EXPECT_EQ(paragraph.inlines[10].style_flags, static_cast<uint8_t>(INLINE_STYLE_NONE));
}

void test_links_and_autolinks() {
	const Document document = parse_markdown(
			"Visit [Cafecito **Games**](https://cafecito.games) or https://example.org/docs "
			"or www.example.com or hello@example.com.");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(1));
	const auto &inlines = document.blocks[0].inlines;

	bool found_plain_link_part = false;
	bool found_bold_link_part = false;
	bool found_url_autolink = false;
	bool found_www_autolink = false;
	bool found_email_autolink = false;

	for (const InlineNode &node : inlines) {
		if (node.kind != InlineKind::TEXT) {
			continue;
		}
		if (node.text == "Cafecito " && node.link_url == "https://cafecito.games" &&
				node.style_flags == INLINE_STYLE_NONE) {
			found_plain_link_part = true;
		}
		if (node.text == "Games" && node.link_url == "https://cafecito.games" &&
				node.style_flags == INLINE_STYLE_BOLD) {
			found_bold_link_part = true;
		}
		if (node.text == "https://example.org/docs" && node.link_url == "https://example.org/docs") {
			found_url_autolink = true;
		}
		if (node.text == "www.example.com" && !node.link_url.empty()) {
			found_www_autolink = true;
		}
		if (node.text == "hello@example.com" && !node.link_url.empty()) {
			found_email_autolink = true;
		}
	}

	EXPECT_TRUE(found_plain_link_part);
	EXPECT_TRUE(found_bold_link_part);
	EXPECT_TRUE(found_url_autolink);
	EXPECT_TRUE(found_www_autolink);
	EXPECT_TRUE(found_email_autolink);
}

void test_images_inline_block_linked_and_alt_capture() {
	const Document document = parse_markdown(
			"See ![Gold *Coin* `50`](res://coin.png \"Shiny\") here.\n\n"
			"![Hero Banner](res://banner.png \"Banner Title\")\n\n"
			"[![Clickable Logo](res://logo.png \"Logo\")](https://example.com/home)\n\n"
			"![First](a.png) ![Second](b.png)\n");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(4));

	// Inline image inside paragraph; nested alt spans must not leak as sibling text nodes.
	const Block &first_paragraph = document.blocks[0];
	EXPECT_EQ(first_paragraph.kind, BlockKind::PARAGRAPH);
	EXPECT_EQ(first_paragraph.inlines.size(), static_cast<size_t>(3));
	EXPECT_EQ(first_paragraph.inlines[0].kind, InlineKind::TEXT);
	EXPECT_EQ(first_paragraph.inlines[0].text, std::string("See "));
	EXPECT_EQ(first_paragraph.inlines[1].kind, InlineKind::IMAGE);
	EXPECT_EQ(first_paragraph.inlines[1].image_source, std::string("res://coin.png"));
	EXPECT_EQ(first_paragraph.inlines[1].image_alt, std::string("Gold Coin 50"));
	EXPECT_EQ(first_paragraph.inlines[1].image_title, std::string("Shiny"));
	EXPECT_EQ(first_paragraph.inlines[1].link_url, std::string(""));
	EXPECT_EQ(first_paragraph.inlines[2].kind, InlineKind::TEXT);
	EXPECT_EQ(first_paragraph.inlines[2].text, std::string(" here."));

	// Standalone image promoted to IMAGE_BLOCK.
	const Block &second_block = document.blocks[1];
	EXPECT_EQ(second_block.kind, BlockKind::IMAGE_BLOCK);
	EXPECT_EQ(second_block.image_source, std::string("res://banner.png"));
	EXPECT_EQ(second_block.image_alt, std::string("Hero Banner"));
	EXPECT_EQ(second_block.image_title, std::string("Banner Title"));
	EXPECT_EQ(second_block.image_link_url, std::string(""));

	// Linked standalone image promoted to IMAGE_BLOCK preserving image_link_url.
	const Block &third_block = document.blocks[2];
	EXPECT_EQ(third_block.kind, BlockKind::IMAGE_BLOCK);
	EXPECT_EQ(third_block.image_source, std::string("res://logo.png"));
	EXPECT_EQ(third_block.image_alt, std::string("Clickable Logo"));
	EXPECT_EQ(third_block.image_title, std::string("Logo"));
	EXPECT_EQ(third_block.image_link_url, std::string("https://example.com/home"));

	// Two images in one paragraph must stay PARAGRAPH.
	const Block &fourth_block = document.blocks[3];
	EXPECT_EQ(fourth_block.kind, BlockKind::PARAGRAPH);
	EXPECT_EQ(fourth_block.inlines.size(), static_cast<size_t>(3));
	EXPECT_EQ(fourth_block.inlines[0].kind, InlineKind::IMAGE);
	EXPECT_EQ(fourth_block.inlines[2].kind, InlineKind::IMAGE);
}

void test_html_entities_and_null_character() {
	const Document named = parse_markdown(
			"&amp;&lt;&gt;&quot;&apos;&nbsp;&mdash;&ndash;&hellip;&copy;&reg;&trade;&bull;"
			"&times;&divide;&plusmn;&laquo;&raquo;&lsquo;&rsquo;&ldquo;&rdquo;"
			"&rarr;&larr;&uarr;&darr;&unknownentity;");

	EXPECT_EQ(named.blocks.size(), static_cast<size_t>(1));
	EXPECT_EQ(named.blocks[0].inlines.size(), static_cast<size_t>(1));

	const std::string expected_named =
			std::string("&<>\"'") +
			"\xC2\xA0\xE2\x80\x94\xE2\x80\x93\xE2\x80\xA6\xC2\xA9\xC2\xAE\xE2\x84\xA2\xE2\x80\xA2" +
			"\xC3\x97\xC3\xB7\xC2\xB1\xC2\xAB\xC2\xBB\xE2\x80\x98\xE2\x80\x99\xE2\x80\x9C\xE2\x80\x9D" +
			"\xE2\x86\x92\xE2\x86\x90\xE2\x86\x91\xE2\x86\x93&unknownentity;";
	EXPECT_EQ(named.blocks[0].inlines[0].text, expected_named);

	const Document numeric = parse_markdown("&#65; &#123; &#x1F600; &#X42; &#0; &#xD800;");
	EXPECT_EQ(numeric.blocks.size(), static_cast<size_t>(1));
	EXPECT_EQ(numeric.blocks[0].inlines.size(), static_cast<size_t>(1));
	const std::string expected_numeric =
			std::string("A { \xF0\x9F\x98\x80 B \xEF\xBF\xBD \xEF\xBF\xBD");
	EXPECT_EQ(numeric.blocks[0].inlines[0].text, expected_numeric);

	const std::string with_null("ab\0cd", 5);
	const Document null_doc = parse_markdown(with_null);
	EXPECT_EQ(null_doc.blocks.size(), static_cast<size_t>(1));
	EXPECT_EQ(null_doc.blocks[0].inlines[0].text, std::string("ab\xEF\xBF\xBD" "cd"));
}

void test_br_tags_and_raw_html_stripping() {
	const Document document = parse_markdown(
			"Line 1<br>Line 2<BR/>Line 3<br />Line 4<Br   />Line 5\n\n"
			"<div class=\"danger\">\nraw html block\n</div>\n\n"
			"Before <span style=\"color:red\">inside</span> <script>x</script> after");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(2));

	const Block &first_paragraph = document.blocks[0];
	EXPECT_EQ(first_paragraph.kind, BlockKind::PARAGRAPH);
	EXPECT_EQ(first_paragraph.inlines.size(), static_cast<size_t>(9));
	EXPECT_EQ(first_paragraph.inlines[0].text, std::string("Line 1"));
	EXPECT_EQ(first_paragraph.inlines[1].kind, InlineKind::HARD_BREAK);
	EXPECT_EQ(first_paragraph.inlines[2].text, std::string("Line 2"));
	EXPECT_EQ(first_paragraph.inlines[3].kind, InlineKind::HARD_BREAK);
	EXPECT_EQ(first_paragraph.inlines[4].text, std::string("Line 3"));
	EXPECT_EQ(first_paragraph.inlines[5].kind, InlineKind::HARD_BREAK);
	EXPECT_EQ(first_paragraph.inlines[6].text, std::string("Line 4"));
	EXPECT_EQ(first_paragraph.inlines[7].kind, InlineKind::HARD_BREAK);
	EXPECT_EQ(first_paragraph.inlines[8].text, std::string("Line 5"));

	// Raw HTML block was discarded; raw inline HTML tags were stripped cleanly.
	const Block &second_paragraph = document.blocks[1];
	EXPECT_EQ(second_paragraph.kind, BlockKind::PARAGRAPH);
	EXPECT_EQ(second_paragraph.inlines.size(), static_cast<size_t>(1));
	EXPECT_EQ(second_paragraph.inlines[0].text, std::string("Before inside x after"));
}

void test_lists_ordered_unordered_and_nested() {
	const Document document = parse_markdown(
			"- Alpha\n"
			"- Beta\n"
			"  1. Gamma\n"
			"  2. Delta\n\n"
			"3. Loose three\n\n"
			"4. Loose four\n");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(2));

	const Block &unordered_list = document.blocks[0];
	EXPECT_EQ(unordered_list.kind, BlockKind::LIST);
	EXPECT_TRUE(!unordered_list.list_ordered);
	EXPECT_TRUE(unordered_list.list_tight);
	EXPECT_EQ(unordered_list.list_items.size(), static_cast<size_t>(2));

	const ListItem &first_item = unordered_list.list_items[0];
	EXPECT_EQ(first_item.children.size(), static_cast<size_t>(1));
	EXPECT_EQ(first_item.children[0].kind, BlockKind::PARAGRAPH);
	EXPECT_EQ(first_item.children[0].inlines[0].text, std::string("Alpha"));

	const ListItem &second_item = unordered_list.list_items[1];
	EXPECT_EQ(second_item.children.size(), static_cast<size_t>(2));
	EXPECT_EQ(second_item.children[0].kind, BlockKind::PARAGRAPH);
	EXPECT_EQ(second_item.children[0].inlines[0].text, std::string("Beta"));

	const Block &nested_list = second_item.children[1];
	EXPECT_EQ(nested_list.kind, BlockKind::LIST);
	EXPECT_TRUE(nested_list.list_ordered);
	EXPECT_EQ(nested_list.list_start, 1);
	EXPECT_EQ(nested_list.list_items.size(), static_cast<size_t>(2));
	EXPECT_EQ(nested_list.list_items[0].children[0].inlines[0].text, std::string("Gamma"));
	EXPECT_EQ(nested_list.list_items[1].children[0].inlines[0].text, std::string("Delta"));

	const Block &loose_list = document.blocks[1];
	EXPECT_EQ(loose_list.kind, BlockKind::LIST);
	EXPECT_TRUE(loose_list.list_ordered);
	EXPECT_EQ(loose_list.list_start, 3);
	EXPECT_TRUE(!loose_list.list_tight);
	EXPECT_EQ(loose_list.list_items.size(), static_cast<size_t>(2));
	EXPECT_EQ(loose_list.list_items[0].children[0].inlines[0].text, std::string("Loose three"));
}

void test_task_lists_and_nested_indexing() {
	const Document document = parse_markdown(
			"- [ ] Task zero\n"
			"- Regular bullet\n"
			"- [x] Task one\n"
			"  - [X] Nested task two\n"
			"  - [ ] Nested task three\n"
			"> [!QUEST] Quest\n"
			"> - [x] Callout task four\n");

	EXPECT_EQ(document.initial_task_states.size(), static_cast<size_t>(5));
	EXPECT_EQ(document.initial_task_states[0], false);
	EXPECT_EQ(document.initial_task_states[1], true);
	EXPECT_EQ(document.initial_task_states[2], true);
	EXPECT_EQ(document.initial_task_states[3], false);
	EXPECT_EQ(document.initial_task_states[4], true);

	const Block &top_list = document.blocks[0];
	EXPECT_EQ(top_list.kind, BlockKind::LIST);
	EXPECT_TRUE(top_list.list_items[0].is_task);
	EXPECT_EQ(top_list.list_items[0].checked, false);
	EXPECT_EQ(top_list.list_items[0].task_index, 0);

	EXPECT_TRUE(!top_list.list_items[1].is_task);
	EXPECT_EQ(top_list.list_items[1].task_index, -1);

	EXPECT_TRUE(top_list.list_items[2].is_task);
	EXPECT_EQ(top_list.list_items[2].checked, true);
	EXPECT_EQ(top_list.list_items[2].task_index, 1);

	const Block &nested_list = top_list.list_items[2].children[1];
	EXPECT_EQ(nested_list.list_items[0].task_index, 2);
	EXPECT_EQ(nested_list.list_items[0].checked, true);
	EXPECT_EQ(nested_list.list_items[1].task_index, 3);
	EXPECT_EQ(nested_list.list_items[1].checked, false);

	const Block &callout = document.blocks[1];
	EXPECT_EQ(callout.kind, BlockKind::CALLOUT);
	EXPECT_EQ(callout.children.size(), static_cast<size_t>(1));
	EXPECT_EQ(callout.children[0].kind, BlockKind::LIST);
	EXPECT_EQ(callout.children[0].list_items[0].task_index, 4);
	EXPECT_EQ(callout.children[0].list_items[0].checked, true);
}

void test_callouts_github_custom_collapsible_and_nested() {
	const Document document = parse_markdown(
			"> [!NOTE]\n"
			"> Note body.\n\n"
			"> [!tip]\n"
			"> Tip body.\n\n"
			"> [!IMPORTANT]\n"
			">\n"
			"> Important body after blank line.\n\n"
			"> [!WARNING]+ Expandable Warning\n"
			"> Warning body.\n\n"
			"> [!CAUTION]- Collapsed Caution\n"
			"> Caution body.\n\n"
			"> [!QUEST] Custom Quest Title  \n"
			"> Find the relic.\n"
			">\n"
			"> > [!LORE_2-B] Nested Lore\n"
			"> > Deeply nested text.\n\n"
			"- List parent\n"
			"  > [!NOTE]-\n"
			"  > Inside list item.\n");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(7));

	// NOTE
	EXPECT_EQ(document.blocks[0].kind, BlockKind::CALLOUT);
	EXPECT_EQ(document.blocks[0].callout_kind, std::string("NOTE"));
	EXPECT_EQ(document.blocks[0].callout_custom_title, std::string(""));
	EXPECT_EQ(document.blocks[0].callout_collapsible, false);
	EXPECT_EQ(document.blocks[0].callout_initially_collapsed, false);
	EXPECT_EQ(document.blocks[0].children.size(), static_cast<size_t>(1));
	EXPECT_EQ(document.blocks[0].children[0].inlines[0].text, std::string("Note body."));

	// tip -> TIP
	EXPECT_EQ(document.blocks[1].kind, BlockKind::CALLOUT);
	EXPECT_EQ(document.blocks[1].callout_kind, std::string("TIP"));
	EXPECT_EQ(document.blocks[1].children[0].inlines[0].text, std::string("Tip body."));

	// IMPORTANT with blank line (header paragraph erased)
	EXPECT_EQ(document.blocks[2].kind, BlockKind::CALLOUT);
	EXPECT_EQ(document.blocks[2].callout_kind, std::string("IMPORTANT"));
	EXPECT_EQ(document.blocks[2].children.size(), static_cast<size_t>(1));
	EXPECT_EQ(document.blocks[2].children[0].inlines[0].text, std::string("Important body after blank line."));

	// WARNING+
	EXPECT_EQ(document.blocks[3].kind, BlockKind::CALLOUT);
	EXPECT_EQ(document.blocks[3].callout_kind, std::string("WARNING"));
	EXPECT_EQ(document.blocks[3].callout_custom_title, std::string("Expandable Warning"));
	EXPECT_EQ(document.blocks[3].callout_collapsible, true);
	EXPECT_EQ(document.blocks[3].callout_initially_collapsed, false);

	// CAUTION-
	EXPECT_EQ(document.blocks[4].kind, BlockKind::CALLOUT);
	EXPECT_EQ(document.blocks[4].callout_kind, std::string("CAUTION"));
	EXPECT_EQ(document.blocks[4].callout_custom_title, std::string("Collapsed Caution"));
	EXPECT_EQ(document.blocks[4].callout_collapsible, true);
	EXPECT_EQ(document.blocks[4].callout_initially_collapsed, true);

	// QUEST with nested LORE_2-B
	const Block &quest = document.blocks[5];
	EXPECT_EQ(quest.kind, BlockKind::CALLOUT);
	EXPECT_EQ(quest.callout_kind, std::string("QUEST"));
	EXPECT_EQ(quest.callout_custom_title, std::string("Custom Quest Title"));
	EXPECT_EQ(quest.children.size(), static_cast<size_t>(2));
	EXPECT_EQ(quest.children[0].kind, BlockKind::PARAGRAPH);
	EXPECT_EQ(quest.children[0].inlines[0].text, std::string("Find the relic."));

	const Block &nested_lore = quest.children[1];
	EXPECT_EQ(nested_lore.kind, BlockKind::CALLOUT);
	EXPECT_EQ(nested_lore.callout_kind, std::string("LORE_2-B"));
	EXPECT_EQ(nested_lore.callout_custom_title, std::string("Nested Lore"));
	EXPECT_EQ(nested_lore.children.size(), static_cast<size_t>(1));
	EXPECT_EQ(nested_lore.children[0].inlines[0].text, std::string("Deeply nested text."));

	// Callout inside ListItem
	const Block &list_block = document.blocks[6];
	EXPECT_EQ(list_block.kind, BlockKind::LIST);
	EXPECT_EQ(list_block.list_items[0].children.size(), static_cast<size_t>(2));
	const Block &list_callout = list_block.list_items[0].children[1];
	EXPECT_EQ(list_callout.kind, BlockKind::CALLOUT);
	EXPECT_EQ(list_callout.callout_kind, std::string("NOTE"));
	EXPECT_EQ(list_callout.callout_collapsible, true);
	EXPECT_EQ(list_callout.callout_initially_collapsed, true);
	EXPECT_EQ(list_callout.callout_custom_title, std::string(""));
	EXPECT_EQ(list_callout.children[0].inlines[0].text, std::string("Inside list item."));
}

void test_non_callout_blockquotes_do_not_promote() {
	const Document document = parse_markdown(
			"> Regular blockquote.\n\n"
			"> `[!NOTE]`\n"
			"> Backtick directive.\n\n"
			"> *[!NOTE]*\n"
			"> Italic directive.\n\n"
			"> **[!WARNING]**\n"
			"> Bold directive.\n\n"
			"> [[!TIP]](https://example.com)\n"
			"> Linked directive.\n\n"
			"> [!123INVALID]\n"
			"> Leading digit.\n\n"
			"> [!NOTE]nospace\n"
			"> Missing separator.\n\n"
			"> [!NOTE]**bold**\n"
			"> Missing separator before styled span.\n");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(8));
	for (size_t index = 0; index < document.blocks.size(); ++index) {
		EXPECT_EQ(document.blocks[index].kind, BlockKind::BLOCK_QUOTE);
	}
}

void test_code_blocks_fenced_and_indented() {
	const Document document = parse_markdown(
			"```gdscript title=\"demo.gd\"\n"
			"func ready():\n"
			"    var value = 1\n"
			"\n"
			"    return value\n"
			"```\n\n"
			"```\n"
			"trailing blank line follows\n"
			"\n"
			"```\n\n"
			"    indented line 1\n"
			"        indented line 2\n");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(3));

	const Block &fenced = document.blocks[0];
	EXPECT_EQ(fenced.kind, BlockKind::CODE_BLOCK);
	EXPECT_EQ(fenced.code_language, std::string("gdscript"));
	EXPECT_EQ(fenced.code_text, std::string("func ready():\n    var value = 1\n\n    return value"));

	const Block &trailing_blank = document.blocks[1];
	EXPECT_EQ(trailing_blank.kind, BlockKind::CODE_BLOCK);
	EXPECT_EQ(trailing_blank.code_language, std::string(""));
	EXPECT_EQ(trailing_blank.code_text, std::string("trailing blank line follows\n"));

	const Block &indented = document.blocks[2];
	EXPECT_EQ(indented.kind, BlockKind::CODE_BLOCK);
	EXPECT_EQ(indented.code_language, std::string(""));
	EXPECT_EQ(indented.code_text, std::string("indented line 1\n    indented line 2"));
}

void test_tables_and_thematic_breaks() {
	const Document document = parse_markdown(
			"| Left | Center | Right | Default |\n"
			"| :--- | :----: | ----: | ------- |\n"
			"| **A** | `B` | C | D |\n"
			"| 1 | 2 | 3 | 4 |\n\n"
			"---\n");

	EXPECT_EQ(document.blocks.size(), static_cast<size_t>(2));

	const Block &table = document.blocks[0];
	EXPECT_EQ(table.kind, BlockKind::TABLE);
	EXPECT_EQ(table.table_alignments.size(), static_cast<size_t>(4));
	EXPECT_EQ(table.table_alignments[0], TableAlignment::LEFT);
	EXPECT_EQ(table.table_alignments[1], TableAlignment::CENTER);
	EXPECT_EQ(table.table_alignments[2], TableAlignment::RIGHT);
	EXPECT_EQ(table.table_alignments[3], TableAlignment::DEFAULT);

	EXPECT_EQ(table.table_header_rows.size(), static_cast<size_t>(1));
	EXPECT_EQ(table.table_header_rows[0].cells.size(), static_cast<size_t>(4));
	EXPECT_EQ(table.table_header_rows[0].cells[0].inlines[0].text, std::string("Left"));
	EXPECT_EQ(table.table_header_rows[0].cells[1].alignment, TableAlignment::CENTER);

	EXPECT_EQ(table.table_body_rows.size(), static_cast<size_t>(2));
	EXPECT_EQ(table.table_body_rows[0].cells[0].inlines[0].text, std::string("A"));
	EXPECT_EQ(table.table_body_rows[0].cells[0].inlines[0].style_flags,
			static_cast<uint8_t>(INLINE_STYLE_BOLD));
	EXPECT_EQ(table.table_body_rows[0].cells[1].inlines[0].text, std::string("B"));
	EXPECT_EQ(table.table_body_rows[0].cells[1].inlines[0].style_flags,
			static_cast<uint8_t>(INLINE_STYLE_CODE));
	EXPECT_EQ(table.table_body_rows[1].cells[3].inlines[0].text, std::string("4"));

	EXPECT_EQ(document.blocks[1].kind, BlockKind::THEMATIC_BREAK);
}

void test_table_sizing_empty_and_non_positive_width() {
	const TableSizingResult empty_result = compute_table_column_widths({}, 400.0f, 10.0f);
	EXPECT_TRUE(empty_result.columns.empty());
	EXPECT_NEAR(empty_result.total_width, 0.0f, 0.001f);

	const std::vector<TableColumnBounds> bounds = { { 60.0f, 120.0f }, { 80.0f, 200.0f } };

	for (float invalid_width : { 0.0f, -50.0f }) {
		const TableSizingResult result = compute_table_column_widths(bounds, invalid_width, 10.0f);
		EXPECT_EQ(result.columns.size(), static_cast<size_t>(2));
		EXPECT_NEAR(result.total_width, 0.0f, 0.001f);
		for (const TableColumnLayout &column : result.columns) {
			EXPECT_TRUE(std::isfinite(column.column_width));
			EXPECT_TRUE(column.effective_padding_horizontal >= 0.0f);
			EXPECT_TRUE(column.text_width >= 1.0f);
		}
	}
}

void test_table_sizing_fits_and_fills() {
	const std::vector<TableColumnBounds> bounds = { { 40.0f, 100.0f }, { 60.0f, 200.0f } };

	const TableSizingResult natural = compute_table_column_widths(bounds, 600.0f, 10.0f, false);
	EXPECT_EQ(natural.columns.size(), static_cast<size_t>(2));
	EXPECT_NEAR(natural.columns[0].column_width, 100.0f, 0.01f);
	EXPECT_NEAR(natural.columns[0].effective_padding_horizontal, 10.0f, 0.01f);
	EXPECT_NEAR(natural.columns[0].text_width, 80.0f, 0.01f);
	EXPECT_NEAR(natural.columns[1].column_width, 200.0f, 0.01f);
	EXPECT_NEAR(natural.columns[1].effective_padding_horizontal, 10.0f, 0.01f);
	EXPECT_NEAR(natural.columns[1].text_width, 180.0f, 0.01f);
	EXPECT_NEAR(natural.total_width, 300.0f, 0.01f);

	const TableSizingResult filled = compute_table_column_widths(bounds, 600.0f, 10.0f, true);
	EXPECT_NEAR(filled.columns[0].column_width, 200.0f, 0.01f);
	EXPECT_NEAR(filled.columns[1].column_width, 400.0f, 0.01f);
	EXPECT_NEAR(filled.total_width, 600.0f, 0.01f);
}

void test_table_sizing_proportional_shrink() {
	const std::vector<TableColumnBounds> bounds = { { 50.0f, 100.0f }, { 50.0f, 200.0f } };
	const TableSizingResult result = compute_table_column_widths(bounds, 200.0f, 10.0f, false);

	EXPECT_EQ(result.columns.size(), static_cast<size_t>(2));
	EXPECT_NEAR(result.columns[0].column_width, 75.0f, 0.01f);
	EXPECT_NEAR(result.columns[0].effective_padding_horizontal, 10.0f, 0.01f);
	EXPECT_NEAR(result.columns[0].text_width, 55.0f, 0.01f);
	EXPECT_NEAR(result.columns[1].column_width, 125.0f, 0.01f);
	EXPECT_NEAR(result.columns[1].effective_padding_horizontal, 10.0f, 0.01f);
	EXPECT_NEAR(result.columns[1].text_width, 105.0f, 0.01f);
	EXPECT_NEAR(result.total_width, 200.0f, 0.01f);
}

void test_table_sizing_extreme_narrow_containers() {
	// Two columns in a 30px container with 10px horizontal cell padding.
	const std::vector<TableColumnBounds> two_columns = { { 68.0f, 150.0f }, { 68.0f, 200.0f } };
	const TableSizingResult narrow = compute_table_column_widths(two_columns, 30.0f, 10.0f);

	EXPECT_EQ(narrow.columns.size(), static_cast<size_t>(2));
	EXPECT_NEAR(narrow.total_width, 30.0f, 0.01f);
	for (const TableColumnLayout &column : narrow.columns) {
		EXPECT_NEAR(column.column_width, 15.0f, 0.01f);
		EXPECT_NEAR(column.effective_padding_horizontal, 7.0f, 0.01f);
		EXPECT_NEAR(column.text_width, 1.0f, 0.01f);
		EXPECT_TRUE(column.text_width >= 1.0f);
	}

	// Five columns in a 2px container with 10px padding: text_width must still stay >= 1.0f.
	const std::vector<TableColumnBounds> five_columns(5, { 50.0f, 100.0f });
	const TableSizingResult ultra_narrow = compute_table_column_widths(five_columns, 2.0f, 10.0f);
	EXPECT_NEAR(ultra_narrow.total_width, 2.0f, 0.01f);
	for (const TableColumnLayout &column : ultra_narrow.columns) {
		EXPECT_NEAR(column.column_width, 0.4f, 0.01f);
		EXPECT_NEAR(column.effective_padding_horizontal, 0.0f, 0.01f);
		EXPECT_TRUE(column.text_width >= 1.0f);
	}
}

} // namespace

int main() {
	run_test("empty_document", test_empty_document);
	run_test("headings_all_levels", test_headings_all_levels);
	run_test("inline_formatting_and_text_merging", test_inline_formatting_and_text_merging);
	run_test("links_and_autolinks", test_links_and_autolinks);
	run_test("images_inline_block_linked_and_alt_capture", test_images_inline_block_linked_and_alt_capture);
	run_test("html_entities_and_null_character", test_html_entities_and_null_character);
	run_test("br_tags_and_raw_html_stripping", test_br_tags_and_raw_html_stripping);
	run_test("lists_ordered_unordered_and_nested", test_lists_ordered_unordered_and_nested);
	run_test("task_lists_and_nested_indexing", test_task_lists_and_nested_indexing);
	run_test("callouts_github_custom_collapsible_and_nested", test_callouts_github_custom_collapsible_and_nested);
	run_test("non_callout_blockquotes_do_not_promote", test_non_callout_blockquotes_do_not_promote);
	run_test("code_blocks_fenced_and_indented", test_code_blocks_fenced_and_indented);
	run_test("tables_and_thematic_breaks", test_tables_and_thematic_breaks);
	run_test("table_sizing_empty_and_non_positive_width", test_table_sizing_empty_and_non_positive_width);
	run_test("table_sizing_fits_and_fills", test_table_sizing_fits_and_fills);
	run_test("table_sizing_proportional_shrink", test_table_sizing_proportional_shrink);
	run_test("table_sizing_extreme_narrow_containers", test_table_sizing_extreme_narrow_containers);

	std::cout << "\nRan " << tests_run << " tests, " << tests_failed << " failures.\n";
	return tests_failed == 0 ? 0 : 1;
}
