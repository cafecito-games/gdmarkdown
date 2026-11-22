// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#pragma once

#include "nodes/markdown_theme.hpp"
#include "parser/document.hpp"

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/text_paragraph.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <functional>
#include <vector>

namespace gdmarkdown {

// Character range within a single LaidOutParagraph that requires per-glyph
// styling, hover coloring, or background/line decorations beyond base_color.
struct ColorSpan {
	int start = 0;
	int end = 0;
	godot::Color color;
	godot::String link_url;
	bool is_code = false;
	bool is_strikethrough = false;
};

// Paragraph-local rectangle (such as an inline code pill) tied to a character
// range so typewriter cutoffs can clip it proportionally.
struct DecorationRect {
	godot::Rect2 rectangle;
	int char_start = 0;
	int char_end = 0;
	godot::String link_url;
};

// Paragraph-local horizontal stroke (link underline or strikethrough) tied to a
// character range for typewriter clipping and link hover color changes.
struct DecorationLine {
	godot::Vector2 start_point;
	godot::Vector2 end_point;
	float thickness = 1.0f;
	godot::Color color;
	int char_start = 0;
	int char_end = 0;
	godot::String link_url;
};

// Resolved position and texture for an inline image embedded in a TextParagraph.
struct InlineImagePlacement {
	godot::Rect2 rectangle;
	godot::Ref<godot::Texture2D> texture;
	int char_index = 0;
};

// A shaped Godot TextParagraph plus its document position, typewriter character
// range, per-span metadata, and precomputed decoration geometry.
struct LaidOutParagraph {
	godot::Ref<godot::TextParagraph> paragraph;
	godot::Vector2 position;
	godot::Vector2 size;
	int paragraph_index = 0;
	// Inclusive start / exclusive end in LayoutDocument's countable character stream.
	// Equal start and end denotes non-countable chrome (e.g., list markers).
	int document_char_start = 0;
	int document_char_end = 0;
	int local_char_count = 0;
	godot::Color base_color;
	// False allows draw_laid_out_paragraph() to take TextParagraph::draw()'s fast path.
	bool has_rich_spans = false;
	std::vector<ColorSpan> spans;
	std::vector<DecorationRect> inline_code_pills;
	std::vector<DecorationLine> strikethrough_lines;
	std::vector<DecorationLine> link_underlines;
	std::vector<InlineImagePlacement> inline_images;
};

// Ordered paint operations emitted by build_layout() and executed by MarkdownView.
enum class PaintPrimitiveKind : uint8_t {
	ROUNDED_PANEL = 0,
	STYLE_BOX = 1,
	ACCENT_BAR = 2,
	HORIZONTAL_RULE = 3,
	TABLE_GRID = 4,
	CALLOUT_ICON = 5,
	TASK_CHECKBOX = 6,
	BLOCK_IMAGE = 7,
	PARAGRAPH = 8,
	SCROLLABLE_CODE_BLOCK = 9,
};

// A single back-to-front draw command in document coordinates.
struct PaintPrimitive {
	PaintPrimitiveKind kind = PaintPrimitiveKind::ROUNDED_PANEL;
	godot::Rect2 rectangle;
	godot::Color fill_color;
	godot::Color border_color;
	float border_width = 0.0f;
	float corner_radius = 0.0f;
	// Minimum visible_characters required before this primitive becomes visible.
	int document_char_threshold = 0;

	godot::Ref<godot::StyleBox> style_box;
	godot::Ref<godot::Texture2D> texture;

	// Callout icon
	godot::String callout_kind;

	// Task checkbox
	int task_index = -1;

	// Paragraph reference (index into LayoutDocument::paragraphs)
	int paragraph_slot = -1;

	// Scrollable code block reference (index into LayoutDocument::code_scroll_regions)
	int code_scroll_slot = -1;

	// Table grid metadata
	std::vector<float> table_column_offsets;
	std::vector<float> table_row_offsets;
	int table_header_row_count = 0;
	godot::Color table_header_bg;
	godot::Color table_alt_row_bg;
};

// Interactive clickable hyperlink region in document coordinates.
struct LinkHitBox {
	godot::Rect2 rectangle;
	godot::String url;
};

// Interactive task-list checkbox hit region in document coordinates (expanded
// by 4 px on each side for comfortable pointer/touch targeting).
struct TaskHitBox {
	godot::Rect2 rectangle;
	int task_index = -1;
};

// State and clip canvas item for an overflowing non-wrapped code block.
// MarkdownView owns the lifecycle of clip_canvas_item on RenderingServer.
struct CodeScrollRegion {
	godot::Rect2 outer_rectangle;
	godot::Rect2 inner_rectangle;
	int paragraph_slot = -1;
	float content_width = 0.0f;
	float max_scroll_x = 0.0f;
	float scroll_x = 0.0f;
	godot::RID clip_canvas_item;
};

// Complete geometry, paint list, and hit-test structures produced by build_layout().
struct LayoutDocument {
	float layout_width = 0.0f;
	float total_height = 0.0f;
	int total_character_count = 0;
	godot::String plain_text;

	std::vector<LaidOutParagraph> paragraphs;
	std::vector<PaintPrimitive> primitives;
	std::vector<LinkHitBox> link_hit_boxes;
	std::vector<TaskHitBox> task_hit_boxes;
	std::vector<CodeScrollRegion> code_scroll_regions;
};

// Resolves a Markdown image URI to a Godot Texture2D (or null if unresolved).
using ImageResolverCallback = std::function<godot::Ref<godot::Texture2D>(const godot::String &)>;

// Configuration inputs for build_layout().
struct LayoutOptions {
	float available_width = 400.0f;
	bool code_block_wrap = true;
	godot::Ref<godot::Font> fallback_font;
	godot::Ref<godot::MarkdownTheme> theme;
	ImageResolverCallback resolve_image;
};

// Shapes and positions every block in `document` for `options`, returning a
// self-contained LayoutDocument ready for rendering and hit-testing.
LayoutDocument build_layout(const Document &document, const LayoutOptions &options);

// Draws a single LaidOutParagraph onto canvas_item, honoring per-span colors,
// hover link highlight, and optional character cutoff (-1 for unlimited).
void draw_laid_out_paragraph(
		godot::RID canvas_item,
		const LaidOutParagraph &laid_out,
		const godot::Vector2 &draw_offset,
		const godot::Ref<godot::MarkdownTheme> &theme,
		const godot::String &hovered_link_url,
		int visible_characters);

// Draws a procedural vector badge icon for built-in or custom callout kinds.
void draw_procedural_callout_icon(
		godot::RID canvas_item,
		const godot::Rect2 &icon_rectangle,
		const godot::String &callout_kind,
		const godot::Color &color);

// Draws a crisp rounded task-list checkbox (checked or unchecked).
void draw_task_checkbox(
		godot::RID canvas_item,
		const godot::Rect2 &box_rectangle,
		bool checked,
		const godot::Color &checked_color,
		const godot::Color &unchecked_color);

// Draws a rounded rectangle panel using StyleBoxFlat-equivalent geometry.
void draw_rounded_panel(
		godot::RID canvas_item,
		const godot::Rect2 &rectangle,
		const godot::Color &fill_color,
		const godot::Color &border_color,
		float border_width,
		float corner_radius);

} // namespace gdmarkdown
