// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#include "nodes/markdown_view.hpp"

#include "parser/markdown_parser.hpp"

#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_pan_gesture.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/transform2d.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace godot {

MarkdownView::MarkdownView() {
	// Use MOUSE_FILTER_PASS so ancestor ScrollContainers still receive wheel and
	// drag events unless _gui_input() explicitly accepts a link/task/code interaction.
	set_mouse_filter(MOUSE_FILTER_PASS);
}

MarkdownView::~MarkdownView() {
	if (markdown_theme.is_valid()) {
		Callable callback = callable_mp(this, &MarkdownView::_on_theme_changed);
		if (markdown_theme->is_connected("changed", callback)) {
			markdown_theme->disconnect("changed", callback);
		}
	}
	_free_clip_canvas_items();
}

void MarkdownView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_markdown", "markdown"), &MarkdownView::set_markdown);
	ClassDB::bind_method(D_METHOD("get_markdown"), &MarkdownView::get_markdown);

	ClassDB::bind_method(D_METHOD("set_markdown_theme", "theme"), &MarkdownView::set_markdown_theme);
	ClassDB::bind_method(D_METHOD("get_markdown_theme"), &MarkdownView::get_markdown_theme);

	ClassDB::bind_method(D_METHOD("set_interactive_tasks", "enabled"), &MarkdownView::set_interactive_tasks);
	ClassDB::bind_method(D_METHOD("is_interactive_tasks"), &MarkdownView::is_interactive_tasks);

	ClassDB::bind_method(D_METHOD("set_code_block_wrap", "wrap"), &MarkdownView::set_code_block_wrap);
	ClassDB::bind_method(D_METHOD("is_code_block_wrap"), &MarkdownView::is_code_block_wrap);

	ClassDB::bind_method(D_METHOD("set_visible_characters", "count"), &MarkdownView::set_visible_characters);
	ClassDB::bind_method(D_METHOD("get_visible_characters"), &MarkdownView::get_visible_characters);

	ClassDB::bind_method(D_METHOD("set_visible_ratio", "ratio"), &MarkdownView::set_visible_ratio);
	ClassDB::bind_method(D_METHOD("get_visible_ratio"), &MarkdownView::get_visible_ratio);

	ClassDB::bind_method(D_METHOD("set_image_resolver", "callable"), &MarkdownView::set_image_resolver);
	ClassDB::bind_method(D_METHOD("get_image_resolver"), &MarkdownView::get_image_resolver);

	ClassDB::bind_method(D_METHOD("get_total_character_count"), &MarkdownView::get_total_character_count);
	ClassDB::bind_method(D_METHOD("get_task_count"), &MarkdownView::get_task_count);
	ClassDB::bind_method(D_METHOD("is_task_checked", "task_index"), &MarkdownView::is_task_checked);
	ClassDB::bind_method(D_METHOD("set_task_checked", "task_index", "checked"), &MarkdownView::set_task_checked);
	ClassDB::bind_method(D_METHOD("get_plain_text"), &MarkdownView::get_plain_text);
	ClassDB::bind_method(D_METHOD("clear_image_cache"), &MarkdownView::clear_image_cache);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "markdown", PROPERTY_HINT_MULTILINE_TEXT), "set_markdown", "get_markdown");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "markdown_theme", PROPERTY_HINT_RESOURCE_TYPE, "MarkdownTheme"), "set_markdown_theme", "get_markdown_theme");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interactive_tasks"), "set_interactive_tasks", "is_interactive_tasks");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "code_block_wrap"), "set_code_block_wrap", "is_code_block_wrap");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visible_characters", PROPERTY_HINT_RANGE, "-1,1000000,1,or_greater"), "set_visible_characters", "get_visible_characters");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visible_ratio", PROPERTY_HINT_RANGE, "0.0,1.0,0.001"), "set_visible_ratio", "get_visible_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "image_resolver"), "set_image_resolver", "get_image_resolver");

	ADD_SIGNAL(MethodInfo("link_clicked", PropertyInfo(Variant::STRING, "url")));
	ADD_SIGNAL(MethodInfo("link_hovered", PropertyInfo(Variant::STRING, "url")));
	ADD_SIGNAL(MethodInfo("task_toggled", PropertyInfo(Variant::INT, "task_index"), PropertyInfo(Variant::BOOL, "checked")));
	ADD_SIGNAL(MethodInfo("layout_updated"));
}

Ref<MarkdownTheme> MarkdownView::_active_theme() {
	if (markdown_theme.is_valid()) {
		return markdown_theme;
	}
	if (fallback_theme.is_null()) {
		fallback_theme = MarkdownTheme::create_dark();
	}
	return fallback_theme;
}

// Cache misses (null Refs) are stored alongside hits so unresolved URIs do not
// re-invoke image_resolver or hit ResourceLoader on every width resize.
Ref<Texture2D> MarkdownView::_resolve_image(const String &p_uri) {
	if (p_uri.is_empty()) {
		return Ref<Texture2D>();
	}
	if (image_cache.has(p_uri)) {
		return image_cache[p_uri];
	}

	Ref<Texture2D> resolved_texture;
	if (image_resolver.is_valid()) {
		Variant resolved_variant = image_resolver.call(p_uri);
		if (resolved_variant.get_type() == Variant::OBJECT) {
			Texture2D *texture_pointer = Object::cast_to<Texture2D>(resolved_variant.operator Object *());
			if (texture_pointer != nullptr) {
				resolved_texture = Ref<Texture2D>(texture_pointer);
			}
		}
	}

	if (resolved_texture.is_null() && p_uri.begins_with("res://")) {
		ResourceLoader *resource_loader = ResourceLoader::get_singleton();
		if (resource_loader != nullptr && resource_loader->exists(p_uri)) {
			Ref<Resource> loaded_resource = resource_loader->load(p_uri);
			if (loaded_resource.is_valid()) {
				Texture2D *texture_pointer = Object::cast_to<Texture2D>(loaded_resource.ptr());
				if (texture_pointer != nullptr) {
					resolved_texture = Ref<Texture2D>(texture_pointer);
				}
			}
		}
	}

	image_cache.insert(p_uri, resolved_texture);
	return resolved_texture;
}

// Frees child RenderingServer clip items owned by horizontally scrollable code
// blocks. Called before every relayout, on NOTIFICATION_EXIT_TREE, and in the
// destructor so canvas item RIDs are never leaked.
void MarkdownView::_free_clip_canvas_items() {
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	for (gdmarkdown::CodeScrollRegion &region : layout.code_scroll_regions) {
		if (region.clip_canvas_item.is_valid()) {
			if (rendering_server != nullptr) {
				rendering_server->free_rid(region.clip_canvas_item);
			}
			region.clip_canvas_item = RID();
		}
	}
}

void MarkdownView::_sync_clip_canvas_items() {
	if (!is_inside_tree()) {
		return;
	}
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	RID parent_canvas_item = get_canvas_item();
	if (rendering_server == nullptr || !parent_canvas_item.is_valid()) {
		return;
	}

	for (gdmarkdown::CodeScrollRegion &region : layout.code_scroll_regions) {
		if (!region.clip_canvas_item.is_valid()) {
			region.clip_canvas_item = rendering_server->canvas_item_create();
			rendering_server->canvas_item_set_parent(region.clip_canvas_item, parent_canvas_item);
		}
		rendering_server->canvas_item_set_custom_rect(
				region.clip_canvas_item,
				true,
				Rect2(Vector2(0.0f, 0.0f), region.inner_rectangle.size));
		rendering_server->canvas_item_set_clip(region.clip_canvas_item, true);
		rendering_server->canvas_item_set_transform(
				region.clip_canvas_item,
				Transform2D(0.0f, region.inner_rectangle.position));
	}
}

void MarkdownView::_mark_layout_dirty() {
	layout_dirty = true;
	_ensure_layout();
}

void MarkdownView::_ensure_layout() {
	// Before the first container pass gives the Control a non-zero width, fall
	// back to custom_minimum_size.x (or 400 px) so callers querying plain text,
	// character count, or minimum height immediately after set_markdown() get
	// valid geometry.
	float target_width = get_size().x;
	if (target_width <= 0.5f) {
		Vector2 custom_minimum = get_custom_minimum_size();
		target_width = custom_minimum.x > 0.5f ? custom_minimum.x : 400.0f;
	}

	// Ignore sub-pixel width jitter (<= 0.5 px) from fractional DPI scaling.
	if (!layout_dirty && std::abs(target_width - last_layout_width) <= 0.5f) {
		return;
	}

	_free_clip_canvas_items();
	dragging_code_scroll_index = -1;

	Ref<Font> fallback_font = get_theme_default_font();
	if (fallback_font.is_null()) {
		ThemeDB *theme_database = ThemeDB::get_singleton();
		if (theme_database != nullptr) {
			fallback_font = theme_database->get_fallback_font();
		}
	}

	gdmarkdown::LayoutOptions options;
	options.available_width = target_width;
	options.code_block_wrap = code_block_wrap;
	options.fallback_font = fallback_font;
	options.theme = _active_theme();
	options.resolve_image = [this](const String &uri) {
		return _resolve_image(uri);
	};

	layout = gdmarkdown::build_layout(document, options);
	last_layout_width = target_width;
	layout_dirty = false;

	if (visible_characters >= 0) {
		if (layout.total_character_count > 0) {
			visible_ratio = std::clamp(
					static_cast<float>(visible_characters) / static_cast<float>(layout.total_character_count),
					0.0f,
					1.0f);
		} else {
			visible_ratio = visible_characters == 0 ? 0.0f : 1.0f;
		}
	} else {
		visible_ratio = 1.0f;
	}

	_sync_clip_canvas_items();
	update_minimum_size();
	queue_redraw();
	emit_signal("layout_updated");
}

void MarkdownView::_rebuild_document() {
	CharString utf8_source = markdown.utf8();
	std::string_view source_view(utf8_source.get_data(), static_cast<size_t>(utf8_source.length()));
	document = gdmarkdown::parse_markdown(source_view);
	task_states = document.initial_task_states;
	_set_hovered_link(String());
	dragging_code_scroll_index = -1;
	_mark_layout_dirty();
}

void MarkdownView::_on_theme_changed() {
	_mark_layout_dirty();
}

void MarkdownView::_set_hovered_link(const String &p_url) {
	if (hovered_link_url == p_url) {
		return;
	}
	hovered_link_url = p_url;
	queue_redraw();
	emit_signal("link_hovered", hovered_link_url);
}

int MarkdownView::_find_task_at(const Vector2 &p_position) const {
	for (const gdmarkdown::TaskHitBox &hit_box : layout.task_hit_boxes) {
		if (hit_box.rectangle.has_point(p_position)) {
			return hit_box.task_index;
		}
	}
	return -1;
}

String MarkdownView::_find_link_at(const Vector2 &p_position) const {
	for (const gdmarkdown::LinkHitBox &hit_box : layout.link_hit_boxes) {
		if (hit_box.rectangle.has_point(p_position)) {
			return hit_box.url;
		}
	}
	return String();
}

int MarkdownView::_find_code_scroll_at(const Vector2 &p_position) const {
	for (size_t index = 0; index < layout.code_scroll_regions.size(); ++index) {
		if (layout.code_scroll_regions[index].outer_rectangle.has_point(p_position)) {
			return static_cast<int>(index);
		}
	}
	return -1;
}

void MarkdownView::_update_cursor_for_position(const Vector2 &p_position) {
	bool pointer_target = !_find_link_at(p_position).is_empty();
	if (!pointer_target && interactive_tasks && _find_task_at(p_position) >= 0) {
		pointer_target = true;
	}
	set_default_cursor_shape(pointer_target ? CURSOR_POINTING_HAND : CURSOR_ARROW);
}

// When nested inside a ScrollContainer, computes the visible vertical band in
// local coordinates plus a 64 px overscan margin so _draw_document() can skip
// off-screen primitives without popping during fast scrolls.
bool MarkdownView::_compute_visible_y_band(float &r_min_y, float &r_max_y) const {
	Node *ancestor = get_parent();
	while (ancestor != nullptr) {
		ScrollContainer *scroll_container = Object::cast_to<ScrollContainer>(ancestor);
		if (scroll_container != nullptr) {
			Vector2 viewport_size = scroll_container->get_size();
			if (viewport_size.y <= 0.0f) {
				return false;
			}
			float local_top_y = scroll_container->get_global_position().y - get_global_position().y;
			r_min_y = local_top_y - 64.0f;
			r_max_y = local_top_y + viewport_size.y + 64.0f;
			return true;
		}
		ancestor = ancestor->get_parent();
	}
	return false;
}

void MarkdownView::_draw_document() {
	RID canvas_item = get_canvas_item();
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	if (!canvas_item.is_valid() || rendering_server == nullptr) {
		return;
	}

	// Clear and hide every scrollable code block's child clip canvas item before
	// redrawing so culled or typewriter-hidden code blocks do not leave stale
	// clipped glyphs visible on screen.
	_sync_clip_canvas_items();
	for (const gdmarkdown::CodeScrollRegion &region : layout.code_scroll_regions) {
		if (region.clip_canvas_item.is_valid()) {
			rendering_server->canvas_item_clear(region.clip_canvas_item);
			rendering_server->canvas_item_set_visible(region.clip_canvas_item, false);
		}
	}

	bool typewriter_active = visible_characters >= 0 && (visible_characters < layout.total_character_count || visible_characters == 0);
	if (typewriter_active && visible_characters <= 0) {
		return;
	}

	float visible_min_y = 0.0f;
	float visible_max_y = 0.0f;
	bool cull_vertical = _compute_visible_y_band(visible_min_y, visible_max_y);

	Ref<MarkdownTheme> theme = _active_theme();

	for (const gdmarkdown::PaintPrimitive &primitive : layout.primitives) {
		if (typewriter_active && primitive.document_char_threshold >= visible_characters) {
			continue;
		}
		if (cull_vertical) {
			float top_y = primitive.rectangle.position.y;
			float bottom_y = top_y + primitive.rectangle.size.y;
			if (bottom_y < visible_min_y || top_y > visible_max_y) {
				continue;
			}
		}

		switch (primitive.kind) {
			case gdmarkdown::PaintPrimitiveKind::ROUNDED_PANEL: {
				gdmarkdown::draw_rounded_panel(
						canvas_item,
						primitive.rectangle,
						primitive.fill_color,
						primitive.border_color,
						primitive.border_width,
						primitive.corner_radius);
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::STYLE_BOX: {
				if (primitive.style_box.is_valid()) {
					primitive.style_box->draw(canvas_item, primitive.rectangle);
				}
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::ACCENT_BAR: {
				gdmarkdown::draw_rounded_panel(
						canvas_item,
						primitive.rectangle,
						primitive.fill_color,
						Color(0.0f, 0.0f, 0.0f, 0.0f),
						0.0f,
						primitive.corner_radius);
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::HORIZONTAL_RULE: {
				rendering_server->canvas_item_add_rect(canvas_item, primitive.rectangle, primitive.fill_color);
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::TABLE_GRID: {
				if (primitive.style_box.is_valid()) {
					primitive.style_box->draw(canvas_item, primitive.rectangle);
				}

				const Rect2 &table_rect = primitive.rectangle;
				const std::vector<float> &row_offsets = primitive.table_row_offsets;
				const std::vector<float> &column_offsets = primitive.table_column_offsets;
				int total_rows = row_offsets.size() > 1 ? static_cast<int>(row_offsets.size() - 1) : 0;

				// Header background
				if (primitive.table_header_row_count > 0 && static_cast<int>(row_offsets.size()) > primitive.table_header_row_count) {
					float header_height = row_offsets[primitive.table_header_row_count];
					Rect2 header_rect(
							table_rect.position.x + 1.0f,
							table_rect.position.y + 1.0f,
							std::max(0.0f, table_rect.size.x - 2.0f),
							std::max(0.0f, header_height - 1.0f));
					if (theme->get_table_header_style().is_valid()) {
						theme->get_table_header_style()->draw(canvas_item, header_rect);
					} else if (primitive.table_header_bg.a > 0.0f) {
						rendering_server->canvas_item_add_rect(canvas_item, header_rect, primitive.table_header_bg);
					}
				}

				// Alternating body row background
				if (primitive.table_alt_row_bg.a > 0.0f) {
					for (int row_index = primitive.table_header_row_count; row_index < total_rows; ++row_index) {
						if (((row_index - primitive.table_header_row_count) % 2) != 1) {
							continue;
						}
						float row_top = row_offsets[row_index];
						float row_bottom = row_offsets[row_index + 1];
						Rect2 row_rect(
								table_rect.position.x + 1.0f,
								table_rect.position.y + row_top,
								std::max(0.0f, table_rect.size.x - 2.0f),
								std::max(0.0f, row_bottom - row_top - (row_index + 1 == total_rows ? 1.0f : 0.0f)));
						rendering_server->canvas_item_add_rect(canvas_item, row_rect, primitive.table_alt_row_bg);
					}
				}

				// Internal horizontal dividers
				for (int row_index = 1; row_index < total_rows; ++row_index) {
					float divider_y = table_rect.position.y + row_offsets[row_index];
					float line_width = (row_index == primitive.table_header_row_count) ? 1.5f : 1.0f;
					rendering_server->canvas_item_add_line(
							canvas_item,
							Vector2(table_rect.position.x, divider_y),
							Vector2(table_rect.position.x + table_rect.size.x, divider_y),
							primitive.border_color,
							line_width,
							false);
				}

				// Internal vertical dividers
				Color vertical_divider_color(
						primitive.border_color.r,
						primitive.border_color.g,
						primitive.border_color.b,
						primitive.border_color.a * 0.65f);
				for (size_t column_index = 1; column_index + 1 < column_offsets.size(); ++column_index) {
					float divider_x = table_rect.position.x + column_offsets[column_index];
					rendering_server->canvas_item_add_line(
							canvas_item,
							Vector2(divider_x, table_rect.position.y),
							Vector2(divider_x, table_rect.position.y + table_rect.size.y),
							vertical_divider_color,
							1.0f,
							false);
				}

				// Crisp outer rounded border
				if (primitive.style_box.is_null()) {
					gdmarkdown::draw_rounded_panel(
							canvas_item,
							table_rect,
							Color(0.0f, 0.0f, 0.0f, 0.0f),
							primitive.border_color,
							primitive.border_width,
							primitive.corner_radius);
				}
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::CALLOUT_ICON: {
				if (primitive.texture.is_valid()) {
					primitive.texture->draw_rect(canvas_item, primitive.rectangle, false);
				} else {
					gdmarkdown::draw_procedural_callout_icon(
							canvas_item,
							primitive.rectangle,
							primitive.callout_kind,
							primitive.fill_color);
				}
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::TASK_CHECKBOX: {
				bool checked = false;
				if (primitive.task_index >= 0 && static_cast<size_t>(primitive.task_index) < task_states.size()) {
					checked = task_states[static_cast<size_t>(primitive.task_index)];
				}
				gdmarkdown::draw_task_checkbox(
						canvas_item,
						primitive.rectangle,
						checked,
						theme->get_task_checked_color(),
						theme->get_task_unchecked_color());
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::BLOCK_IMAGE: {
				if (primitive.texture.is_valid()) {
					primitive.texture->draw_rect(canvas_item, primitive.rectangle, false);
				}
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::PARAGRAPH: {
				if (primitive.paragraph_slot >= 0 && static_cast<size_t>(primitive.paragraph_slot) < layout.paragraphs.size()) {
					const gdmarkdown::LaidOutParagraph &laid_out = layout.paragraphs[static_cast<size_t>(primitive.paragraph_slot)];
					gdmarkdown::draw_laid_out_paragraph(
							canvas_item,
							laid_out,
							laid_out.position,
							theme,
							hovered_link_url,
							typewriter_active ? visible_characters : -1);
				}
				break;
			}

			case gdmarkdown::PaintPrimitiveKind::SCROLLABLE_CODE_BLOCK: {
				if (primitive.code_scroll_slot < 0 || static_cast<size_t>(primitive.code_scroll_slot) >= layout.code_scroll_regions.size()) {
					break;
				}
				const gdmarkdown::CodeScrollRegion &region = layout.code_scroll_regions[static_cast<size_t>(primitive.code_scroll_slot)];
				if (region.paragraph_slot < 0 || static_cast<size_t>(region.paragraph_slot) >= layout.paragraphs.size()) {
					break;
				}
				if (region.clip_canvas_item.is_valid()) {
					rendering_server->canvas_item_set_visible(region.clip_canvas_item, true);
					gdmarkdown::draw_laid_out_paragraph(
							region.clip_canvas_item,
							layout.paragraphs[static_cast<size_t>(region.paragraph_slot)],
							Vector2(-region.scroll_x, 0.0f),
							theme,
							hovered_link_url,
							typewriter_active ? visible_characters : -1);
				}

				if (region.max_scroll_x > 0.0f && region.content_width > 0.0f) {
					float track_width = region.inner_rectangle.size.x;
					float thumb_ratio = std::clamp(track_width / region.content_width, 0.08f, 1.0f);
					float thumb_width = track_width * thumb_ratio;
					float scroll_fraction = std::clamp(region.scroll_x / region.max_scroll_x, 0.0f, 1.0f);
					float thumb_x = region.inner_rectangle.position.x + (track_width - thumb_width) * scroll_fraction;
					float thumb_height = 3.0f;
					float thumb_y = region.outer_rectangle.position.y + region.outer_rectangle.size.y - thumb_height - 3.0f;

					Color muted = theme->get_muted_color();
					Color thumb_color(muted.r, muted.g, muted.b, muted.a * 0.5f);
					gdmarkdown::draw_rounded_panel(
							canvas_item,
							Rect2(thumb_x, thumb_y, thumb_width, thumb_height),
							thumb_color,
							Color(0.0f, 0.0f, 0.0f, 0.0f),
							0.0f,
							1.5f);
				}
				break;
			}
		}
	}
}

void MarkdownView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_ensure_layout();
			_sync_clip_canvas_items();
			queue_redraw();
			break;
		}
		case NOTIFICATION_READY: {
			_ensure_layout();
			break;
		}
		case NOTIFICATION_RESIZED: {
			if (std::abs(get_size().x - last_layout_width) > 0.5f) {
				layout_dirty = true;
				_ensure_layout();
			}
			break;
		}
		case NOTIFICATION_THEME_CHANGED: {
			_mark_layout_dirty();
			break;
		}
		case NOTIFICATION_DRAW: {
			_ensure_layout();
			_draw_document();
			break;
		}
		case NOTIFICATION_MOUSE_EXIT: {
			_set_hovered_link(String());
			set_default_cursor_shape(CURSOR_ARROW);
			dragging_code_scroll_index = -1;
			break;
		}
		case NOTIFICATION_EXIT_TREE: {
			_free_clip_canvas_items();
			break;
		}
		default:
			break;
	}
}

// Report 0 minimum width so parent containers can shrink or expand the control
// horizontally freely, while minimum height tracks the laid-out document height.
Vector2 MarkdownView::_get_minimum_size() const {
	return Vector2(0.0f, layout.total_height);
}

// Input priority: active code-scroll drag > interactive task checkbox > link
// click > code-scroll drag start / horizontal wheel / trackpad pan gesture.
// Unhandled events propagate untouched to enclosing ScrollContainers.
void MarkdownView::_gui_input(const Ref<InputEvent> &p_event) {
	if (p_event.is_null()) {
		return;
	}

	Ref<InputEventMouseMotion> mouse_motion = p_event;
	if (mouse_motion.is_valid()) {
		Vector2 pointer_position = mouse_motion->get_position();
		if (dragging_code_scroll_index >= 0 && static_cast<size_t>(dragging_code_scroll_index) < layout.code_scroll_regions.size()) {
			gdmarkdown::CodeScrollRegion &region = layout.code_scroll_regions[static_cast<size_t>(dragging_code_scroll_index)];
			float delta_x = pointer_position.x - drag_start_x;
			float next_scroll_x = std::clamp(drag_start_scroll_x - delta_x, 0.0f, region.max_scroll_x);
			if (std::abs(next_scroll_x - region.scroll_x) > 0.25f) {
				region.scroll_x = next_scroll_x;
				queue_redraw();
			}
			accept_event();
			return;
		}

		_set_hovered_link(_find_link_at(pointer_position));
		_update_cursor_for_position(pointer_position);
		return;
	}

	Ref<InputEventMouseButton> mouse_button = p_event;
	if (mouse_button.is_valid()) {
		Vector2 pointer_position = mouse_button->get_position();
		MouseButton button_index = mouse_button->get_button_index();

		if (button_index == MOUSE_BUTTON_LEFT) {
			if (mouse_button->is_pressed()) {
				if (interactive_tasks) {
					int task_index = _find_task_at(pointer_position);
					if (task_index >= 0 && static_cast<size_t>(task_index) < task_states.size()) {
						bool next_checked = !task_states[static_cast<size_t>(task_index)];
						task_states[static_cast<size_t>(task_index)] = next_checked;
						queue_redraw();
						emit_signal("task_toggled", task_index, next_checked);
						accept_event();
						return;
					}
				}

				String link_url = _find_link_at(pointer_position);
				if (!link_url.is_empty()) {
					emit_signal("link_clicked", link_url);
					accept_event();
					return;
				}

				int code_region_index = _find_code_scroll_at(pointer_position);
				if (code_region_index >= 0) {
					dragging_code_scroll_index = code_region_index;
					drag_start_x = pointer_position.x;
					drag_start_scroll_x = layout.code_scroll_regions[static_cast<size_t>(code_region_index)].scroll_x;
					accept_event();
					return;
				}
			} else if (dragging_code_scroll_index >= 0) {
				dragging_code_scroll_index = -1;
				accept_event();
				return;
			}
		}

		if (mouse_button->is_pressed()) {
			float wheel_delta = 0.0f;
			float factor = mouse_button->get_factor() > 0.0f ? mouse_button->get_factor() : 1.0f;
			if (button_index == MOUSE_BUTTON_WHEEL_LEFT || (mouse_button->is_shift_pressed() && button_index == MOUSE_BUTTON_WHEEL_UP)) {
				wheel_delta = -32.0f * factor;
			} else if (button_index == MOUSE_BUTTON_WHEEL_RIGHT || (mouse_button->is_shift_pressed() && button_index == MOUSE_BUTTON_WHEEL_DOWN)) {
				wheel_delta = 32.0f * factor;
			}

			if (wheel_delta != 0.0f) {
				int code_region_index = _find_code_scroll_at(pointer_position);
				if (code_region_index >= 0) {
					gdmarkdown::CodeScrollRegion &region = layout.code_scroll_regions[static_cast<size_t>(code_region_index)];
					float next_scroll_x = std::clamp(region.scroll_x + wheel_delta, 0.0f, region.max_scroll_x);
					if (std::abs(next_scroll_x - region.scroll_x) > 0.25f) {
						region.scroll_x = next_scroll_x;
						queue_redraw();
					}
					accept_event();
					return;
				}
			}
		}
		return;
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		Vector2 delta = pan_gesture->get_delta();
		if (std::abs(delta.x) > std::abs(delta.y)) {
			int code_region_index = _find_code_scroll_at(pan_gesture->get_position());
			if (code_region_index >= 0) {
				gdmarkdown::CodeScrollRegion &region = layout.code_scroll_regions[static_cast<size_t>(code_region_index)];
				float next_scroll_x = std::clamp(region.scroll_x + delta.x * 24.0f, 0.0f, region.max_scroll_x);
				if (std::abs(next_scroll_x - region.scroll_x) > 0.25f) {
					region.scroll_x = next_scroll_x;
					queue_redraw();
				}
				accept_event();
			}
		}
	}
}

void MarkdownView::set_markdown(const String &p_markdown) {
	markdown = p_markdown;
	_rebuild_document();
}

String MarkdownView::get_markdown() const {
	return markdown;
}

void MarkdownView::set_markdown_theme(const Ref<MarkdownTheme> &p_theme) {
	if (markdown_theme == p_theme) {
		return;
	}

	Callable callback = callable_mp(this, &MarkdownView::_on_theme_changed);
	if (markdown_theme.is_valid() && markdown_theme->is_connected("changed", callback)) {
		markdown_theme->disconnect("changed", callback);
	}

	markdown_theme = p_theme;
	if (markdown_theme.is_valid() && !markdown_theme->is_connected("changed", callback)) {
		markdown_theme->connect("changed", callback);
	}

	_mark_layout_dirty();
}

Ref<MarkdownTheme> MarkdownView::get_markdown_theme() const {
	return markdown_theme;
}

void MarkdownView::set_interactive_tasks(bool p_enabled) {
	if (interactive_tasks == p_enabled) {
		return;
	}
	interactive_tasks = p_enabled;
	queue_redraw();
}

bool MarkdownView::is_interactive_tasks() const {
	return interactive_tasks;
}

void MarkdownView::set_code_block_wrap(bool p_wrap) {
	if (code_block_wrap == p_wrap) {
		return;
	}
	code_block_wrap = p_wrap;
	_mark_layout_dirty();
}

bool MarkdownView::is_code_block_wrap() const {
	return code_block_wrap;
}

void MarkdownView::set_visible_characters(int p_count) {
	int normalized = p_count < 0 ? -1 : p_count;
	if (visible_characters == normalized) {
		return;
	}
	visible_characters = normalized;
	_ensure_layout();

	if (visible_characters < 0) {
		visible_ratio = 1.0f;
	} else if (layout.total_character_count > 0) {
		visible_ratio = std::clamp(
				static_cast<float>(visible_characters) / static_cast<float>(layout.total_character_count),
				0.0f,
				1.0f);
	} else {
		visible_ratio = visible_characters == 0 ? 0.0f : 1.0f;
	}
	queue_redraw();
}

int MarkdownView::get_visible_characters() const {
	return visible_characters;
}

void MarkdownView::set_visible_ratio(float p_ratio) {
	float clamped = std::clamp(p_ratio, 0.0f, 1.0f);
	_ensure_layout();
	visible_ratio = clamped;
	if (clamped >= 1.0f) {
		visible_characters = -1;
	} else {
		visible_characters = static_cast<int>(std::round(clamped * static_cast<float>(layout.total_character_count)));
	}
	queue_redraw();
}

float MarkdownView::get_visible_ratio() const {
	return visible_ratio;
}

void MarkdownView::set_image_resolver(const Callable &p_callable) {
	if (image_resolver == p_callable) {
		return;
	}
	image_resolver = p_callable;
	image_cache.clear();
	_mark_layout_dirty();
}

Callable MarkdownView::get_image_resolver() const {
	return image_resolver;
}

int MarkdownView::get_total_character_count() {
	_ensure_layout();
	return layout.total_character_count;
}

int MarkdownView::get_task_count() const {
	return static_cast<int>(task_states.size());
}

bool MarkdownView::is_task_checked(int p_task_index) const {
	if (p_task_index < 0 || static_cast<size_t>(p_task_index) >= task_states.size()) {
		return false;
	}
	return task_states[static_cast<size_t>(p_task_index)];
}

void MarkdownView::set_task_checked(int p_task_index, bool p_checked) {
	if (p_task_index < 0 || static_cast<size_t>(p_task_index) >= task_states.size()) {
		return;
	}
	if (task_states[static_cast<size_t>(p_task_index)] == p_checked) {
		return;
	}
	task_states[static_cast<size_t>(p_task_index)] = p_checked;
	queue_redraw();
}

String MarkdownView::get_plain_text() {
	_ensure_layout();
	return layout.plain_text;
}

void MarkdownView::clear_image_cache() {
	image_cache.clear();
	_mark_layout_dirty();
}

} // namespace godot
