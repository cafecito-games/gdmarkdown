// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#pragma once

#include "layout/layout_engine.hpp"
#include "nodes/markdown_theme.hpp"
#include "parser/document.hpp"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <vector>

namespace godot {

// Native Godot Control that parses CommonMark + GitHub Flavored Markdown +
// callouts and draws them directly via TextServer and RenderingServer.
// Reports laid-out height through _get_minimum_size() for container auto-sizing,
// supports interactive links/task checkboxes, horizontally scrollable code
// blocks, typewriter reveals, custom image resolution, and ScrollContainer
// vertical culling.
class MarkdownView : public Control {
	GDCLASS(MarkdownView, Control)

private:
	String markdown;
	Ref<MarkdownTheme> markdown_theme;
	Ref<MarkdownTheme> fallback_theme;
	bool interactive_tasks = false;
	bool code_block_wrap = true;
	int visible_characters = -1;
	float visible_ratio = 1.0f;
	Callable image_resolver;

	gdmarkdown::Document document;
	std::vector<bool> task_states;
	gdmarkdown::LayoutDocument layout;
	bool layout_dirty = true;
	float last_layout_width = -1.0f;
	float last_drawn_band_min_y = 0.0f;
	float last_drawn_band_max_y = 0.0f;
	bool last_draw_was_culled = false;
	ObjectID bound_scroll_ancestor_id;

	String hovered_link_url;
	int pressed_task_index = -1;
	String pressed_link_url;
	int pressed_code_scroll_index = -1;
	bool dragging_code = false;
	Vector2 press_position;
	float drag_start_scroll_x = 0.0f;

	HashMap<String, Ref<Texture2D>> image_cache;

	Ref<MarkdownTheme> _active_theme();
	Ref<Texture2D> _resolve_image(const String &p_uri);
	void _free_clip_canvas_items();
	void _sync_clip_canvas_items();
	void _mark_layout_dirty();
	void _ensure_layout();
	void _rebuild_document();
	void _on_theme_changed();
	void _set_hovered_link(const String &p_url);
	int _find_task_at(const Vector2 &p_position) const;
	String _find_link_at(const Vector2 &p_position) const;
	int _find_code_scroll_at(const Vector2 &p_position) const;
	void _clear_pointer_press();
	void _update_cursor_for_position(const Vector2 &p_position);
	ScrollContainer *_find_scroll_ancestor() const;
	void _bind_scroll_ancestor();
	void _unbind_scroll_ancestor();
	bool _compute_visible_y_band(float &r_min_y, float &r_max_y) const;
	void _redraw_if_visible_band_moved();
	void _draw_document();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	MarkdownView();
	~MarkdownView() override;

	Vector2 _get_minimum_size() const override;
	void _gui_input(const Ref<InputEvent> &p_event) override;

	// Bound properties
	void set_markdown(const String &p_markdown);
	String get_markdown() const;

	void set_markdown_theme(const Ref<MarkdownTheme> &p_theme);
	Ref<MarkdownTheme> get_markdown_theme() const;

	void set_interactive_tasks(bool p_enabled);
	bool is_interactive_tasks() const;

	void set_code_block_wrap(bool p_wrap);
	bool is_code_block_wrap() const;

	void set_visible_characters(int p_count);
	int get_visible_characters() const;

	void set_visible_ratio(float p_ratio);
	float get_visible_ratio() const;

	void set_image_resolver(const Callable &p_callable);
	Callable get_image_resolver() const;

	// Document inspection & state helpers
	// Ensures layout is up to date and returns total countable characters for typewriter reveal.
	int get_total_character_count();
	int get_task_count() const;
	bool is_task_checked(int p_task_index) const;
	// Updates task state and redraws without emitting task_toggled.
	void set_task_checked(int p_task_index, bool p_checked);
	// Ensures layout is up to date and returns formatting-free plain text.
	String get_plain_text();
	// Drops all cached image resolutions (including misses) and rebuilds layout.
	void clear_image_cache();
};

} // namespace godot
