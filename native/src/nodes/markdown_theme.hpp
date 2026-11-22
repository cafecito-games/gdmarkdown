// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#pragma once

#include "nodes/markdown_callout_style.hpp"

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

// Reusable Resource configuring fonts, typography, colors, StyleBox overrides,
// box metrics, and callout styles for MarkdownView. Every mutating setter
// clamps invalid values, skips no-op writes, and emits Resource::changed so
// attached views re-layout immediately.
class MarkdownTheme : public Resource {
	GDCLASS(MarkdownTheme, Resource)

private:
	// Fonts
	Ref<Font> regular_font;
	Ref<Font> bold_font;
	Ref<Font> italic_font;
	Ref<Font> bold_italic_font;
	Ref<Font> monospace_font;

	// Typography & font-relative spacing
	int base_font_size = 16;
	float code_font_scale = 0.9f;
	float line_spacing = 4.0f;
	float paragraph_spacing_em = 0.75f;
	PackedFloat32Array heading_scales;
	float heading_top_spacing_em = 1.1f;
	float heading_bottom_spacing_em = 0.4f;
	float list_indent_em = 1.5f;
	float list_item_spacing_em = 0.3f;
	float blockquote_indent_em = 1.0f;

	// Colors (default: sleek dark palette)
	Color text_color = Color(0.90f, 0.92f, 0.95f, 1.0f);
	Color heading_color = Color(0.98f, 0.99f, 1.00f, 1.0f);
	Color muted_color = Color(0.60f, 0.65f, 0.72f, 1.0f);
	Color link_color = Color(0.42f, 0.70f, 1.00f, 1.0f);
	Color link_hover_color = Color(0.62f, 0.82f, 1.00f, 1.0f);
	Color code_text_color = Color(0.89f, 0.78f, 0.56f, 1.0f);
	Color code_bg_color = Color(0.11f, 0.13f, 0.17f, 1.0f);
	Color inline_code_bg_color = Color(0.18f, 0.21f, 0.27f, 0.95f);
	Color strikethrough_color = Color(0.60f, 0.65f, 0.72f, 0.85f);
	Color rule_color = Color(0.26f, 0.30f, 0.37f, 0.9f);
	Color blockquote_accent_color = Color(0.38f, 0.46f, 0.58f, 1.0f);
	Color blockquote_bg_color = Color(0.14f, 0.17f, 0.22f, 0.65f);
	Color table_border_color = Color(0.25f, 0.29f, 0.36f, 1.0f);
	Color table_header_bg_color = Color(0.16f, 0.19f, 0.25f, 1.0f);
	Color table_alt_row_bg_color = Color(0.13f, 0.16f, 0.21f, 0.55f);
	Color task_checked_color = Color(0.29f, 0.78f, 0.52f, 1.0f);
	Color task_unchecked_color = Color(0.48f, 0.53f, 0.62f, 1.0f);

	// Optional StyleBoxes (procedural defaults used when null)
	Ref<StyleBox> code_block_style;
	Ref<StyleBox> inline_code_style;
	Ref<StyleBox> blockquote_style;
	Ref<StyleBox> table_style;
	Ref<StyleBox> table_header_style;

	// Box metrics
	float inline_code_padding_horizontal = 5.0f;
	float inline_code_padding_vertical = 1.5f;
	float code_block_padding = 12.0f;
	float blockquote_padding = 10.0f;
	float blockquote_accent_width = 3.0f;
	float callout_padding = 12.0f;
	float callout_accent_width = 4.0f;
	float callout_icon_size = 16.0f;
	float table_cell_padding_horizontal = 10.0f;
	float table_cell_padding_vertical = 7.0f;
	float rule_thickness = 1.0f;
	float corner_radius = 6.0f;
	float max_image_height = 420.0f;
	float inline_image_scale = 1.15f;

	// Extensible callout styles ("NOTE", "TIP", "IMPORTANT", "WARNING", "CAUTION", ...)
	Dictionary callouts;

	void _populate_default_callouts(bool p_light_mode);
	void _disconnect_callout_signals();
	void _connect_callout_signal(const Ref<MarkdownCalloutStyle> &p_style);
	void _on_callout_style_changed();

protected:
	static void _bind_methods();

public:
	MarkdownTheme();
	~MarkdownTheme() override;

	// Fonts
	void set_regular_font(const Ref<Font> &p_font);
	Ref<Font> get_regular_font() const;

	void set_bold_font(const Ref<Font> &p_font);
	Ref<Font> get_bold_font() const;

	void set_italic_font(const Ref<Font> &p_font);
	Ref<Font> get_italic_font() const;

	void set_bold_italic_font(const Ref<Font> &p_font);
	Ref<Font> get_bold_italic_font() const;

	void set_monospace_font(const Ref<Font> &p_font);
	Ref<Font> get_monospace_font() const;

	// Typography & rhythm
	void set_base_font_size(int p_size);
	int get_base_font_size() const;

	void set_code_font_scale(float p_scale);
	float get_code_font_scale() const;

	void set_line_spacing(float p_spacing);
	float get_line_spacing() const;

	void set_paragraph_spacing_em(float p_spacing);
	float get_paragraph_spacing_em() const;

	// Heading multipliers for H1..H6; get_heading_scale() clamps level to 1..6
	// and falls back to built-in defaults if an entry is missing or <= 0.
	void set_heading_scales(const PackedFloat32Array &p_scales);
	PackedFloat32Array get_heading_scales() const;
	float get_heading_scale(int p_level) const;

	void set_heading_top_spacing_em(float p_spacing);
	float get_heading_top_spacing_em() const;

	void set_heading_bottom_spacing_em(float p_spacing);
	float get_heading_bottom_spacing_em() const;

	void set_list_indent_em(float p_indent);
	float get_list_indent_em() const;

	void set_list_item_spacing_em(float p_spacing);
	float get_list_item_spacing_em() const;

	void set_blockquote_indent_em(float p_indent);
	float get_blockquote_indent_em() const;

	// Colors
	void set_text_color(const Color &p_color);
	Color get_text_color() const;

	void set_heading_color(const Color &p_color);
	Color get_heading_color() const;

	void set_muted_color(const Color &p_color);
	Color get_muted_color() const;

	void set_link_color(const Color &p_color);
	Color get_link_color() const;

	void set_link_hover_color(const Color &p_color);
	Color get_link_hover_color() const;

	void set_code_text_color(const Color &p_color);
	Color get_code_text_color() const;

	void set_code_bg_color(const Color &p_color);
	Color get_code_bg_color() const;

	void set_inline_code_bg_color(const Color &p_color);
	Color get_inline_code_bg_color() const;

	void set_strikethrough_color(const Color &p_color);
	Color get_strikethrough_color() const;

	void set_rule_color(const Color &p_color);
	Color get_rule_color() const;

	void set_blockquote_accent_color(const Color &p_color);
	Color get_blockquote_accent_color() const;

	void set_blockquote_bg_color(const Color &p_color);
	Color get_blockquote_bg_color() const;

	void set_table_border_color(const Color &p_color);
	Color get_table_border_color() const;

	void set_table_header_bg_color(const Color &p_color);
	Color get_table_header_bg_color() const;

	void set_table_alt_row_bg_color(const Color &p_color);
	Color get_table_alt_row_bg_color() const;

	void set_task_checked_color(const Color &p_color);
	Color get_task_checked_color() const;

	void set_task_unchecked_color(const Color &p_color);
	Color get_task_unchecked_color() const;

	// StyleBoxes
	void set_code_block_style(const Ref<StyleBox> &p_style);
	Ref<StyleBox> get_code_block_style() const;

	void set_inline_code_style(const Ref<StyleBox> &p_style);
	Ref<StyleBox> get_inline_code_style() const;

	void set_blockquote_style(const Ref<StyleBox> &p_style);
	Ref<StyleBox> get_blockquote_style() const;

	void set_table_style(const Ref<StyleBox> &p_style);
	Ref<StyleBox> get_table_style() const;

	void set_table_header_style(const Ref<StyleBox> &p_style);
	Ref<StyleBox> get_table_header_style() const;

	// Metrics
	void set_inline_code_padding_horizontal(float p_padding);
	float get_inline_code_padding_horizontal() const;

	void set_inline_code_padding_vertical(float p_padding);
	float get_inline_code_padding_vertical() const;

	void set_code_block_padding(float p_padding);
	float get_code_block_padding() const;

	void set_blockquote_padding(float p_padding);
	float get_blockquote_padding() const;

	void set_blockquote_accent_width(float p_width);
	float get_blockquote_accent_width() const;

	void set_callout_padding(float p_padding);
	float get_callout_padding() const;

	void set_callout_accent_width(float p_width);
	float get_callout_accent_width() const;

	void set_callout_icon_size(float p_size);
	float get_callout_icon_size() const;

	void set_table_cell_padding_horizontal(float p_padding);
	float get_table_cell_padding_horizontal() const;

	void set_table_cell_padding_vertical(float p_padding);
	float get_table_cell_padding_vertical() const;

	void set_rule_thickness(float p_thickness);
	float get_rule_thickness() const;

	void set_corner_radius(float p_radius);
	float get_corner_radius() const;

	void set_max_image_height(float p_height);
	float get_max_image_height() const;

	void set_inline_image_scale(float p_scale);
	float get_inline_image_scale() const;

	// Callouts (keys are trimmed and normalized to uppercase; get_callout_style
	// synthesizes a fallback style for unregistered kinds).
	void set_callouts(const Dictionary &p_callouts);
	Dictionary get_callouts() const;
	void set_callout_style(const String &p_kind, const Ref<MarkdownCalloutStyle> &p_style);
	Ref<MarkdownCalloutStyle> get_callout_style(const String &p_kind) const;

	// Built-in dark and light theme presets.
	static Ref<MarkdownTheme> create_dark();
	static Ref<MarkdownTheme> create_light();
};

} // namespace godot
