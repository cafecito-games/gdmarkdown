// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#include "nodes/markdown_theme.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>

#include <algorithm>
#include <cmath>

namespace godot {

namespace {

constexpr float DEFAULT_HEADING_SCALES[6] = { 1.85f, 1.5f, 1.25f, 1.1f, 1.0f, 0.9f };

bool float_equals(float left, float right) {
	return std::fabs(left - right) <= 1e-6f;
}

String format_callout_fallback_title(const String &normalized_kind) {
	if (normalized_kind.is_empty()) {
		return String("Note");
	}
	String spaced = normalized_kind.replace("_", " ").replace("-", " ");
	return spaced.to_lower().capitalize();
}

} // namespace

MarkdownTheme::MarkdownTheme() {
	heading_scales.resize(6);
	for (int index = 0; index < 6; ++index) {
		heading_scales.set(index, DEFAULT_HEADING_SCALES[index]);
	}
	_populate_default_callouts(false);
}

MarkdownTheme::~MarkdownTheme() {
	_disconnect_callout_signals();
}

void MarkdownTheme::_bind_methods() {
	// Fonts
	ClassDB::bind_method(D_METHOD("set_regular_font", "font"), &MarkdownTheme::set_regular_font);
	ClassDB::bind_method(D_METHOD("get_regular_font"), &MarkdownTheme::get_regular_font);

	ClassDB::bind_method(D_METHOD("set_bold_font", "font"), &MarkdownTheme::set_bold_font);
	ClassDB::bind_method(D_METHOD("get_bold_font"), &MarkdownTheme::get_bold_font);

	ClassDB::bind_method(D_METHOD("set_italic_font", "font"), &MarkdownTheme::set_italic_font);
	ClassDB::bind_method(D_METHOD("get_italic_font"), &MarkdownTheme::get_italic_font);

	ClassDB::bind_method(D_METHOD("set_bold_italic_font", "font"), &MarkdownTheme::set_bold_italic_font);
	ClassDB::bind_method(D_METHOD("get_bold_italic_font"), &MarkdownTheme::get_bold_italic_font);

	ClassDB::bind_method(D_METHOD("set_monospace_font", "font"), &MarkdownTheme::set_monospace_font);
	ClassDB::bind_method(D_METHOD("get_monospace_font"), &MarkdownTheme::get_monospace_font);

	// Typography & rhythm
	ClassDB::bind_method(D_METHOD("set_base_font_size", "size"), &MarkdownTheme::set_base_font_size);
	ClassDB::bind_method(D_METHOD("get_base_font_size"), &MarkdownTheme::get_base_font_size);

	ClassDB::bind_method(D_METHOD("set_code_font_scale", "scale"), &MarkdownTheme::set_code_font_scale);
	ClassDB::bind_method(D_METHOD("get_code_font_scale"), &MarkdownTheme::get_code_font_scale);

	ClassDB::bind_method(D_METHOD("set_line_spacing", "spacing"), &MarkdownTheme::set_line_spacing);
	ClassDB::bind_method(D_METHOD("get_line_spacing"), &MarkdownTheme::get_line_spacing);

	ClassDB::bind_method(D_METHOD("set_paragraph_spacing_em", "spacing"), &MarkdownTheme::set_paragraph_spacing_em);
	ClassDB::bind_method(D_METHOD("get_paragraph_spacing_em"), &MarkdownTheme::get_paragraph_spacing_em);

	ClassDB::bind_method(D_METHOD("set_heading_scales", "scales"), &MarkdownTheme::set_heading_scales);
	ClassDB::bind_method(D_METHOD("get_heading_scales"), &MarkdownTheme::get_heading_scales);
	ClassDB::bind_method(D_METHOD("get_heading_scale", "level"), &MarkdownTheme::get_heading_scale);

	ClassDB::bind_method(D_METHOD("set_heading_top_spacing_em", "spacing"), &MarkdownTheme::set_heading_top_spacing_em);
	ClassDB::bind_method(D_METHOD("get_heading_top_spacing_em"), &MarkdownTheme::get_heading_top_spacing_em);

	ClassDB::bind_method(D_METHOD("set_heading_bottom_spacing_em", "spacing"), &MarkdownTheme::set_heading_bottom_spacing_em);
	ClassDB::bind_method(D_METHOD("get_heading_bottom_spacing_em"), &MarkdownTheme::get_heading_bottom_spacing_em);

	ClassDB::bind_method(D_METHOD("set_list_indent_em", "indent"), &MarkdownTheme::set_list_indent_em);
	ClassDB::bind_method(D_METHOD("get_list_indent_em"), &MarkdownTheme::get_list_indent_em);

	ClassDB::bind_method(D_METHOD("set_list_item_spacing_em", "spacing"), &MarkdownTheme::set_list_item_spacing_em);
	ClassDB::bind_method(D_METHOD("get_list_item_spacing_em"), &MarkdownTheme::get_list_item_spacing_em);

	ClassDB::bind_method(D_METHOD("set_blockquote_indent_em", "indent"), &MarkdownTheme::set_blockquote_indent_em);
	ClassDB::bind_method(D_METHOD("get_blockquote_indent_em"), &MarkdownTheme::get_blockquote_indent_em);

	// Colors
	ClassDB::bind_method(D_METHOD("set_text_color", "color"), &MarkdownTheme::set_text_color);
	ClassDB::bind_method(D_METHOD("get_text_color"), &MarkdownTheme::get_text_color);

	ClassDB::bind_method(D_METHOD("set_heading_color", "color"), &MarkdownTheme::set_heading_color);
	ClassDB::bind_method(D_METHOD("get_heading_color"), &MarkdownTheme::get_heading_color);

	ClassDB::bind_method(D_METHOD("set_muted_color", "color"), &MarkdownTheme::set_muted_color);
	ClassDB::bind_method(D_METHOD("get_muted_color"), &MarkdownTheme::get_muted_color);

	ClassDB::bind_method(D_METHOD("set_link_color", "color"), &MarkdownTheme::set_link_color);
	ClassDB::bind_method(D_METHOD("get_link_color"), &MarkdownTheme::get_link_color);

	ClassDB::bind_method(D_METHOD("set_link_hover_color", "color"), &MarkdownTheme::set_link_hover_color);
	ClassDB::bind_method(D_METHOD("get_link_hover_color"), &MarkdownTheme::get_link_hover_color);

	ClassDB::bind_method(D_METHOD("set_code_text_color", "color"), &MarkdownTheme::set_code_text_color);
	ClassDB::bind_method(D_METHOD("get_code_text_color"), &MarkdownTheme::get_code_text_color);

	ClassDB::bind_method(D_METHOD("set_code_bg_color", "color"), &MarkdownTheme::set_code_bg_color);
	ClassDB::bind_method(D_METHOD("get_code_bg_color"), &MarkdownTheme::get_code_bg_color);

	ClassDB::bind_method(D_METHOD("set_inline_code_bg_color", "color"), &MarkdownTheme::set_inline_code_bg_color);
	ClassDB::bind_method(D_METHOD("get_inline_code_bg_color"), &MarkdownTheme::get_inline_code_bg_color);

	ClassDB::bind_method(D_METHOD("set_strikethrough_color", "color"), &MarkdownTheme::set_strikethrough_color);
	ClassDB::bind_method(D_METHOD("get_strikethrough_color"), &MarkdownTheme::get_strikethrough_color);

	ClassDB::bind_method(D_METHOD("set_rule_color", "color"), &MarkdownTheme::set_rule_color);
	ClassDB::bind_method(D_METHOD("get_rule_color"), &MarkdownTheme::get_rule_color);

	ClassDB::bind_method(D_METHOD("set_blockquote_accent_color", "color"), &MarkdownTheme::set_blockquote_accent_color);
	ClassDB::bind_method(D_METHOD("get_blockquote_accent_color"), &MarkdownTheme::get_blockquote_accent_color);

	ClassDB::bind_method(D_METHOD("set_blockquote_bg_color", "color"), &MarkdownTheme::set_blockquote_bg_color);
	ClassDB::bind_method(D_METHOD("get_blockquote_bg_color"), &MarkdownTheme::get_blockquote_bg_color);

	ClassDB::bind_method(D_METHOD("set_table_border_color", "color"), &MarkdownTheme::set_table_border_color);
	ClassDB::bind_method(D_METHOD("get_table_border_color"), &MarkdownTheme::get_table_border_color);

	ClassDB::bind_method(D_METHOD("set_table_header_bg_color", "color"), &MarkdownTheme::set_table_header_bg_color);
	ClassDB::bind_method(D_METHOD("get_table_header_bg_color"), &MarkdownTheme::get_table_header_bg_color);

	ClassDB::bind_method(D_METHOD("set_table_alt_row_bg_color", "color"), &MarkdownTheme::set_table_alt_row_bg_color);
	ClassDB::bind_method(D_METHOD("get_table_alt_row_bg_color"), &MarkdownTheme::get_table_alt_row_bg_color);

	ClassDB::bind_method(D_METHOD("set_task_checked_color", "color"), &MarkdownTheme::set_task_checked_color);
	ClassDB::bind_method(D_METHOD("get_task_checked_color"), &MarkdownTheme::get_task_checked_color);

	ClassDB::bind_method(D_METHOD("set_task_unchecked_color", "color"), &MarkdownTheme::set_task_unchecked_color);
	ClassDB::bind_method(D_METHOD("get_task_unchecked_color"), &MarkdownTheme::get_task_unchecked_color);

	// StyleBoxes
	ClassDB::bind_method(D_METHOD("set_code_block_style", "style"), &MarkdownTheme::set_code_block_style);
	ClassDB::bind_method(D_METHOD("get_code_block_style"), &MarkdownTheme::get_code_block_style);

	ClassDB::bind_method(D_METHOD("set_inline_code_style", "style"), &MarkdownTheme::set_inline_code_style);
	ClassDB::bind_method(D_METHOD("get_inline_code_style"), &MarkdownTheme::get_inline_code_style);

	ClassDB::bind_method(D_METHOD("set_blockquote_style", "style"), &MarkdownTheme::set_blockquote_style);
	ClassDB::bind_method(D_METHOD("get_blockquote_style"), &MarkdownTheme::get_blockquote_style);

	ClassDB::bind_method(D_METHOD("set_table_style", "style"), &MarkdownTheme::set_table_style);
	ClassDB::bind_method(D_METHOD("get_table_style"), &MarkdownTheme::get_table_style);

	ClassDB::bind_method(D_METHOD("set_table_header_style", "style"), &MarkdownTheme::set_table_header_style);
	ClassDB::bind_method(D_METHOD("get_table_header_style"), &MarkdownTheme::get_table_header_style);

	// Metrics
	ClassDB::bind_method(D_METHOD("set_inline_code_padding_horizontal", "padding"), &MarkdownTheme::set_inline_code_padding_horizontal);
	ClassDB::bind_method(D_METHOD("get_inline_code_padding_horizontal"), &MarkdownTheme::get_inline_code_padding_horizontal);

	ClassDB::bind_method(D_METHOD("set_inline_code_padding_vertical", "padding"), &MarkdownTheme::set_inline_code_padding_vertical);
	ClassDB::bind_method(D_METHOD("get_inline_code_padding_vertical"), &MarkdownTheme::get_inline_code_padding_vertical);

	ClassDB::bind_method(D_METHOD("set_code_block_padding", "padding"), &MarkdownTheme::set_code_block_padding);
	ClassDB::bind_method(D_METHOD("get_code_block_padding"), &MarkdownTheme::get_code_block_padding);

	ClassDB::bind_method(D_METHOD("set_blockquote_padding", "padding"), &MarkdownTheme::set_blockquote_padding);
	ClassDB::bind_method(D_METHOD("get_blockquote_padding"), &MarkdownTheme::get_blockquote_padding);

	ClassDB::bind_method(D_METHOD("set_blockquote_accent_width", "width"), &MarkdownTheme::set_blockquote_accent_width);
	ClassDB::bind_method(D_METHOD("get_blockquote_accent_width"), &MarkdownTheme::get_blockquote_accent_width);

	ClassDB::bind_method(D_METHOD("set_callout_padding", "padding"), &MarkdownTheme::set_callout_padding);
	ClassDB::bind_method(D_METHOD("get_callout_padding"), &MarkdownTheme::get_callout_padding);

	ClassDB::bind_method(D_METHOD("set_callout_accent_width", "width"), &MarkdownTheme::set_callout_accent_width);
	ClassDB::bind_method(D_METHOD("get_callout_accent_width"), &MarkdownTheme::get_callout_accent_width);

	ClassDB::bind_method(D_METHOD("set_callout_icon_size", "size"), &MarkdownTheme::set_callout_icon_size);
	ClassDB::bind_method(D_METHOD("get_callout_icon_size"), &MarkdownTheme::get_callout_icon_size);

	ClassDB::bind_method(D_METHOD("set_table_cell_padding_horizontal", "padding"), &MarkdownTheme::set_table_cell_padding_horizontal);
	ClassDB::bind_method(D_METHOD("get_table_cell_padding_horizontal"), &MarkdownTheme::get_table_cell_padding_horizontal);

	ClassDB::bind_method(D_METHOD("set_table_cell_padding_vertical", "padding"), &MarkdownTheme::set_table_cell_padding_vertical);
	ClassDB::bind_method(D_METHOD("get_table_cell_padding_vertical"), &MarkdownTheme::get_table_cell_padding_vertical);

	ClassDB::bind_method(D_METHOD("set_rule_thickness", "thickness"), &MarkdownTheme::set_rule_thickness);
	ClassDB::bind_method(D_METHOD("get_rule_thickness"), &MarkdownTheme::get_rule_thickness);

	ClassDB::bind_method(D_METHOD("set_corner_radius", "radius"), &MarkdownTheme::set_corner_radius);
	ClassDB::bind_method(D_METHOD("get_corner_radius"), &MarkdownTheme::get_corner_radius);

	ClassDB::bind_method(D_METHOD("set_max_image_height", "height"), &MarkdownTheme::set_max_image_height);
	ClassDB::bind_method(D_METHOD("get_max_image_height"), &MarkdownTheme::get_max_image_height);

	ClassDB::bind_method(D_METHOD("set_inline_image_scale", "scale"), &MarkdownTheme::set_inline_image_scale);
	ClassDB::bind_method(D_METHOD("get_inline_image_scale"), &MarkdownTheme::get_inline_image_scale);

	// Callouts
	ClassDB::bind_method(D_METHOD("set_callouts", "callouts"), &MarkdownTheme::set_callouts);
	ClassDB::bind_method(D_METHOD("get_callouts"), &MarkdownTheme::get_callouts);
	ClassDB::bind_method(D_METHOD("set_callout_style", "kind", "style"), &MarkdownTheme::set_callout_style);
	ClassDB::bind_method(D_METHOD("get_callout_style", "kind"), &MarkdownTheme::get_callout_style);

	// Presets
	ClassDB::bind_static_method("MarkdownTheme", D_METHOD("create_dark"), &MarkdownTheme::create_dark);
	ClassDB::bind_static_method("MarkdownTheme", D_METHOD("create_light"), &MarkdownTheme::create_light);

	// Inspector property groups & properties
	ADD_GROUP("Fonts", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "regular_font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_regular_font", "get_regular_font");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "bold_font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_bold_font", "get_bold_font");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "italic_font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_italic_font", "get_italic_font");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "bold_italic_font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_bold_italic_font", "get_bold_italic_font");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "monospace_font", PROPERTY_HINT_RESOURCE_TYPE, "Font"), "set_monospace_font", "get_monospace_font");

	ADD_GROUP("Typography", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "base_font_size", PROPERTY_HINT_RANGE, "6,128,1,or_greater"), "set_base_font_size", "get_base_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "code_font_scale", PROPERTY_HINT_RANGE, "0.5,1.5,0.05"), "set_code_font_scale", "get_code_font_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "line_spacing", PROPERTY_HINT_RANGE, "0.0,32.0,0.5,or_greater"), "set_line_spacing", "get_line_spacing");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "paragraph_spacing_em", PROPERTY_HINT_RANGE, "0.0,4.0,0.05,or_greater"), "set_paragraph_spacing_em", "get_paragraph_spacing_em");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "heading_scales"), "set_heading_scales", "get_heading_scales");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "heading_top_spacing_em", PROPERTY_HINT_RANGE, "0.0,4.0,0.05,or_greater"), "set_heading_top_spacing_em", "get_heading_top_spacing_em");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "heading_bottom_spacing_em", PROPERTY_HINT_RANGE, "0.0,4.0,0.05,or_greater"), "set_heading_bottom_spacing_em", "get_heading_bottom_spacing_em");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "list_indent_em", PROPERTY_HINT_RANGE, "0.25,4.0,0.05,or_greater"), "set_list_indent_em", "get_list_indent_em");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "list_item_spacing_em", PROPERTY_HINT_RANGE, "0.0,2.0,0.05,or_greater"), "set_list_item_spacing_em", "get_list_item_spacing_em");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "blockquote_indent_em", PROPERTY_HINT_RANGE, "0.0,4.0,0.05,or_greater"), "set_blockquote_indent_em", "get_blockquote_indent_em");

	ADD_GROUP("Colors", "");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "text_color"), "set_text_color", "get_text_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "heading_color"), "set_heading_color", "get_heading_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "muted_color"), "set_muted_color", "get_muted_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "link_color"), "set_link_color", "get_link_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "link_hover_color"), "set_link_hover_color", "get_link_hover_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "code_text_color"), "set_code_text_color", "get_code_text_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "code_bg_color"), "set_code_bg_color", "get_code_bg_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "inline_code_bg_color"), "set_inline_code_bg_color", "get_inline_code_bg_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "strikethrough_color"), "set_strikethrough_color", "get_strikethrough_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "rule_color"), "set_rule_color", "get_rule_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "blockquote_accent_color"), "set_blockquote_accent_color", "get_blockquote_accent_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "blockquote_bg_color"), "set_blockquote_bg_color", "get_blockquote_bg_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "table_border_color"), "set_table_border_color", "get_table_border_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "table_header_bg_color"), "set_table_header_bg_color", "get_table_header_bg_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "table_alt_row_bg_color"), "set_table_alt_row_bg_color", "get_table_alt_row_bg_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "task_checked_color"), "set_task_checked_color", "get_task_checked_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "task_unchecked_color"), "set_task_unchecked_color", "get_task_unchecked_color");

	ADD_GROUP("StyleBoxes", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "code_block_style", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_code_block_style", "get_code_block_style");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "inline_code_style", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_inline_code_style", "get_inline_code_style");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "blockquote_style", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_blockquote_style", "get_blockquote_style");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "table_style", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_table_style", "get_table_style");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "table_header_style", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_table_header_style", "get_table_header_style");

	ADD_GROUP("Metrics", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inline_code_padding_horizontal", PROPERTY_HINT_RANGE, "0.0,32.0,0.5,or_greater"), "set_inline_code_padding_horizontal", "get_inline_code_padding_horizontal");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inline_code_padding_vertical", PROPERTY_HINT_RANGE, "0.0,16.0,0.5,or_greater"), "set_inline_code_padding_vertical", "get_inline_code_padding_vertical");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "code_block_padding", PROPERTY_HINT_RANGE, "0.0,64.0,0.5,or_greater"), "set_code_block_padding", "get_code_block_padding");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "blockquote_padding", PROPERTY_HINT_RANGE, "0.0,64.0,0.5,or_greater"), "set_blockquote_padding", "get_blockquote_padding");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "blockquote_accent_width", PROPERTY_HINT_RANGE, "0.0,16.0,0.5,or_greater"), "set_blockquote_accent_width", "get_blockquote_accent_width");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "callout_padding", PROPERTY_HINT_RANGE, "0.0,64.0,0.5,or_greater"), "set_callout_padding", "get_callout_padding");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "callout_accent_width", PROPERTY_HINT_RANGE, "0.0,16.0,0.5,or_greater"), "set_callout_accent_width", "get_callout_accent_width");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "callout_icon_size", PROPERTY_HINT_RANGE, "8.0,48.0,1.0,or_greater"), "set_callout_icon_size", "get_callout_icon_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "table_cell_padding_horizontal", PROPERTY_HINT_RANGE, "0.0,48.0,0.5,or_greater"), "set_table_cell_padding_horizontal", "get_table_cell_padding_horizontal");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "table_cell_padding_vertical", PROPERTY_HINT_RANGE, "0.0,32.0,0.5,or_greater"), "set_table_cell_padding_vertical", "get_table_cell_padding_vertical");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rule_thickness", PROPERTY_HINT_RANGE, "0.5,8.0,0.5,or_greater"), "set_rule_thickness", "get_rule_thickness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "corner_radius", PROPERTY_HINT_RANGE, "0.0,24.0,0.5,or_greater"), "set_corner_radius", "get_corner_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_image_height", PROPERTY_HINT_RANGE, "32.0,2048.0,4.0,or_greater"), "set_max_image_height", "get_max_image_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inline_image_scale", PROPERTY_HINT_RANGE, "0.5,3.0,0.05,or_greater"), "set_inline_image_scale", "get_inline_image_scale");

	ADD_GROUP("Callouts", "");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "callouts"), "set_callouts", "get_callouts");
}

// Forwarding nested MarkdownCalloutStyle::changed signals lets Inspector edits
// on an individual callout style automatically re-layout every MarkdownView
// referencing this theme; signals are disconnected in the destructor and before
// dictionary mutations to avoid dangling callbacks.
void MarkdownTheme::_connect_callout_signal(const Ref<MarkdownCalloutStyle> &p_style) {
	if (p_style.is_null()) {
		return;
	}
	Callable callback = callable_mp(this, &MarkdownTheme::_on_callout_style_changed);
	if (!p_style->is_connected("changed", callback)) {
		p_style->connect("changed", callback);
	}
}

void MarkdownTheme::_disconnect_callout_signals() {
	Callable callback = callable_mp(this, &MarkdownTheme::_on_callout_style_changed);
	Array keys = callouts.keys();
	for (int index = 0; index < keys.size(); ++index) {
		Variant entry = callouts[keys[index]];
		if (entry.get_type() != Variant::OBJECT) {
			continue;
		}
		MarkdownCalloutStyle *style = Object::cast_to<MarkdownCalloutStyle>(entry.operator Object *());
		if (style != nullptr && style->is_connected("changed", callback)) {
			style->disconnect("changed", callback);
		}
	}
}

void MarkdownTheme::_on_callout_style_changed() {
	emit_changed();
}

void MarkdownTheme::_populate_default_callouts(bool p_light_mode) {
	_disconnect_callout_signals();
	callouts.clear();

	if (p_light_mode) {
		callouts["NOTE"] = MarkdownCalloutStyle::create("Note", Color(0.09f, 0.44f, 0.84f, 1.0f));
		callouts["TIP"] = MarkdownCalloutStyle::create("Tip", Color(0.11f, 0.55f, 0.30f, 1.0f));
		callouts["IMPORTANT"] = MarkdownCalloutStyle::create("Important", Color(0.51f, 0.31f, 0.85f, 1.0f));
		callouts["WARNING"] = MarkdownCalloutStyle::create("Warning", Color(0.75f, 0.48f, 0.06f, 1.0f));
		callouts["CAUTION"] = MarkdownCalloutStyle::create("Caution", Color(0.82f, 0.19f, 0.22f, 1.0f));
	} else {
		callouts["NOTE"] = MarkdownCalloutStyle::create("Note", Color(0.35f, 0.65f, 1.00f, 1.0f));
		callouts["TIP"] = MarkdownCalloutStyle::create("Tip", Color(0.28f, 0.80f, 0.54f, 1.0f));
		callouts["IMPORTANT"] = MarkdownCalloutStyle::create("Important", Color(0.68f, 0.50f, 0.98f, 1.0f));
		callouts["WARNING"] = MarkdownCalloutStyle::create("Warning", Color(0.95f, 0.72f, 0.28f, 1.0f));
		callouts["CAUTION"] = MarkdownCalloutStyle::create("Caution", Color(0.96f, 0.40f, 0.42f, 1.0f));
	}

	Array keys = callouts.keys();
	for (int index = 0; index < keys.size(); ++index) {
		Ref<MarkdownCalloutStyle> style = Object::cast_to<MarkdownCalloutStyle>(callouts[keys[index]].operator Object *());
		_connect_callout_signal(style);
	}
}

// Fonts

void MarkdownTheme::set_regular_font(const Ref<Font> &p_font) {
	if (regular_font == p_font) {
		return;
	}
	regular_font = p_font;
	emit_changed();
}

Ref<Font> MarkdownTheme::get_regular_font() const {
	return regular_font;
}

void MarkdownTheme::set_bold_font(const Ref<Font> &p_font) {
	if (bold_font == p_font) {
		return;
	}
	bold_font = p_font;
	emit_changed();
}

Ref<Font> MarkdownTheme::get_bold_font() const {
	return bold_font;
}

void MarkdownTheme::set_italic_font(const Ref<Font> &p_font) {
	if (italic_font == p_font) {
		return;
	}
	italic_font = p_font;
	emit_changed();
}

Ref<Font> MarkdownTheme::get_italic_font() const {
	return italic_font;
}

void MarkdownTheme::set_bold_italic_font(const Ref<Font> &p_font) {
	if (bold_italic_font == p_font) {
		return;
	}
	bold_italic_font = p_font;
	emit_changed();
}

Ref<Font> MarkdownTheme::get_bold_italic_font() const {
	return bold_italic_font;
}

void MarkdownTheme::set_monospace_font(const Ref<Font> &p_font) {
	if (monospace_font == p_font) {
		return;
	}
	monospace_font = p_font;
	emit_changed();
}

Ref<Font> MarkdownTheme::get_monospace_font() const {
	return monospace_font;
}

// Typography & rhythm

void MarkdownTheme::set_base_font_size(int p_size) {
	int clamped = std::max(1, p_size);
	if (base_font_size == clamped) {
		return;
	}
	base_font_size = clamped;
	emit_changed();
}

int MarkdownTheme::get_base_font_size() const {
	return base_font_size;
}

void MarkdownTheme::set_code_font_scale(float p_scale) {
	float clamped = std::max(0.1f, p_scale);
	if (float_equals(code_font_scale, clamped)) {
		return;
	}
	code_font_scale = clamped;
	emit_changed();
}

float MarkdownTheme::get_code_font_scale() const {
	return code_font_scale;
}

void MarkdownTheme::set_line_spacing(float p_spacing) {
	float clamped = std::max(0.0f, p_spacing);
	if (float_equals(line_spacing, clamped)) {
		return;
	}
	line_spacing = clamped;
	emit_changed();
}

float MarkdownTheme::get_line_spacing() const {
	return line_spacing;
}

void MarkdownTheme::set_paragraph_spacing_em(float p_spacing) {
	float clamped = std::max(0.0f, p_spacing);
	if (float_equals(paragraph_spacing_em, clamped)) {
		return;
	}
	paragraph_spacing_em = clamped;
	emit_changed();
}

float MarkdownTheme::get_paragraph_spacing_em() const {
	return paragraph_spacing_em;
}

void MarkdownTheme::set_heading_scales(const PackedFloat32Array &p_scales) {
	if (heading_scales == p_scales) {
		return;
	}
	heading_scales = p_scales;
	emit_changed();
}

PackedFloat32Array MarkdownTheme::get_heading_scales() const {
	return heading_scales;
}

float MarkdownTheme::get_heading_scale(int p_level) const {
	int index = std::clamp(p_level, 1, 6) - 1;
	if (index < heading_scales.size() && heading_scales[index] > 0.0f) {
		return heading_scales[index];
	}
	return DEFAULT_HEADING_SCALES[index];
}

void MarkdownTheme::set_heading_top_spacing_em(float p_spacing) {
	float clamped = std::max(0.0f, p_spacing);
	if (float_equals(heading_top_spacing_em, clamped)) {
		return;
	}
	heading_top_spacing_em = clamped;
	emit_changed();
}

float MarkdownTheme::get_heading_top_spacing_em() const {
	return heading_top_spacing_em;
}

void MarkdownTheme::set_heading_bottom_spacing_em(float p_spacing) {
	float clamped = std::max(0.0f, p_spacing);
	if (float_equals(heading_bottom_spacing_em, clamped)) {
		return;
	}
	heading_bottom_spacing_em = clamped;
	emit_changed();
}

float MarkdownTheme::get_heading_bottom_spacing_em() const {
	return heading_bottom_spacing_em;
}

void MarkdownTheme::set_list_indent_em(float p_indent) {
	float clamped = std::max(0.25f, p_indent);
	if (float_equals(list_indent_em, clamped)) {
		return;
	}
	list_indent_em = clamped;
	emit_changed();
}

float MarkdownTheme::get_list_indent_em() const {
	return list_indent_em;
}

void MarkdownTheme::set_list_item_spacing_em(float p_spacing) {
	float clamped = std::max(0.0f, p_spacing);
	if (float_equals(list_item_spacing_em, clamped)) {
		return;
	}
	list_item_spacing_em = clamped;
	emit_changed();
}

float MarkdownTheme::get_list_item_spacing_em() const {
	return list_item_spacing_em;
}

void MarkdownTheme::set_blockquote_indent_em(float p_indent) {
	float clamped = std::max(0.0f, p_indent);
	if (float_equals(blockquote_indent_em, clamped)) {
		return;
	}
	blockquote_indent_em = clamped;
	emit_changed();
}

float MarkdownTheme::get_blockquote_indent_em() const {
	return blockquote_indent_em;
}

// Colors

void MarkdownTheme::set_text_color(const Color &p_color) {
	if (text_color == p_color) {
		return;
	}
	text_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_text_color() const {
	return text_color;
}

void MarkdownTheme::set_heading_color(const Color &p_color) {
	if (heading_color == p_color) {
		return;
	}
	heading_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_heading_color() const {
	return heading_color;
}

void MarkdownTheme::set_muted_color(const Color &p_color) {
	if (muted_color == p_color) {
		return;
	}
	muted_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_muted_color() const {
	return muted_color;
}

void MarkdownTheme::set_link_color(const Color &p_color) {
	if (link_color == p_color) {
		return;
	}
	link_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_link_color() const {
	return link_color;
}

void MarkdownTheme::set_link_hover_color(const Color &p_color) {
	if (link_hover_color == p_color) {
		return;
	}
	link_hover_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_link_hover_color() const {
	return link_hover_color;
}

void MarkdownTheme::set_code_text_color(const Color &p_color) {
	if (code_text_color == p_color) {
		return;
	}
	code_text_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_code_text_color() const {
	return code_text_color;
}

void MarkdownTheme::set_code_bg_color(const Color &p_color) {
	if (code_bg_color == p_color) {
		return;
	}
	code_bg_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_code_bg_color() const {
	return code_bg_color;
}

void MarkdownTheme::set_inline_code_bg_color(const Color &p_color) {
	if (inline_code_bg_color == p_color) {
		return;
	}
	inline_code_bg_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_inline_code_bg_color() const {
	return inline_code_bg_color;
}

void MarkdownTheme::set_strikethrough_color(const Color &p_color) {
	if (strikethrough_color == p_color) {
		return;
	}
	strikethrough_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_strikethrough_color() const {
	return strikethrough_color;
}

void MarkdownTheme::set_rule_color(const Color &p_color) {
	if (rule_color == p_color) {
		return;
	}
	rule_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_rule_color() const {
	return rule_color;
}

void MarkdownTheme::set_blockquote_accent_color(const Color &p_color) {
	if (blockquote_accent_color == p_color) {
		return;
	}
	blockquote_accent_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_blockquote_accent_color() const {
	return blockquote_accent_color;
}

void MarkdownTheme::set_blockquote_bg_color(const Color &p_color) {
	if (blockquote_bg_color == p_color) {
		return;
	}
	blockquote_bg_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_blockquote_bg_color() const {
	return blockquote_bg_color;
}

void MarkdownTheme::set_table_border_color(const Color &p_color) {
	if (table_border_color == p_color) {
		return;
	}
	table_border_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_table_border_color() const {
	return table_border_color;
}

void MarkdownTheme::set_table_header_bg_color(const Color &p_color) {
	if (table_header_bg_color == p_color) {
		return;
	}
	table_header_bg_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_table_header_bg_color() const {
	return table_header_bg_color;
}

void MarkdownTheme::set_table_alt_row_bg_color(const Color &p_color) {
	if (table_alt_row_bg_color == p_color) {
		return;
	}
	table_alt_row_bg_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_table_alt_row_bg_color() const {
	return table_alt_row_bg_color;
}

void MarkdownTheme::set_task_checked_color(const Color &p_color) {
	if (task_checked_color == p_color) {
		return;
	}
	task_checked_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_task_checked_color() const {
	return task_checked_color;
}

void MarkdownTheme::set_task_unchecked_color(const Color &p_color) {
	if (task_unchecked_color == p_color) {
		return;
	}
	task_unchecked_color = p_color;
	emit_changed();
}

Color MarkdownTheme::get_task_unchecked_color() const {
	return task_unchecked_color;
}

// StyleBoxes

void MarkdownTheme::set_code_block_style(const Ref<StyleBox> &p_style) {
	if (code_block_style == p_style) {
		return;
	}
	code_block_style = p_style;
	emit_changed();
}

Ref<StyleBox> MarkdownTheme::get_code_block_style() const {
	return code_block_style;
}

void MarkdownTheme::set_inline_code_style(const Ref<StyleBox> &p_style) {
	if (inline_code_style == p_style) {
		return;
	}
	inline_code_style = p_style;
	emit_changed();
}

Ref<StyleBox> MarkdownTheme::get_inline_code_style() const {
	return inline_code_style;
}

void MarkdownTheme::set_blockquote_style(const Ref<StyleBox> &p_style) {
	if (blockquote_style == p_style) {
		return;
	}
	blockquote_style = p_style;
	emit_changed();
}

Ref<StyleBox> MarkdownTheme::get_blockquote_style() const {
	return blockquote_style;
}

void MarkdownTheme::set_table_style(const Ref<StyleBox> &p_style) {
	if (table_style == p_style) {
		return;
	}
	table_style = p_style;
	emit_changed();
}

Ref<StyleBox> MarkdownTheme::get_table_style() const {
	return table_style;
}

void MarkdownTheme::set_table_header_style(const Ref<StyleBox> &p_style) {
	if (table_header_style == p_style) {
		return;
	}
	table_header_style = p_style;
	emit_changed();
}

Ref<StyleBox> MarkdownTheme::get_table_header_style() const {
	return table_header_style;
}

// Metrics

void MarkdownTheme::set_inline_code_padding_horizontal(float p_padding) {
	float clamped = std::max(0.0f, p_padding);
	if (float_equals(inline_code_padding_horizontal, clamped)) {
		return;
	}
	inline_code_padding_horizontal = clamped;
	emit_changed();
}

float MarkdownTheme::get_inline_code_padding_horizontal() const {
	return inline_code_padding_horizontal;
}

void MarkdownTheme::set_inline_code_padding_vertical(float p_padding) {
	float clamped = std::max(0.0f, p_padding);
	if (float_equals(inline_code_padding_vertical, clamped)) {
		return;
	}
	inline_code_padding_vertical = clamped;
	emit_changed();
}

float MarkdownTheme::get_inline_code_padding_vertical() const {
	return inline_code_padding_vertical;
}

void MarkdownTheme::set_code_block_padding(float p_padding) {
	float clamped = std::max(0.0f, p_padding);
	if (float_equals(code_block_padding, clamped)) {
		return;
	}
	code_block_padding = clamped;
	emit_changed();
}

float MarkdownTheme::get_code_block_padding() const {
	return code_block_padding;
}

void MarkdownTheme::set_blockquote_padding(float p_padding) {
	float clamped = std::max(0.0f, p_padding);
	if (float_equals(blockquote_padding, clamped)) {
		return;
	}
	blockquote_padding = clamped;
	emit_changed();
}

float MarkdownTheme::get_blockquote_padding() const {
	return blockquote_padding;
}

void MarkdownTheme::set_blockquote_accent_width(float p_width) {
	float clamped = std::max(0.0f, p_width);
	if (float_equals(blockquote_accent_width, clamped)) {
		return;
	}
	blockquote_accent_width = clamped;
	emit_changed();
}

float MarkdownTheme::get_blockquote_accent_width() const {
	return blockquote_accent_width;
}

void MarkdownTheme::set_callout_padding(float p_padding) {
	float clamped = std::max(0.0f, p_padding);
	if (float_equals(callout_padding, clamped)) {
		return;
	}
	callout_padding = clamped;
	emit_changed();
}

float MarkdownTheme::get_callout_padding() const {
	return callout_padding;
}

void MarkdownTheme::set_callout_accent_width(float p_width) {
	float clamped = std::max(0.0f, p_width);
	if (float_equals(callout_accent_width, clamped)) {
		return;
	}
	callout_accent_width = clamped;
	emit_changed();
}

float MarkdownTheme::get_callout_accent_width() const {
	return callout_accent_width;
}

void MarkdownTheme::set_callout_icon_size(float p_size) {
	float clamped = std::max(4.0f, p_size);
	if (float_equals(callout_icon_size, clamped)) {
		return;
	}
	callout_icon_size = clamped;
	emit_changed();
}

float MarkdownTheme::get_callout_icon_size() const {
	return callout_icon_size;
}

void MarkdownTheme::set_table_cell_padding_horizontal(float p_padding) {
	float clamped = std::max(0.0f, p_padding);
	if (float_equals(table_cell_padding_horizontal, clamped)) {
		return;
	}
	table_cell_padding_horizontal = clamped;
	emit_changed();
}

float MarkdownTheme::get_table_cell_padding_horizontal() const {
	return table_cell_padding_horizontal;
}

void MarkdownTheme::set_table_cell_padding_vertical(float p_padding) {
	float clamped = std::max(0.0f, p_padding);
	if (float_equals(table_cell_padding_vertical, clamped)) {
		return;
	}
	table_cell_padding_vertical = clamped;
	emit_changed();
}

float MarkdownTheme::get_table_cell_padding_vertical() const {
	return table_cell_padding_vertical;
}

void MarkdownTheme::set_rule_thickness(float p_thickness) {
	float clamped = std::max(0.5f, p_thickness);
	if (float_equals(rule_thickness, clamped)) {
		return;
	}
	rule_thickness = clamped;
	emit_changed();
}

float MarkdownTheme::get_rule_thickness() const {
	return rule_thickness;
}

void MarkdownTheme::set_corner_radius(float p_radius) {
	float clamped = std::max(0.0f, p_radius);
	if (float_equals(corner_radius, clamped)) {
		return;
	}
	corner_radius = clamped;
	emit_changed();
}

float MarkdownTheme::get_corner_radius() const {
	return corner_radius;
}

void MarkdownTheme::set_max_image_height(float p_height) {
	float clamped = std::max(1.0f, p_height);
	if (float_equals(max_image_height, clamped)) {
		return;
	}
	max_image_height = clamped;
	emit_changed();
}

float MarkdownTheme::get_max_image_height() const {
	return max_image_height;
}

void MarkdownTheme::set_inline_image_scale(float p_scale) {
	float clamped = std::max(0.1f, p_scale);
	if (float_equals(inline_image_scale, clamped)) {
		return;
	}
	inline_image_scale = clamped;
	emit_changed();
}

float MarkdownTheme::get_inline_image_scale() const {
	return inline_image_scale;
}

// Callouts

void MarkdownTheme::set_callouts(const Dictionary &p_callouts) {
	_disconnect_callout_signals();
	callouts.clear();

	Array keys = p_callouts.keys();
	for (int index = 0; index < keys.size(); ++index) {
		String normalized_kind = String(keys[index]).strip_edges().to_upper();
		if (normalized_kind.is_empty()) {
			continue;
		}
		Variant value = p_callouts[keys[index]];
		if (value.get_type() != Variant::OBJECT) {
			continue;
		}
		MarkdownCalloutStyle *style_pointer = Object::cast_to<MarkdownCalloutStyle>(value.operator Object *());
		if (style_pointer == nullptr) {
			continue;
		}
		Ref<MarkdownCalloutStyle> style(style_pointer);
		callouts[normalized_kind] = style;
		_connect_callout_signal(style);
	}

	emit_changed();
}

Dictionary MarkdownTheme::get_callouts() const {
	return callouts;
}

void MarkdownTheme::set_callout_style(const String &p_kind, const Ref<MarkdownCalloutStyle> &p_style) {
	String normalized_kind = p_kind.strip_edges().to_upper();
	if (normalized_kind.is_empty()) {
		return;
	}

	if (callouts.has(normalized_kind)) {
		Variant existing = callouts[normalized_kind];
		Ref<MarkdownCalloutStyle> existing_style;
		if (existing.get_type() == Variant::OBJECT) {
			existing_style = Ref<MarkdownCalloutStyle>(Object::cast_to<MarkdownCalloutStyle>(existing.operator Object *()));
		}
		if (existing_style == p_style) {
			return;
		}
	} else if (p_style.is_null()) {
		return;
	}

	// Rebuild signal connections across the full dictionary so shared
	// MarkdownCalloutStyle instances referenced under multiple keys stay connected
	// when one alias is overwritten or erased.
	_disconnect_callout_signals();
	if (p_style.is_null()) {
		callouts.erase(normalized_kind);
	} else {
		callouts[normalized_kind] = p_style;
	}

	Array keys = callouts.keys();
	for (int index = 0; index < keys.size(); ++index) {
		Variant entry = callouts[keys[index]];
		if (entry.get_type() == Variant::OBJECT) {
			Ref<MarkdownCalloutStyle> style(Object::cast_to<MarkdownCalloutStyle>(entry.operator Object *()));
			_connect_callout_signal(style);
		}
	}

	emit_changed();
}

Ref<MarkdownCalloutStyle> MarkdownTheme::get_callout_style(const String &p_kind) const {
	String normalized_kind = p_kind.strip_edges().to_upper();
	if (callouts.has(normalized_kind)) {
		Variant entry = callouts[normalized_kind];
		if (entry.get_type() == Variant::OBJECT) {
			MarkdownCalloutStyle *style_pointer = Object::cast_to<MarkdownCalloutStyle>(entry.operator Object *());
			if (style_pointer != nullptr) {
				return Ref<MarkdownCalloutStyle>(style_pointer);
			}
		}
	}

	Color fallback_accent = blockquote_accent_color.a > 0.0f ? blockquote_accent_color : muted_color;
	return MarkdownCalloutStyle::create(format_callout_fallback_title(normalized_kind), fallback_accent);
}

// Presets

Ref<MarkdownTheme> MarkdownTheme::create_dark() {
	Ref<MarkdownTheme> theme;
	theme.instantiate();
	return theme;
}

Ref<MarkdownTheme> MarkdownTheme::create_light() {
	Ref<MarkdownTheme> theme;
	theme.instantiate();

	theme->text_color = Color(0.14f, 0.16f, 0.20f, 1.0f);
	theme->heading_color = Color(0.08f, 0.10f, 0.14f, 1.0f);
	theme->muted_color = Color(0.42f, 0.47f, 0.54f, 1.0f);
	theme->link_color = Color(0.08f, 0.42f, 0.84f, 1.0f);
	theme->link_hover_color = Color(0.04f, 0.29f, 0.66f, 1.0f);
	theme->code_text_color = Color(0.54f, 0.20f, 0.25f, 1.0f);
	theme->code_bg_color = Color(0.95f, 0.96f, 0.98f, 1.0f);
	theme->inline_code_bg_color = Color(0.90f, 0.92f, 0.96f, 1.0f);
	theme->strikethrough_color = Color(0.45f, 0.50f, 0.57f, 0.85f);
	theme->rule_color = Color(0.82f, 0.85f, 0.89f, 1.0f);
	theme->blockquote_accent_color = Color(0.65f, 0.70f, 0.78f, 1.0f);
	theme->blockquote_bg_color = Color(0.95f, 0.96f, 0.98f, 0.85f);
	theme->table_border_color = Color(0.81f, 0.84f, 0.89f, 1.0f);
	theme->table_header_bg_color = Color(0.93f, 0.95f, 0.97f, 1.0f);
	theme->table_alt_row_bg_color = Color(0.97f, 0.98f, 0.99f, 0.85f);
	theme->task_checked_color = Color(0.12f, 0.60f, 0.34f, 1.0f);
	theme->task_unchecked_color = Color(0.55f, 0.60f, 0.68f, 1.0f);

	theme->_populate_default_callouts(true);
	return theme;
}

} // namespace godot
