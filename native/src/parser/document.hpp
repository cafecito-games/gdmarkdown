// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gdmarkdown {

// Pure C++ (Godot-free) Markdown AST produced by parse_markdown() and consumed
// by the layout engine. Keeping this representation independent of godot-cpp
// allows the parser and table sizing logic to be unit-tested headlessly.

// Bitmask of nested inline text formatting active on an InlineNode.
enum InlineStyleFlag : uint8_t {
	INLINE_STYLE_NONE = 0,
	INLINE_STYLE_BOLD = 1 << 0,
	INLINE_STYLE_ITALIC = 1 << 1,
	INLINE_STYLE_STRIKETHROUGH = 1 << 2,
	INLINE_STYLE_CODE = 1 << 3,
};

// Column or cell text alignment parsed from GFM table delimiter rows.
enum class TableAlignment : uint8_t {
	DEFAULT = 0,
	LEFT = 1,
	CENTER = 2,
	RIGHT = 3,
};

// Discriminator for inline elements inside a paragraph, heading, or table cell.
enum class InlineKind : uint8_t {
	TEXT = 0,
	IMAGE = 1,
	SOFT_BREAK = 2,
	HARD_BREAK = 3,
};

// A single inline run (styled text, inline image, or line break). Adjacent text
// runs sharing identical style_flags and link_url are coalesced during parsing.
struct InlineNode {
	InlineKind kind = InlineKind::TEXT;
	std::string text;
	uint8_t style_flags = INLINE_STYLE_NONE;
	// Decoded hyperlink destination if this text run or inline image is wrapped in a link.
	std::string link_url;
	std::string image_source;
	std::string image_alt;
	std::string image_title;
};

// A single header or body cell inside a GFM table row.
struct TableCell {
	TableAlignment alignment = TableAlignment::DEFAULT;
	std::vector<InlineNode> inlines;
};

// A single row of cells in a GFM table header or body section.
struct TableRow {
	std::vector<TableCell> cells;
};

struct Block;

// An item inside an ordered, unordered, or GFM task list.
struct ListItem {
	bool is_task = false;
	bool checked = false;
	// Zero-based index into Document::initial_task_states, or -1 if not a task.
	int task_index = -1;
	std::vector<Block> children;
};

// Discriminator for block-level Markdown AST nodes.
enum class BlockKind : uint8_t {
	PARAGRAPH = 0,
	HEADING = 1,
	BLOCK_QUOTE = 2,
	CALLOUT = 3,
	LIST = 4,
	CODE_BLOCK = 5,
	TABLE = 6,
	THEMATIC_BREAK = 7,
	IMAGE_BLOCK = 8,
};

// Flat tagged block node. Only the field group corresponding to `kind` is populated.
struct Block {
	BlockKind kind = BlockKind::PARAGRAPH;

	// Paragraph & Heading (heading_level is 1..6 for HEADING)
	int heading_level = 1;
	std::vector<InlineNode> inlines;

	// BlockQuote & Callout (callout_kind is normalized to uppercase, e.g. "NOTE")
	std::string callout_kind;
	std::string callout_custom_title;
	bool callout_collapsible = false;
	bool callout_initially_collapsed = false;
	std::vector<Block> children;

	// List
	bool list_ordered = false;
	int list_start = 1;
	bool list_tight = true;
	std::vector<ListItem> list_items;

	// CodeBlock (code_language holds the first word of the fence info string)
	std::string code_language;
	std::string code_text;

	// Table
	std::vector<TableAlignment> table_alignments;
	std::vector<TableRow> table_header_rows;
	std::vector<TableRow> table_body_rows;

	// ImageBlock (promoted from paragraphs containing only a single image)
	std::string image_source;
	std::string image_alt;
	std::string image_title;
	std::string image_link_url;
};

// Root AST container returned by parse_markdown().
struct Document {
	std::vector<Block> blocks;
	// Initial checked state for every task item in document pre-order DFS order.
	std::vector<bool> initial_task_states;
};

} // namespace gdmarkdown
