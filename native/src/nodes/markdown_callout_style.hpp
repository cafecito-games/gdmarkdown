// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

// Resource describing the title, colors, optional icon, and optional panel
// StyleBox for a single Markdown callout kind (e.g. "NOTE", "TIP", or custom).
// Mutating any property emits Resource::changed so owning MarkdownThemes and
// MarkdownViews rebuild automatically.
class MarkdownCalloutStyle : public Resource {
	GDCLASS(MarkdownCalloutStyle, Resource)

private:
	String title;
	Color accent_color = Color(0.35f, 0.62f, 0.98f, 1.0f);
	// Alpha 0.0 signals "inherit accent_color" via resolve_title_color().
	Color title_color = Color(0.0f, 0.0f, 0.0f, 0.0f);
	Ref<Texture2D> icon;
	Ref<StyleBox> panel_style;

protected:
	static void _bind_methods();

public:
	MarkdownCalloutStyle() = default;
	~MarkdownCalloutStyle() override = default;

	void set_title(const String &p_title);
	String get_title() const;

	void set_accent_color(const Color &p_color);
	Color get_accent_color() const;

	void set_title_color(const Color &p_color);
	Color get_title_color() const;

	void set_icon(const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_icon() const;

	void set_panel_style(const Ref<StyleBox> &p_style);
	Ref<StyleBox> get_panel_style() const;

	// Returns title_color when its alpha > 0, otherwise falls back to accent_color.
	Color resolve_title_color() const;

	// Factory helper for constructing a fully populated style from GDScript or C++.
	static Ref<MarkdownCalloutStyle> create(
			const String &p_title,
			const Color &p_accent_color,
			const Color &p_title_color = Color(0.0f, 0.0f, 0.0f, 0.0f),
			const Ref<Texture2D> &p_icon = Ref<Texture2D>(),
			const Ref<StyleBox> &p_panel_style = Ref<StyleBox>());
};

} // namespace godot
