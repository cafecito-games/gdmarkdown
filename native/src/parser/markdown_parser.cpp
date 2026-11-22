// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#include "parser/markdown_parser.hpp"

#include "md4c.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdmarkdown {
namespace {

constexpr const char *UTF8_REPLACEMENT_CHARACTER = "\xEF\xBF\xBD";

char ascii_to_lower(char character) {
	if (character >= 'A' && character <= 'Z') {
		return static_cast<char>(character - 'A' + 'a');
	}
	return character;
}

char ascii_to_upper(char character) {
	if (character >= 'a' && character <= 'z') {
		return static_cast<char>(character - 'a' + 'A');
	}
	return character;
}

bool is_ascii_alpha(char character) {
	return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}

bool is_ascii_alphanumeric(char character) {
	return is_ascii_alpha(character) || (character >= '0' && character <= '9');
}

bool is_ascii_whitespace(char character) {
	return character == ' ' || character == '\t' || character == '\r' || character == '\n';
}

bool is_ascii_whitespace_only(std::string_view text) {
	for (char character : text) {
		if (!is_ascii_whitespace(character)) {
			return false;
		}
	}
	return true;
}

std::string trim_horizontal_whitespace(std::string_view text) {
	size_t start = 0;
	while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) {
		++start;
	}
	size_t end = text.size();
	while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
		--end;
	}
	return std::string(text.substr(start, end - start));
}

// Encodes a Unicode scalar value to UTF-8, substituting U+FFFD for NUL, UTF-16
// surrogates, and out-of-range code points per CommonMark §2.3 / §6.2.
void append_utf8_code_point(std::string &output, uint32_t code_point) {
	if (code_point == 0 || (code_point >= 0xD800 && code_point <= 0xDFFF) || code_point > 0x10FFFF) {
		output.append(UTF8_REPLACEMENT_CHARACTER);
		return;
	}

	if (code_point <= 0x7F) {
		output.push_back(static_cast<char>(code_point));
	} else if (code_point <= 0x7FF) {
		output.push_back(static_cast<char>(0xC0 | ((code_point >> 6) & 0x1F)));
		output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
	} else if (code_point <= 0xFFFF) {
		output.push_back(static_cast<char>(0xE0 | ((code_point >> 12) & 0x0F)));
		output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
		output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
	} else {
		output.push_back(static_cast<char>(0xF0 | ((code_point >> 18) & 0x07)));
		output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
		output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
		output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
	}
}

struct NamedEntityEntry {
	std::string_view name;
	std::string_view replacement;
};

constexpr NamedEntityEntry NAMED_ENTITIES[] = {
	{ "&amp;", "&" },
	{ "&lt;", "<" },
	{ "&gt;", ">" },
	{ "&quot;", "\"" },
	{ "&apos;", "'" },
	{ "&nbsp;", "\xC2\xA0" },
	{ "&mdash;", "\xE2\x80\x94" },
	{ "&ndash;", "\xE2\x80\x93" },
	{ "&hellip;", "\xE2\x80\xA6" },
	{ "&copy;", "\xC2\xA9" },
	{ "&reg;", "\xC2\xAE" },
	{ "&trade;", "\xE2\x84\xA2" },
	{ "&bull;", "\xE2\x80\xA2" },
	{ "&times;", "\xC3\x97" },
	{ "&divide;", "\xC3\xB7" },
	{ "&plusmn;", "\xC2\xB1" },
	{ "&laquo;", "\xC2\xAB" },
	{ "&raquo;", "\xC2\xBB" },
	{ "&lsquo;", "\xE2\x80\x98" },
	{ "&rsquo;", "\xE2\x80\x99" },
	{ "&ldquo;", "\xE2\x80\x9C" },
	{ "&rdquo;", "\xE2\x80\x9D" },
	{ "&rarr;", "\xE2\x86\x92" },
	{ "&larr;", "\xE2\x86\x90" },
	{ "&uarr;", "\xE2\x86\x91" },
	{ "&darr;", "\xE2\x86\x93" },
};

std::string decode_entity(std::string_view entity) {
	if (entity.size() < 3 || entity.front() != '&' || entity.back() != ';') {
		return std::string(entity);
	}

	if (entity[1] == '#') {
		const size_t body_end = entity.size() - 1;
		if (body_end <= 2) {
			return std::string(entity);
		}

		uint32_t code_point = 0;
		bool overflowed = false;

		if (entity[2] == 'x' || entity[2] == 'X') {
			if (body_end <= 3) {
				return std::string(entity);
			}
			for (size_t position = 3; position < body_end; ++position) {
				const char character = entity[position];
				uint32_t digit = 0;
				if (character >= '0' && character <= '9') {
					digit = static_cast<uint32_t>(character - '0');
				} else if (character >= 'a' && character <= 'f') {
					digit = static_cast<uint32_t>(10 + (character - 'a'));
				} else if (character >= 'A' && character <= 'F') {
					digit = static_cast<uint32_t>(10 + (character - 'A'));
				} else {
					return std::string(entity);
				}

				if (code_point > 0x10FFFF) {
					overflowed = true;
				} else {
					code_point = (code_point << 4) | digit;
				}
			}
		} else {
			for (size_t position = 2; position < body_end; ++position) {
				const char character = entity[position];
				if (character < '0' || character > '9') {
					return std::string(entity);
				}
				const uint32_t digit = static_cast<uint32_t>(character - '0');
				if (code_point > 0x10FFFF) {
					overflowed = true;
				} else {
					code_point = code_point * 10 + digit;
				}
			}
		}

		std::string decoded;
		append_utf8_code_point(decoded, overflowed ? 0xFFFDu : code_point);
		return decoded;
	}

	for (const NamedEntityEntry &entry : NAMED_ENTITIES) {
		if (entry.name == entity) {
			return std::string(entry.replacement);
		}
	}

	return std::string(entity);
}

std::string decode_attribute(const MD_ATTRIBUTE &attribute) {
	if (attribute.text == nullptr || attribute.size == 0) {
		return {};
	}
	if (attribute.substr_types == nullptr || attribute.substr_offsets == nullptr) {
		return std::string(attribute.text, attribute.size);
	}

	std::string decoded;
	decoded.reserve(attribute.size);
	for (size_t segment_index = 0; attribute.substr_offsets[segment_index] < attribute.size; ++segment_index) {
		const MD_OFFSET start = attribute.substr_offsets[segment_index];
		const MD_OFFSET end = attribute.substr_offsets[segment_index + 1];
		const std::string_view slice(attribute.text + start, end - start);

		switch (attribute.substr_types[segment_index]) {
			case MD_TEXT_ENTITY:
				decoded.append(decode_entity(slice));
				break;
			case MD_TEXT_NULLCHAR:
				decoded.append(UTF8_REPLACEMENT_CHARACTER);
				break;
			default:
				decoded.append(slice);
				break;
		}
	}
	return decoded;
}

std::string extract_first_word(std::string_view text) {
	size_t start = 0;
	while (start < text.size() && is_ascii_whitespace(text[start])) {
		++start;
	}
	size_t end = start;
	while (end < text.size() && !is_ascii_whitespace(text[end])) {
		++end;
	}
	return std::string(text.substr(start, end - start));
}

bool is_html_line_break_tag(std::string_view html) {
	if (html.size() < 4 || html.front() != '<' || html.back() != '>') {
		return false;
	}
	if (ascii_to_lower(html[1]) != 'b' || ascii_to_lower(html[2]) != 'r') {
		return false;
	}

	size_t position = 3;
	const size_t closing_bracket = html.size() - 1;
	while (position < closing_bracket && is_ascii_whitespace(html[position])) {
		++position;
	}
	if (position == closing_bracket) {
		return true;
	}
	if (html[position] == '/') {
		++position;
		while (position < closing_bracket && is_ascii_whitespace(html[position])) {
			++position;
		}
		return position == closing_bracket;
	}
	return false;
}

TableAlignment convert_table_alignment(MD_ALIGN alignment) {
	switch (alignment) {
		case MD_ALIGN_LEFT:
			return TableAlignment::LEFT;
		case MD_ALIGN_CENTER:
			return TableAlignment::CENTER;
		case MD_ALIGN_RIGHT:
			return TableAlignment::RIGHT;
		case MD_ALIGN_DEFAULT:
		default:
			return TableAlignment::DEFAULT;
	}
}

// Promotes a paragraph containing only a single image (plus optional plain
// whitespace) into a standalone IMAGE_BLOCK so layout can scale it against the
// full container width rather than constraining it to inline line height.
void promote_single_image_paragraph(Block &block) {
	if (block.kind != BlockKind::PARAGRAPH) {
		return;
	}

	size_t image_count = 0;
	size_t image_index = 0;
	for (size_t index = 0; index < block.inlines.size(); ++index) {
		const InlineNode &node = block.inlines[index];
		if (node.kind == InlineKind::IMAGE) {
			++image_count;
			image_index = index;
			if (image_count > 1) {
				return;
			}
		} else if (node.kind == InlineKind::TEXT) {
			if (node.style_flags != INLINE_STYLE_NONE || !is_ascii_whitespace_only(node.text)) {
				return;
			}
		} else {
			return;
		}
	}

	if (image_count != 1) {
		return;
	}

	InlineNode image_node = std::move(block.inlines[image_index]);
	block.kind = BlockKind::IMAGE_BLOCK;
	block.image_source = std::move(image_node.image_source);
	block.image_alt = std::move(image_node.image_alt);
	block.image_title = std::move(image_node.image_title);
	block.image_link_url = std::move(image_node.link_url);
	block.inlines.clear();
}

enum class FrameKind : uint8_t {
	DOCUMENT = 0,
	BLOCK = 1,
	LIST_ITEM = 2,
	TABLE_ROW = 3,
	TABLE_CELL = 4,
};

struct ParserFrame {
	FrameKind kind = FrameKind::DOCUMENT;
	Block block;
	ListItem list_item;
	TableRow table_row;
	TableCell table_cell;
	bool in_table_header = false;
	bool table_row_is_header = false;
	bool implicit_paragraph_open = false;
};

struct ImageCapture {
	std::string source;
	std::string title;
	std::string alt;
	std::string link_url;
};

struct ParserContext {
	Document document;
	std::vector<ParserFrame> frames;
	std::vector<std::string> link_stack;
	std::vector<ImageCapture> image_stack;
	int bold_depth = 0;
	int italic_depth = 0;
	int strikethrough_depth = 0;
	int code_depth = 0;
	int html_block_depth = 0;

	uint8_t current_style_flags() const {
		uint8_t flags = INLINE_STYLE_NONE;
		if (bold_depth > 0) {
			flags |= INLINE_STYLE_BOLD;
		}
		if (italic_depth > 0) {
			flags |= INLINE_STYLE_ITALIC;
		}
		if (strikethrough_depth > 0) {
			flags |= INLINE_STYLE_STRIKETHROUGH;
		}
		if (code_depth > 0) {
			flags |= INLINE_STYLE_CODE;
		}
		return flags;
	}

	std::string current_link_url() const {
		if (link_stack.empty()) {
			return {};
		}
		return link_stack.back();
	}

	void close_implicit_list_item_paragraph() {
		if (frames.empty()) {
			return;
		}
		ParserFrame &top_frame = frames.back();
		if (top_frame.kind == FrameKind::LIST_ITEM && top_frame.implicit_paragraph_open) {
			top_frame.implicit_paragraph_open = false;
			if (!top_frame.list_item.children.empty()) {
				promote_single_image_paragraph(top_frame.list_item.children.back());
			}
		}
	}

	void append_completed_block(Block &&completed_block) {
		if (frames.empty()) {
			document.blocks.push_back(std::move(completed_block));
			return;
		}

		ParserFrame &parent_frame = frames.back();
		if (parent_frame.kind == FrameKind::DOCUMENT) {
			document.blocks.push_back(std::move(completed_block));
		} else if (parent_frame.kind == FrameKind::BLOCK) {
			parent_frame.block.children.push_back(std::move(completed_block));
		} else if (parent_frame.kind == FrameKind::LIST_ITEM) {
			parent_frame.list_item.children.push_back(std::move(completed_block));
		}
	}

	std::vector<InlineNode> *ensure_current_inlines() {
		if (frames.empty()) {
			return nullptr;
		}

		ParserFrame &top_frame = frames.back();
		if (top_frame.kind == FrameKind::BLOCK &&
				(top_frame.block.kind == BlockKind::PARAGRAPH || top_frame.block.kind == BlockKind::HEADING)) {
			return &top_frame.block.inlines;
		}
		if (top_frame.kind == FrameKind::TABLE_CELL) {
			return &top_frame.table_cell.inlines;
		}
		if (top_frame.kind == FrameKind::LIST_ITEM) {
			// Tight list items in md4c emit inline callbacks directly without an
			// enclosing MD_BLOCK_P; synthesize an implicit paragraph until a nested
			// block opens or the list item closes.
			if (!top_frame.implicit_paragraph_open ||
					top_frame.list_item.children.empty() ||
					top_frame.list_item.children.back().kind != BlockKind::PARAGRAPH) {
				Block paragraph;
				paragraph.kind = BlockKind::PARAGRAPH;
				top_frame.list_item.children.push_back(std::move(paragraph));
				top_frame.implicit_paragraph_open = true;
			}
			return &top_frame.list_item.children.back().inlines;
		}
		return nullptr;
	}

	// Inside an image span, md4c emits nested text/break callbacks as the alt
	// description; capture them onto the active ImageCapture instead of emitting
	// visible text runs.
	void append_text_inline(std::string_view text) {
		if (text.empty()) {
			return;
		}
		if (!image_stack.empty()) {
			image_stack.back().alt.append(text);
			return;
		}

		std::vector<InlineNode> *inlines = ensure_current_inlines();
		if (inlines == nullptr) {
			return;
		}

		const uint8_t style_flags = current_style_flags();
		const std::string link_url = current_link_url();
		if (!inlines->empty()) {
			InlineNode &previous = inlines->back();
			if (previous.kind == InlineKind::TEXT &&
					previous.style_flags == style_flags &&
					previous.link_url == link_url) {
				previous.text.append(text);
				return;
			}
		}

		InlineNode node;
		node.kind = InlineKind::TEXT;
		node.text.assign(text.data(), text.size());
		node.style_flags = style_flags;
		node.link_url = link_url;
		inlines->push_back(std::move(node));
	}

	void append_break_inline(InlineKind break_kind) {
		if (!image_stack.empty()) {
			image_stack.back().alt.push_back(break_kind == InlineKind::HARD_BREAK ? '\n' : ' ');
			return;
		}

		std::vector<InlineNode> *inlines = ensure_current_inlines();
		if (inlines == nullptr) {
			return;
		}

		InlineNode node;
		node.kind = break_kind;
		inlines->push_back(std::move(node));
	}
};

int on_enter_block(MD_BLOCKTYPE block_type, void *detail, void *userdata) {
	auto *context = static_cast<ParserContext *>(userdata);
	if (block_type == MD_BLOCK_HTML) {
		++context->html_block_depth;
		return 0;
	}
	if (context->html_block_depth > 0) {
		return 0;
	}

	switch (block_type) {
		case MD_BLOCK_DOC: {
			ParserFrame frame;
			frame.kind = FrameKind::DOCUMENT;
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_P: {
			context->close_implicit_list_item_paragraph();
			ParserFrame frame;
			frame.kind = FrameKind::BLOCK;
			frame.block.kind = BlockKind::PARAGRAPH;
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_H: {
			context->close_implicit_list_item_paragraph();
			const auto *heading_detail = static_cast<const MD_BLOCK_H_DETAIL *>(detail);
			ParserFrame frame;
			frame.kind = FrameKind::BLOCK;
			frame.block.kind = BlockKind::HEADING;
			frame.block.heading_level = heading_detail != nullptr ? static_cast<int>(heading_detail->level) : 1;
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_QUOTE: {
			context->close_implicit_list_item_paragraph();
			ParserFrame frame;
			frame.kind = FrameKind::BLOCK;
			frame.block.kind = BlockKind::BLOCK_QUOTE;
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_UL: {
			context->close_implicit_list_item_paragraph();
			const auto *list_detail = static_cast<const MD_BLOCK_UL_DETAIL *>(detail);
			ParserFrame frame;
			frame.kind = FrameKind::BLOCK;
			frame.block.kind = BlockKind::LIST;
			frame.block.list_ordered = false;
			frame.block.list_start = 1;
			frame.block.list_tight = list_detail != nullptr ? (list_detail->is_tight != 0) : true;
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_OL: {
			context->close_implicit_list_item_paragraph();
			const auto *list_detail = static_cast<const MD_BLOCK_OL_DETAIL *>(detail);
			ParserFrame frame;
			frame.kind = FrameKind::BLOCK;
			frame.block.kind = BlockKind::LIST;
			frame.block.list_ordered = true;
			frame.block.list_start = list_detail != nullptr ? static_cast<int>(list_detail->start) : 1;
			frame.block.list_tight = list_detail != nullptr ? (list_detail->is_tight != 0) : true;
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_LI: {
			const auto *item_detail = static_cast<const MD_BLOCK_LI_DETAIL *>(detail);
			ParserFrame frame;
			frame.kind = FrameKind::LIST_ITEM;
			if (item_detail != nullptr && item_detail->is_task != 0) {
				frame.list_item.is_task = true;
				frame.list_item.checked = (item_detail->task_mark == 'x' || item_detail->task_mark == 'X');
			}
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_CODE: {
			context->close_implicit_list_item_paragraph();
			const auto *code_detail = static_cast<const MD_BLOCK_CODE_DETAIL *>(detail);
			ParserFrame frame;
			frame.kind = FrameKind::BLOCK;
			frame.block.kind = BlockKind::CODE_BLOCK;
			if (code_detail != nullptr) {
				const std::string decoded_info = decode_attribute(code_detail->info);
				frame.block.code_language = extract_first_word(decoded_info);
			}
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_HR: {
			context->close_implicit_list_item_paragraph();
			Block horizontal_rule;
			horizontal_rule.kind = BlockKind::THEMATIC_BREAK;
			context->append_completed_block(std::move(horizontal_rule));
			break;
		}
		case MD_BLOCK_TABLE: {
			context->close_implicit_list_item_paragraph();
			ParserFrame frame;
			frame.kind = FrameKind::BLOCK;
			frame.block.kind = BlockKind::TABLE;
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_THEAD: {
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK &&
					context->frames.back().block.kind == BlockKind::TABLE) {
				context->frames.back().in_table_header = true;
			}
			break;
		}
		case MD_BLOCK_TBODY: {
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK &&
					context->frames.back().block.kind == BlockKind::TABLE) {
				context->frames.back().in_table_header = false;
			}
			break;
		}
		case MD_BLOCK_TR: {
			bool row_is_header = false;
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK &&
					context->frames.back().block.kind == BlockKind::TABLE) {
				row_is_header = context->frames.back().in_table_header;
			}
			ParserFrame frame;
			frame.kind = FrameKind::TABLE_ROW;
			frame.table_row_is_header = row_is_header;
			context->frames.push_back(std::move(frame));
			break;
		}
		case MD_BLOCK_TH:
		case MD_BLOCK_TD: {
			const auto *cell_detail = static_cast<const MD_BLOCK_TD_DETAIL *>(detail);
			ParserFrame frame;
			frame.kind = FrameKind::TABLE_CELL;
			frame.table_cell.alignment = cell_detail != nullptr
					? convert_table_alignment(cell_detail->align)
					: TableAlignment::DEFAULT;
			context->frames.push_back(std::move(frame));
			break;
		}
		default:
			break;
	}
	return 0;
}

int on_leave_block(MD_BLOCKTYPE block_type, void *detail, void *userdata) {
	(void)detail;
	auto *context = static_cast<ParserContext *>(userdata);
	if (block_type == MD_BLOCK_HTML) {
		if (context->html_block_depth > 0) {
			--context->html_block_depth;
		}
		return 0;
	}
	if (context->html_block_depth > 0) {
		return 0;
	}

	switch (block_type) {
		case MD_BLOCK_DOC: {
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::DOCUMENT) {
				context->frames.pop_back();
			}
			break;
		}
		case MD_BLOCK_P: {
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK) {
				Block completed = std::move(context->frames.back().block);
				context->frames.pop_back();
				promote_single_image_paragraph(completed);
				context->append_completed_block(std::move(completed));
			}
			break;
		}
		case MD_BLOCK_CODE: {
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK) {
				Block completed = std::move(context->frames.back().block);
				context->frames.pop_back();
				// md4c appends a trailing newline to each code line; strip the final
				// newline so fenced blocks do not render an extra empty bottom line.
				if (!completed.code_text.empty() && completed.code_text.back() == '\n') {
					completed.code_text.pop_back();
				}
				context->append_completed_block(std::move(completed));
			}
			break;
		}
		case MD_BLOCK_H:
		case MD_BLOCK_QUOTE:
		case MD_BLOCK_UL:
		case MD_BLOCK_OL:
		case MD_BLOCK_TABLE: {
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK) {
				Block completed = std::move(context->frames.back().block);
				context->frames.pop_back();
				context->append_completed_block(std::move(completed));
			}
			break;
		}
		case MD_BLOCK_LI: {
			context->close_implicit_list_item_paragraph();
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::LIST_ITEM) {
				ListItem completed = std::move(context->frames.back().list_item);
				context->frames.pop_back();
				if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK &&
						context->frames.back().block.kind == BlockKind::LIST) {
					context->frames.back().block.list_items.push_back(std::move(completed));
				}
			}
			break;
		}
		case MD_BLOCK_TH:
		case MD_BLOCK_TD: {
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::TABLE_CELL) {
				TableCell completed = std::move(context->frames.back().table_cell);
				context->frames.pop_back();
				if (!context->frames.empty() && context->frames.back().kind == FrameKind::TABLE_ROW) {
					context->frames.back().table_row.cells.push_back(std::move(completed));
				}
			}
			break;
		}
		case MD_BLOCK_TR: {
			if (!context->frames.empty() && context->frames.back().kind == FrameKind::TABLE_ROW) {
				TableRow completed = std::move(context->frames.back().table_row);
				const bool is_header = context->frames.back().table_row_is_header;
				context->frames.pop_back();

				if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK &&
						context->frames.back().block.kind == BlockKind::TABLE) {
					Block &table_block = context->frames.back().block;
					if (is_header) {
						if (table_block.table_alignments.empty()) {
							table_block.table_alignments.reserve(completed.cells.size());
							for (const TableCell &cell : completed.cells) {
								table_block.table_alignments.push_back(cell.alignment);
							}
						}
						table_block.table_header_rows.push_back(std::move(completed));
					} else {
						table_block.table_body_rows.push_back(std::move(completed));
					}
				}
			}
			break;
		}
		case MD_BLOCK_THEAD:
		case MD_BLOCK_TBODY:
		case MD_BLOCK_HR:
		default:
			break;
	}
	return 0;
}

int on_enter_span(MD_SPANTYPE span_type, void *detail, void *userdata) {
	auto *context = static_cast<ParserContext *>(userdata);
	if (context->html_block_depth > 0) {
		return 0;
	}

	switch (span_type) {
		case MD_SPAN_STRONG:
			++context->bold_depth;
			break;
		case MD_SPAN_EM:
			++context->italic_depth;
			break;
		case MD_SPAN_DEL:
			++context->strikethrough_depth;
			break;
		case MD_SPAN_CODE:
			++context->code_depth;
			break;
		case MD_SPAN_A: {
			const auto *link_detail = static_cast<const MD_SPAN_A_DETAIL *>(detail);
			context->link_stack.push_back(link_detail != nullptr ? decode_attribute(link_detail->href) : std::string());
			break;
		}
		case MD_SPAN_IMG: {
			const auto *image_detail = static_cast<const MD_SPAN_IMG_DETAIL *>(detail);
			ImageCapture capture;
			if (image_detail != nullptr) {
				capture.source = decode_attribute(image_detail->src);
				capture.title = decode_attribute(image_detail->title);
			}
			capture.link_url = context->current_link_url();
			context->image_stack.push_back(std::move(capture));
			break;
		}
		default:
			break;
	}
	return 0;
}

int on_leave_span(MD_SPANTYPE span_type, void *detail, void *userdata) {
	(void)detail;
	auto *context = static_cast<ParserContext *>(userdata);
	if (context->html_block_depth > 0) {
		return 0;
	}

	switch (span_type) {
		case MD_SPAN_STRONG:
			if (context->bold_depth > 0) {
				--context->bold_depth;
			}
			break;
		case MD_SPAN_EM:
			if (context->italic_depth > 0) {
				--context->italic_depth;
			}
			break;
		case MD_SPAN_DEL:
			if (context->strikethrough_depth > 0) {
				--context->strikethrough_depth;
			}
			break;
		case MD_SPAN_CODE:
			if (context->code_depth > 0) {
				--context->code_depth;
			}
			break;
		case MD_SPAN_A:
			if (!context->link_stack.empty()) {
				context->link_stack.pop_back();
			}
			break;
		case MD_SPAN_IMG: {
			if (context->image_stack.empty()) {
				break;
			}
			ImageCapture completed = std::move(context->image_stack.back());
			context->image_stack.pop_back();

			if (!context->image_stack.empty()) {
				context->image_stack.back().alt.append(completed.alt);
			} else {
				std::vector<InlineNode> *inlines = context->ensure_current_inlines();
				if (inlines != nullptr) {
					InlineNode image_node;
					image_node.kind = InlineKind::IMAGE;
					image_node.image_source = std::move(completed.source);
					image_node.image_alt = std::move(completed.alt);
					image_node.image_title = std::move(completed.title);
					image_node.link_url = std::move(completed.link_url);
					inlines->push_back(std::move(image_node));
				}
			}
			break;
		}
		default:
			break;
	}
	return 0;
}

int on_text(MD_TEXTTYPE text_type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
	auto *context = static_cast<ParserContext *>(userdata);
	if (context->html_block_depth > 0) {
		return 0;
	}

	const std::string_view slice(text != nullptr ? text : "", size);

	if (!context->frames.empty() && context->frames.back().kind == FrameKind::BLOCK &&
			context->frames.back().block.kind == BlockKind::CODE_BLOCK) {
		if (text_type == MD_TEXT_NULLCHAR) {
			context->frames.back().block.code_text.append(UTF8_REPLACEMENT_CHARACTER);
		} else {
			context->frames.back().block.code_text.append(slice);
		}
		return 0;
	}

	switch (text_type) {
		case MD_TEXT_NORMAL:
		case MD_TEXT_CODE:
			context->append_text_inline(slice);
			break;
		case MD_TEXT_NULLCHAR:
			context->append_text_inline(UTF8_REPLACEMENT_CHARACTER);
			break;
		case MD_TEXT_ENTITY: {
			const std::string decoded = decode_entity(slice);
			context->append_text_inline(decoded);
			break;
		}
		case MD_TEXT_SOFTBR:
			context->append_break_inline(InlineKind::SOFT_BREAK);
			break;
		case MD_TEXT_BR:
			context->append_break_inline(InlineKind::HARD_BREAK);
			break;
		case MD_TEXT_HTML:
			if (is_html_line_break_tag(slice)) {
				context->append_break_inline(InlineKind::HARD_BREAK);
			}
			break;
		default:
			break;
	}
	return 0;
}

struct ParsedCalloutHeader {
	std::string kind;
	std::string custom_title;
	bool collapsible = false;
	bool initially_collapsed = false;
	bool has_title_separator = false;
};

// Matches ^\[!([A-Za-z][A-Za-z0-9_-]*)\](\+|-)?(?:[ \t]+([^\n]*))?$ without regex.
bool scan_callout_directive(std::string_view text, ParsedCalloutHeader &output) {
	if (text.size() < 4 || text[0] != '[' || text[1] != '!') {
		return false;
	}
	if (!is_ascii_alpha(text[2])) {
		return false;
	}

	size_t position = 2;
	while (position < text.size() &&
			(is_ascii_alphanumeric(text[position]) || text[position] == '_' || text[position] == '-')) {
		++position;
	}

	if (position >= text.size() || text[position] != ']') {
		return false;
	}

	std::string uppercase_kind;
	uppercase_kind.reserve(position - 2);
	for (size_t kind_index = 2; kind_index < position; ++kind_index) {
		uppercase_kind.push_back(ascii_to_upper(text[kind_index]));
	}
	++position;

	bool collapsible = false;
	bool initially_collapsed = false;
	if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
		collapsible = true;
		initially_collapsed = (text[position] == '-');
		++position;
	}

	bool has_title_separator = false;
	std::string custom_title;
	if (position < text.size()) {
		if (text[position] != ' ' && text[position] != '\t') {
			return false;
		}
		has_title_separator = true;
		while (position < text.size() && (text[position] == ' ' || text[position] == '\t')) {
			++position;
		}
		const size_t title_start = position;
		while (position < text.size() && text[position] != '\n') {
			++position;
		}
		if (position != text.size()) {
			return false;
		}
		custom_title = trim_horizontal_whitespace(text.substr(title_start, position - title_start));
	}

	output.kind = std::move(uppercase_kind);
	output.custom_title = std::move(custom_title);
	output.collapsible = collapsible;
	output.initially_collapsed = initially_collapsed;
	output.has_title_separator = has_title_separator;
	return true;
}

// Post-parse AST transform: promotes blockquotes whose opening plain text line
// starts with a GitHub/Obsidian alert directive (`[!KIND][+-]? [title]`) into
// BlockKind::CALLOUT, stripping the directive line and preserving remaining
// inline/block body content.
void try_promote_blockquote_to_callout(Block &block) {
	if (block.kind != BlockKind::BLOCK_QUOTE || block.children.empty()) {
		return;
	}

	Block &first_child = block.children.front();
	if (first_child.kind != BlockKind::PARAGRAPH || first_child.inlines.empty()) {
		return;
	}

	const InlineNode &first_inline = first_child.inlines.front();
	if (first_inline.kind != InlineKind::TEXT ||
			first_inline.style_flags != INLINE_STYLE_NONE ||
			!first_inline.link_url.empty()) {
		return;
	}

	ParsedCalloutHeader header;
	if (!scan_callout_directive(first_inline.text, header)) {
		return;
	}

	size_t header_end_index = 1;
	if (first_child.inlines.size() > 1) {
		const InlineKind second_kind = first_child.inlines[1].kind;
		if (second_kind != InlineKind::SOFT_BREAK && second_kind != InlineKind::HARD_BREAK) {
			if (!header.has_title_separator) {
				return;
			}
			std::string extra_title = header.custom_title;
			while (header_end_index < first_child.inlines.size()) {
				const InlineNode &node = first_child.inlines[header_end_index];
				if (node.kind == InlineKind::SOFT_BREAK || node.kind == InlineKind::HARD_BREAK) {
					break;
				}
				if (node.kind == InlineKind::TEXT) {
					extra_title.append(node.text);
				} else if (node.kind == InlineKind::IMAGE) {
					extra_title.append(node.image_alt);
				}
				++header_end_index;
			}
			header.custom_title = trim_horizontal_whitespace(extra_title);
		}
	}

	size_t erase_end_index = header_end_index;
	if (erase_end_index < first_child.inlines.size() &&
			(first_child.inlines[erase_end_index].kind == InlineKind::SOFT_BREAK ||
					first_child.inlines[erase_end_index].kind == InlineKind::HARD_BREAK)) {
		++erase_end_index;
	}

	first_child.inlines.erase(
			first_child.inlines.begin(),
			first_child.inlines.begin() + static_cast<std::ptrdiff_t>(erase_end_index));

	if (first_child.inlines.empty()) {
		block.children.erase(block.children.begin());
	}

	block.kind = BlockKind::CALLOUT;
	block.callout_kind = std::move(header.kind);
	block.callout_custom_title = std::move(header.custom_title);
	block.callout_collapsible = header.collapsible;
	block.callout_initially_collapsed = header.initially_collapsed;
}

void promote_callouts_recursive(std::vector<Block> &blocks) {
	for (Block &block : blocks) {
		if (block.kind == BlockKind::BLOCK_QUOTE) {
			try_promote_blockquote_to_callout(block);
		}
		if (block.kind == BlockKind::BLOCK_QUOTE || block.kind == BlockKind::CALLOUT) {
			promote_callouts_recursive(block.children);
		} else if (block.kind == BlockKind::LIST) {
			for (ListItem &item : block.list_items) {
				promote_callouts_recursive(item.children);
			}
		}
	}
}

// Assigns deterministic zero-based task indices in pre-order DFS traversal so
// UI task toggles map 1:1 to Document::initial_task_states across re-layouts.
void assign_task_indices_recursive(std::vector<Block> &blocks, std::vector<bool> &initial_task_states) {
	for (Block &block : blocks) {
		if (block.kind == BlockKind::BLOCK_QUOTE || block.kind == BlockKind::CALLOUT) {
			assign_task_indices_recursive(block.children, initial_task_states);
		} else if (block.kind == BlockKind::LIST) {
			for (ListItem &item : block.list_items) {
				if (item.is_task) {
					item.task_index = static_cast<int>(initial_task_states.size());
					initial_task_states.push_back(item.checked);
				} else {
					item.task_index = -1;
				}
				assign_task_indices_recursive(item.children, initial_task_states);
			}
		}
	}
}

} // namespace

Document parse_markdown(std::string_view source) {
	ParserContext context;
	if (source.empty()) {
		return context.document;
	}

	MD_PARSER parser = {};
	parser.abi_version = 0;
	parser.flags = MD_FLAG_TABLES |
			MD_FLAG_STRIKETHROUGH |
			MD_FLAG_TASKLISTS |
			MD_FLAG_PERMISSIVEAUTOLINKS |
			MD_FLAG_COLLAPSEWHITESPACE;
	parser.enter_block = on_enter_block;
	parser.leave_block = on_leave_block;
	parser.enter_span = on_enter_span;
	parser.leave_span = on_leave_span;
	parser.text = on_text;
	parser.debug_log = nullptr;
	parser.syntax = nullptr;

	md_parse(source.data(), static_cast<MD_SIZE>(source.size()), &parser, &context);

	promote_callouts_recursive(context.document.blocks);
	assign_task_indices_recursive(context.document.blocks, context.document.initial_task_states);

	return context.document;
}

} // namespace gdmarkdown
