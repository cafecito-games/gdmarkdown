// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#include "nodes/markdown_callout_style.hpp"

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void MarkdownCalloutStyle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title", "title"), &MarkdownCalloutStyle::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &MarkdownCalloutStyle::get_title);

	ClassDB::bind_method(D_METHOD("set_accent_color", "color"), &MarkdownCalloutStyle::set_accent_color);
	ClassDB::bind_method(D_METHOD("get_accent_color"), &MarkdownCalloutStyle::get_accent_color);

	ClassDB::bind_method(D_METHOD("set_title_color", "color"), &MarkdownCalloutStyle::set_title_color);
	ClassDB::bind_method(D_METHOD("get_title_color"), &MarkdownCalloutStyle::get_title_color);

	ClassDB::bind_method(D_METHOD("set_icon", "icon"), &MarkdownCalloutStyle::set_icon);
	ClassDB::bind_method(D_METHOD("get_icon"), &MarkdownCalloutStyle::get_icon);

	ClassDB::bind_method(D_METHOD("set_panel_style", "style"), &MarkdownCalloutStyle::set_panel_style);
	ClassDB::bind_method(D_METHOD("get_panel_style"), &MarkdownCalloutStyle::get_panel_style);

	ClassDB::bind_method(D_METHOD("resolve_title_color"), &MarkdownCalloutStyle::resolve_title_color);

	ClassDB::bind_static_method(
			"MarkdownCalloutStyle",
			D_METHOD("create", "title", "accent_color", "title_color", "icon", "panel_style"),
			&MarkdownCalloutStyle::create,
			DEFVAL(Color(0.0f, 0.0f, 0.0f, 0.0f)),
			DEFVAL(Ref<Texture2D>()),
			DEFVAL(Ref<StyleBox>()));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "accent_color"), "set_accent_color", "get_accent_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "title_color"), "set_title_color", "get_title_color");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_icon", "get_icon");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "panel_style", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_panel_style", "get_panel_style");
}

void MarkdownCalloutStyle::set_title(const String &p_title) {
	if (title == p_title) {
		return;
	}
	title = p_title;
	emit_changed();
}

String MarkdownCalloutStyle::get_title() const {
	return title;
}

void MarkdownCalloutStyle::set_accent_color(const Color &p_color) {
	if (accent_color == p_color) {
		return;
	}
	accent_color = p_color;
	emit_changed();
}

Color MarkdownCalloutStyle::get_accent_color() const {
	return accent_color;
}

void MarkdownCalloutStyle::set_title_color(const Color &p_color) {
	if (title_color == p_color) {
		return;
	}
	title_color = p_color;
	emit_changed();
}

Color MarkdownCalloutStyle::get_title_color() const {
	return title_color;
}

void MarkdownCalloutStyle::set_icon(const Ref<Texture2D> &p_icon) {
	if (icon == p_icon) {
		return;
	}
	icon = p_icon;
	emit_changed();
}

Ref<Texture2D> MarkdownCalloutStyle::get_icon() const {
	return icon;
}

void MarkdownCalloutStyle::set_panel_style(const Ref<StyleBox> &p_style) {
	if (panel_style == p_style) {
		return;
	}
	panel_style = p_style;
	emit_changed();
}

Ref<StyleBox> MarkdownCalloutStyle::get_panel_style() const {
	return panel_style;
}

Color MarkdownCalloutStyle::resolve_title_color() const {
	// Treat a fully transparent title_color as "unset" so Inspector defaults
	// automatically track accent_color without requiring a separate boolean flag.
	return title_color.a > 0.0f ? title_color : accent_color;
}

Ref<MarkdownCalloutStyle> MarkdownCalloutStyle::create(
		const String &p_title,
		const Color &p_accent_color,
		const Color &p_title_color,
		const Ref<Texture2D> &p_icon,
		const Ref<StyleBox> &p_panel_style) {
	Ref<MarkdownCalloutStyle> style;
	style.instantiate();
	style->title = p_title;
	style->accent_color = p_accent_color;
	style->title_color = p_title_color;
	style->icon = p_icon;
	style->panel_style = p_panel_style;
	return style;
}

} // namespace godot
