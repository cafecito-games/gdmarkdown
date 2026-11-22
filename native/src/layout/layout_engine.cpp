// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#include "layout/layout_engine.hpp"

#include "layout/table_sizing.hpp"

#include <godot_cpp/classes/font_variation.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace godot;

namespace gdmarkdown {

namespace {

constexpr float PI_VALUE = 3.14159265358979323846f;

String utf8_to_godot_string(const std::string &source) {
	if (source.empty()) {
		return String();
	}
	return String::utf8(source.c_str(), static_cast<int>(source.size()));
}

HorizontalAlignment to_horizontal_alignment(TableAlignment alignment) {
	switch (alignment) {
		case TableAlignment::CENTER:
			return HORIZONTAL_ALIGNMENT_CENTER;
		case TableAlignment::RIGHT:
			return HORIZONTAL_ALIGNMENT_RIGHT;
		case TableAlignment::LEFT:
		case TableAlignment::DEFAULT:
		default:
			return HORIZONTAL_ALIGNMENT_LEFT;
	}
}

// Computes horizontal line shift matching TextParagraph's alignment so manual
// decoration rects and per-glyph draws line up with TextParagraph::draw().
float compute_line_offset_x(const Ref<TextParagraph> &paragraph, float line_width) {
	float container_width = paragraph->get_width();
	if (container_width <= 0.0f) {
		return 0.0f;
	}
	float remaining_width = std::max(0.0f, container_width - line_width);
	switch (paragraph->get_alignment()) {
		case HORIZONTAL_ALIGNMENT_CENTER:
			return std::floor(remaining_width * 0.5f);
		case HORIZONTAL_ALIGNMENT_RIGHT:
			return std::floor(remaining_width);
		case HORIZONTAL_ALIGNMENT_LEFT:
		case HORIZONTAL_ALIGNMENT_FILL:
		default:
			return 0.0f;
	}
}

// Synthesizes faux bold and/or oblique italic faces via FontVariation when the
// theme does not supply dedicated font resources for those weights/styles.
Ref<Font> synthesize_font_variation(const Ref<Font> &base_font, float embolden, bool italic_slant) {
	if (base_font.is_null()) {
		return Ref<Font>();
	}
	Ref<FontVariation> variation;
	variation.instantiate();
	variation->set_base_font(base_font);
	if (embolden > 0.0f) {
		variation->set_variation_embolden(embolden);
	}
	if (italic_slant) {
		variation->set_variation_transform(Transform2D(1.0f, 0.2f, 0.0f, 1.0f, 0.0f, 0.0f));
	}
	return variation;
}

Color resolve_glyph_color(
		const LaidOutParagraph &laid_out,
		int character_index,
		const String &hovered_link_url,
		const Ref<MarkdownTheme> &theme) {
	for (const ColorSpan &span : laid_out.spans) {
		if (character_index >= span.start && character_index < span.end) {
			if (!span.link_url.is_empty() && !hovered_link_url.is_empty() && span.link_url == hovered_link_url && theme.is_valid()) {
				return theme->get_link_hover_color();
			}
			return span.color;
		}
	}
	return laid_out.base_color;
}

struct PendingInlineImage {
	String object_key;
	Ref<Texture2D> texture;
	int character_index = 0;
	String link_url;
};

struct ResolvedFonts {
	Ref<Font> regular;
	Ref<Font> bold;
	Ref<Font> italic;
	Ref<Font> bold_italic;
	Ref<Font> monospace;

	void initialize(const Ref<MarkdownTheme> &theme, const Ref<Font> &fallback_font) {
		regular = theme->get_regular_font().is_valid() ? theme->get_regular_font() : fallback_font;

		if (theme->get_bold_font().is_valid()) {
			bold = theme->get_bold_font();
		} else if (regular.is_valid()) {
			bold = synthesize_font_variation(regular, 0.5f, false);
		}

		if (theme->get_italic_font().is_valid()) {
			italic = theme->get_italic_font();
		} else if (regular.is_valid()) {
			italic = synthesize_font_variation(regular, 0.0f, true);
		}

		if (theme->get_bold_italic_font().is_valid()) {
			bold_italic = theme->get_bold_italic_font();
		} else if (theme->get_bold_font().is_valid()) {
			bold_italic = synthesize_font_variation(theme->get_bold_font(), 0.0f, true);
		} else if (theme->get_italic_font().is_valid()) {
			bold_italic = synthesize_font_variation(theme->get_italic_font(), 0.5f, false);
		} else if (regular.is_valid()) {
			bold_italic = synthesize_font_variation(regular, 0.5f, true);
		}

		if (theme->get_monospace_font().is_valid()) {
			monospace = theme->get_monospace_font();
		} else {
			monospace = regular;
		}
	}

	Ref<Font> select(uint8_t style_flags) const {
		if ((style_flags & INLINE_STYLE_CODE) != 0) {
			return monospace;
		}
		bool is_bold = (style_flags & INLINE_STYLE_BOLD) != 0;
		bool is_italic = (style_flags & INLINE_STYLE_ITALIC) != 0;
		if (is_bold && is_italic) {
			return bold_italic;
		}
		if (is_bold) {
			return bold;
		}
		if (is_italic) {
			return italic;
		}
		return regular;
	}
};

class LayoutBuilder {
private:
	LayoutOptions options;
	Ref<MarkdownTheme> theme;
	ResolvedFonts fonts;
	LayoutDocument result;
	int running_character_count = 0;
	int64_t next_object_id = 1;

	void append_plain_text_segment(const String &segment) {
		if (segment.is_empty()) {
			return;
		}
		if (!result.plain_text.is_empty()) {
			result.plain_text += "\n";
		}
		result.plain_text += segment;
	}

	Ref<Texture2D> resolve_image_uri(const String &uri) const {
		if (uri.is_empty() || !options.resolve_image) {
			return Ref<Texture2D>();
		}
		return options.resolve_image(uri);
	}

	int add_paragraph(
			const std::vector<InlineNode> &inlines,
			const Vector2 &position,
			float width,
			int font_size,
			const Color &base_color,
			uint8_t default_style_flags,
			HorizontalAlignment alignment,
			bool count_in_document,
			bool emit_primitive) {
		Ref<TextParagraph> paragraph;
		paragraph.instantiate();
		paragraph->set_alignment(alignment);
		paragraph->set_line_spacing(theme->get_line_spacing());
		paragraph->set_width(width);
		paragraph->set_break_flags(
				TextServer::BREAK_MANDATORY |
				TextServer::BREAK_WORD_BOUND |
				TextServer::BREAK_ADAPTIVE);

		LaidOutParagraph laid_out;
		laid_out.paragraph = paragraph;
		laid_out.position = position;
		laid_out.base_color = base_color;

		int code_font_size = std::max(1, static_cast<int>(std::round(static_cast<float>(font_size) * theme->get_code_font_scale())));
		float inline_code_padding_horizontal = theme->get_inline_code_padding_horizontal();
		int local_offset = 0;
		String plain_segment;
		std::vector<PendingInlineImage> pending_images;

		for (const InlineNode &inline_node : inlines) {
			if (inline_node.kind == InlineKind::SOFT_BREAK) {
				Ref<Font> font = fonts.select(default_style_flags);
				if (font.is_valid()) {
					paragraph->add_string(" ", font, font_size);
					local_offset += 1;
					plain_segment += " ";
				}
				continue;
			}

			if (inline_node.kind == InlineKind::HARD_BREAK) {
				Ref<Font> font = fonts.select(default_style_flags);
				if (font.is_valid()) {
					paragraph->add_string("\n", font, font_size);
					local_offset += 1;
					plain_segment += "\n";
				}
				continue;
			}

			if (inline_node.kind == InlineKind::IMAGE) {
				String image_uri = utf8_to_godot_string(inline_node.image_source);
				String link_url = utf8_to_godot_string(inline_node.link_url);
				Ref<Texture2D> texture = resolve_image_uri(image_uri);

				if (texture.is_valid() && texture->get_width() > 0 && texture->get_height() > 0) {
					float target_height = std::max(8.0f, static_cast<float>(font_size) * theme->get_inline_image_scale());
					float aspect_ratio = static_cast<float>(texture->get_width()) / static_cast<float>(texture->get_height());
					Vector2 display_size(std::max(1.0f, target_height * aspect_ratio), target_height);
					if (width > 0.0f && display_size.x > width) {
						float clamp_scale = width / display_size.x;
						display_size.x = width;
						display_size.y = std::max(1.0f, display_size.y * clamp_scale);
					}

					String image_key = String("__img_") + String::num_int64(next_object_id++);
					int image_character_index = local_offset;
					paragraph->add_object(image_key, display_size, INLINE_ALIGNMENT_CENTER, 1);
					local_offset += 1;

					PendingInlineImage pending;
					pending.object_key = image_key;
					pending.texture = texture;
					pending.character_index = image_character_index;
					pending.link_url = link_url;
					pending_images.push_back(pending);
				} else {
					String fallback_text = utf8_to_godot_string(inline_node.image_alt);
					if (fallback_text.is_empty()) {
						fallback_text = String("[image]");
					}
					Ref<Font> font = fonts.select(default_style_flags | INLINE_STYLE_ITALIC);
					if (font.is_valid() && !fallback_text.is_empty()) {
						int span_start = local_offset;
						paragraph->add_string(fallback_text, font, font_size);
						local_offset += fallback_text.length();
						plain_segment += fallback_text;

						ColorSpan span;
						span.start = span_start;
						span.end = local_offset;
						span.color = !link_url.is_empty() ? theme->get_link_color() : theme->get_muted_color();
						span.link_url = link_url;
						laid_out.spans.push_back(span);
						laid_out.has_rich_spans = true;
					}
				}
				continue;
			}

			// InlineKind::TEXT
			String text = utf8_to_godot_string(inline_node.text);
			if (text.is_empty()) {
				continue;
			}

			uint8_t combined_flags = static_cast<uint8_t>(default_style_flags | inline_node.style_flags);
			bool is_code = (combined_flags & INLINE_STYLE_CODE) != 0;
			bool is_strikethrough = (combined_flags & INLINE_STYLE_STRIKETHROUGH) != 0;
			String link_url = utf8_to_godot_string(inline_node.link_url);

			Ref<Font> font = fonts.select(combined_flags);
			if (font.is_null()) {
				continue;
			}

			// Reserve horizontal breathing room on both sides of inline code spans
			// using zero-visual-height embedded objects so adjacent text glyphs never
			// collide with the rounded pill background.
			if (is_code && inline_code_padding_horizontal > 0.0f) {
				String left_spacer_key = String("__cs_l_") + String::num_int64(next_object_id++);
				paragraph->add_object(left_spacer_key, Vector2(inline_code_padding_horizontal * 0.8f, 1.0f), INLINE_ALIGNMENT_CENTER, 1);
				local_offset += 1;
			}

			int span_start = local_offset;
			paragraph->add_string(text, font, is_code ? code_font_size : font_size);
			local_offset += text.length();
			plain_segment += text;

			Color span_color = base_color;
			bool needs_span = false;
			if (!link_url.is_empty()) {
				span_color = theme->get_link_color();
				needs_span = true;
			} else if (is_code) {
				span_color = theme->get_code_text_color();
				needs_span = true;
			} else if (is_strikethrough) {
				span_color = base_color.lerp(theme->get_muted_color(), 0.35f);
				needs_span = true;
			}

			if (needs_span) {
				ColorSpan span;
				span.start = span_start;
				span.end = local_offset;
				span.color = span_color;
				span.link_url = link_url;
				span.is_code = is_code;
				span.is_strikethrough = is_strikethrough;
				laid_out.spans.push_back(span);
				laid_out.has_rich_spans = true;
			}

			if (is_code && inline_code_padding_horizontal > 0.0f) {
				String right_spacer_key = String("__cs_r_") + String::num_int64(next_object_id++);
				paragraph->add_object(right_spacer_key, Vector2(inline_code_padding_horizontal * 0.8f, 1.0f), INLINE_ALIGNMENT_CENTER, 1);
				local_offset += 1;
			}
		}

		laid_out.size = paragraph->get_size();
		laid_out.local_char_count = local_offset;
		if (count_in_document) {
			laid_out.document_char_start = running_character_count;
			laid_out.document_char_end = running_character_count + local_offset;
			running_character_count = laid_out.document_char_end;
			append_plain_text_segment(plain_segment);
		} else {
			laid_out.document_char_start = running_character_count;
			laid_out.document_char_end = running_character_count;
		}

		populate_decorations_and_hits(laid_out, font_size, pending_images);

		int slot_index = static_cast<int>(result.paragraphs.size());
		laid_out.paragraph_index = slot_index;
		result.paragraphs.push_back(std::move(laid_out));

		if (emit_primitive) {
			const LaidOutParagraph &stored = result.paragraphs[slot_index];
			PaintPrimitive primitive;
			primitive.kind = PaintPrimitiveKind::PARAGRAPH;
			primitive.rectangle = Rect2(stored.position, stored.size);
			primitive.paragraph_slot = slot_index;
			primitive.document_char_threshold = stored.document_char_start;
			result.primitives.push_back(primitive);
		}

		return slot_index;
	}

	int add_plain_text_paragraph(
			const String &text,
			const Vector2 &position,
			float width,
			int font_size,
			const Ref<Font> &font,
			const Color &color,
			HorizontalAlignment alignment,
			BitField<TextServer::LineBreakFlag> break_flags,
			bool count_in_document,
			bool emit_primitive) {
		Ref<TextParagraph> paragraph;
		paragraph.instantiate();
		paragraph->set_alignment(alignment);
		paragraph->set_line_spacing(theme->get_line_spacing());
		paragraph->set_width(width);
		paragraph->set_break_flags(break_flags);

		int local_offset = 0;
		if (font.is_valid() && !text.is_empty()) {
			paragraph->add_string(text, font, font_size);
			local_offset = text.length();
		}

		LaidOutParagraph laid_out;
		laid_out.paragraph = paragraph;
		laid_out.position = position;
		laid_out.size = paragraph->get_size();
		laid_out.base_color = color;
		laid_out.local_char_count = local_offset;
		laid_out.has_rich_spans = false;

		if (count_in_document) {
			laid_out.document_char_start = running_character_count;
			laid_out.document_char_end = running_character_count + local_offset;
			running_character_count = laid_out.document_char_end;
			append_plain_text_segment(text);
		} else {
			laid_out.document_char_start = running_character_count;
			laid_out.document_char_end = running_character_count;
		}

		int slot_index = static_cast<int>(result.paragraphs.size());
		laid_out.paragraph_index = slot_index;
		result.paragraphs.push_back(std::move(laid_out));

		if (emit_primitive) {
			const LaidOutParagraph &stored = result.paragraphs[slot_index];
			PaintPrimitive primitive;
			primitive.kind = PaintPrimitiveKind::PARAGRAPH;
			primitive.rectangle = Rect2(stored.position, stored.size);
			primitive.paragraph_slot = slot_index;
			primitive.document_char_threshold = stored.document_char_start;
			result.primitives.push_back(primitive);
		}

		return slot_index;
	}

	// Queries TextServer selection geometry for every shaped line to derive
	// exact visual bounds for inline code pills, strikethrough strokes, link
	// underlines, link hitboxes, and embedded inline images.
	void populate_decorations_and_hits(
			LaidOutParagraph &laid_out,
			int font_size,
			const std::vector<PendingInlineImage> &pending_images) {
		if (laid_out.spans.empty() && pending_images.empty()) {
			return;
		}

		TextServerManager *manager = TextServerManager::get_singleton();
		if (manager == nullptr) {
			return;
		}
		Ref<TextServer> text_server = manager->get_primary_interface();
		if (text_server.is_null()) {
			return;
		}

		const Ref<TextParagraph> &paragraph = laid_out.paragraph;
		int line_count = paragraph->get_line_count();
		float line_spacing = paragraph->get_line_spacing();
		float line_y = 0.0f;

		float padding_horizontal = theme->get_inline_code_padding_horizontal();
		float padding_vertical = theme->get_inline_code_padding_vertical();

		for (int line_index = 0; line_index < line_count; ++line_index) {
			RID line_rid = paragraph->get_line_rid(line_index);
			Vector2 line_size = paragraph->get_line_size(line_index);
			float line_ascent = paragraph->get_line_ascent(line_index);
			float line_descent = paragraph->get_line_descent(line_index);
			Vector2i line_range = paragraph->get_line_range(line_index);
			float line_offset_x = compute_line_offset_x(paragraph, line_size.x);

			for (const ColorSpan &span : laid_out.spans) {
				int overlap_start = std::max(span.start, line_range.x);
				int overlap_end = std::min(span.end, line_range.y);
				if (overlap_start >= overlap_end) {
					continue;
				}

				PackedVector2Array selections = text_server->shaped_text_get_selection(line_rid, overlap_start, overlap_end);
				for (int selection_index = 0; selection_index < selections.size(); ++selection_index) {
					Vector2 selection = selections[selection_index];
					float left_x = line_offset_x + std::min(selection.x, selection.y);
					float right_x = line_offset_x + std::max(selection.x, selection.y);
					float span_width = right_x - left_x;
					if (span_width <= 0.0f) {
						continue;
					}

					if (span.is_code) {
						DecorationRect pill;
						pill.rectangle = Rect2(
								left_x - padding_horizontal * 0.72f,
								line_y - padding_vertical,
								span_width + padding_horizontal * 1.44f,
								line_size.y + padding_vertical * 2.0f);
						pill.char_start = overlap_start;
						pill.char_end = overlap_end;
						pill.link_url = span.link_url;
						laid_out.inline_code_pills.push_back(pill);
					}

					if (span.is_strikethrough) {
						float strike_y = line_y + line_ascent * 0.62f;
						DecorationLine strike_line;
						strike_line.start_point = Vector2(left_x, strike_y);
						strike_line.end_point = Vector2(right_x, strike_y);
						strike_line.thickness = std::max(1.0f, static_cast<float>(font_size) * 0.075f);
						strike_line.color = theme->get_strikethrough_color();
						strike_line.char_start = overlap_start;
						strike_line.char_end = overlap_end;
						strike_line.link_url = span.link_url;
						laid_out.strikethrough_lines.push_back(strike_line);
					}

					if (!span.link_url.is_empty()) {
						float underline_offset = paragraph->get_line_underline_position(line_index);
						float underline_y = line_y + line_ascent + (underline_offset > 0.0f ? underline_offset : std::max(2.0f, line_descent * 0.45f));
						float underline_thickness = std::max(1.0f, paragraph->get_line_underline_thickness(line_index));

						DecorationLine underline;
						underline.start_point = Vector2(left_x, underline_y);
						underline.end_point = Vector2(right_x, underline_y);
						underline.thickness = underline_thickness;
						underline.color = theme->get_link_color();
						underline.char_start = overlap_start;
						underline.char_end = overlap_end;
						underline.link_url = span.link_url;
						laid_out.link_underlines.push_back(underline);

						LinkHitBox hit_box;
						hit_box.rectangle = Rect2(
								laid_out.position.x + left_x,
								laid_out.position.y + line_y,
								span_width,
								line_size.y);
						hit_box.url = span.link_url;
						result.link_hit_boxes.push_back(hit_box);
					}
				}
			}

			if (!pending_images.empty()) {
				Array line_objects = paragraph->get_line_objects(line_index);
				for (int object_index = 0; object_index < line_objects.size(); ++object_index) {
					Variant object_key = line_objects[object_index];
					if (object_key.get_type() != Variant::STRING) {
						continue;
					}
					String key_string = object_key;
					for (const PendingInlineImage &pending : pending_images) {
						if (pending.object_key == key_string) {
							Rect2 object_rectangle = paragraph->get_line_object_rect(line_index, object_key);
							InlineImagePlacement placement;
							placement.rectangle = object_rectangle;
							placement.texture = pending.texture;
							placement.char_index = pending.character_index;
							laid_out.inline_images.push_back(placement);

							if (!pending.link_url.is_empty()) {
								LinkHitBox hit_box;
								hit_box.rectangle = Rect2(laid_out.position + object_rectangle.position, object_rectangle.size);
								hit_box.url = pending.link_url;
								result.link_hit_boxes.push_back(hit_box);
							}
							break;
						}
					}
				}
			}

			line_y += line_size.y + line_spacing;
		}
	}

	Vector2 measure_inlines(
			const std::vector<InlineNode> &inlines,
			float width,
			int font_size,
			uint8_t default_style_flags,
			BitField<TextServer::LineBreakFlag> break_flags) const {
		Ref<TextParagraph> paragraph;
		paragraph.instantiate();
		paragraph->set_line_spacing(theme->get_line_spacing());
		paragraph->set_width(width);
		paragraph->set_break_flags(break_flags);

		int code_font_size = std::max(1, static_cast<int>(std::round(static_cast<float>(font_size) * theme->get_code_font_scale())));
		float inline_code_padding_horizontal = theme->get_inline_code_padding_horizontal();
		int64_t measurement_object_id = 1;

		for (const InlineNode &inline_node : inlines) {
			if (inline_node.kind == InlineKind::SOFT_BREAK) {
				Ref<Font> font = fonts.select(default_style_flags);
				if (font.is_valid()) {
					paragraph->add_string(" ", font, font_size);
				}
				continue;
			}
			if (inline_node.kind == InlineKind::HARD_BREAK) {
				Ref<Font> font = fonts.select(default_style_flags);
				if (font.is_valid()) {
					paragraph->add_string("\n", font, font_size);
				}
				continue;
			}
			if (inline_node.kind == InlineKind::IMAGE) {
				String image_uri = utf8_to_godot_string(inline_node.image_source);
				Ref<Texture2D> texture = resolve_image_uri(image_uri);
				if (texture.is_valid() && texture->get_width() > 0 && texture->get_height() > 0) {
					float target_height = std::max(8.0f, static_cast<float>(font_size) * theme->get_inline_image_scale());
					float aspect_ratio = static_cast<float>(texture->get_width()) / static_cast<float>(texture->get_height());
					Vector2 display_size(std::max(1.0f, target_height * aspect_ratio), target_height);
					paragraph->add_object(String("__m_") + String::num_int64(measurement_object_id++), display_size, INLINE_ALIGNMENT_CENTER, 1);
				} else {
					String fallback_text = utf8_to_godot_string(inline_node.image_alt);
					if (fallback_text.is_empty()) {
						fallback_text = String("[image]");
					}
					Ref<Font> font = fonts.select(default_style_flags | INLINE_STYLE_ITALIC);
					if (font.is_valid()) {
						paragraph->add_string(fallback_text, font, font_size);
					}
				}
				continue;
			}

			String text = utf8_to_godot_string(inline_node.text);
			if (text.is_empty()) {
				continue;
			}
			uint8_t combined_flags = static_cast<uint8_t>(default_style_flags | inline_node.style_flags);
			bool is_code = (combined_flags & INLINE_STYLE_CODE) != 0;
			Ref<Font> font = fonts.select(combined_flags);
			if (font.is_null()) {
				continue;
			}

			// Mirror add_paragraph()'s inline-code padding spacers so table column
			// measurements match final rendered widths.
			if (is_code && inline_code_padding_horizontal > 0.0f) {
				paragraph->add_object(String("__m_") + String::num_int64(measurement_object_id++), Vector2(inline_code_padding_horizontal * 0.8f, 1.0f), INLINE_ALIGNMENT_CENTER, 1);
			}
			paragraph->add_string(text, font, is_code ? code_font_size : font_size);
			if (is_code && inline_code_padding_horizontal > 0.0f) {
				paragraph->add_object(String("__m_") + String::num_int64(measurement_object_id++), Vector2(inline_code_padding_horizontal * 0.8f, 1.0f), INLINE_ALIGNMENT_CENTER, 1);
			}
		}

		return paragraph->get_size();
	}

	float layout_block_list(
			const std::vector<Block> &blocks,
			float origin_x,
			float start_y,
			float available_width,
			const Color &text_color,
			int list_depth,
			bool tight_paragraphs) {
		float base_font_size = static_cast<float>(theme->get_base_font_size());
		float paragraph_gap = tight_paragraphs
				? (base_font_size * theme->get_list_item_spacing_em())
				: (base_font_size * theme->get_paragraph_spacing_em());
		float heading_top_gap = base_font_size * theme->get_heading_top_spacing_em();
		float heading_bottom_gap = base_font_size * theme->get_heading_bottom_spacing_em();

		float current_y = start_y;
		for (size_t index = 0; index < blocks.size(); ++index) {
			const Block &block = blocks[index];
			if (index > 0) {
				const Block &previous = blocks[index - 1];
				if (block.kind == BlockKind::HEADING) {
					current_y += heading_top_gap;
				} else if (previous.kind == BlockKind::HEADING) {
					current_y += heading_bottom_gap;
				} else {
					current_y += paragraph_gap;
				}
			}

			current_y = layout_block(block, origin_x, current_y, available_width, text_color, list_depth);
		}
		return current_y;
	}

	float layout_block(
			const Block &block,
			float origin_x,
			float start_y,
			float available_width,
			const Color &text_color,
			int list_depth) {
		int base_font_size = theme->get_base_font_size();
		float clamped_width = std::max(1.0f, available_width);

		switch (block.kind) {
			case BlockKind::PARAGRAPH: {
				int slot = add_paragraph(
						block.inlines,
						Vector2(origin_x, start_y),
						clamped_width,
						base_font_size,
						text_color,
						INLINE_STYLE_NONE,
						HORIZONTAL_ALIGNMENT_LEFT,
						true,
						true);
				return start_y + result.paragraphs[slot].size.y;
			}

			case BlockKind::HEADING: {
				int level = std::clamp(block.heading_level, 1, 6);
				int heading_font_size = std::max(1, static_cast<int>(std::round(static_cast<float>(base_font_size) * theme->get_heading_scale(level))));
				int slot = add_paragraph(
						block.inlines,
						Vector2(origin_x, start_y),
						clamped_width,
						heading_font_size,
						theme->get_heading_color(),
						INLINE_STYLE_BOLD,
						HORIZONTAL_ALIGNMENT_LEFT,
						true,
						true);
				return start_y + result.paragraphs[slot].size.y;
			}

			case BlockKind::BLOCK_QUOTE: {
				float padding = theme->get_blockquote_padding();
				float accent_width = theme->get_blockquote_accent_width();
				float indent = static_cast<float>(base_font_size) * theme->get_blockquote_indent_em() * 0.25f;
				float box_x = origin_x + indent;
				float box_width = std::max(1.0f, clamped_width - indent);
				int threshold = running_character_count;

				size_t background_index = result.primitives.size();
				bool has_background = theme->get_blockquote_style().is_valid() || theme->get_blockquote_bg_color().a > 0.0f;
				if (has_background) {
					PaintPrimitive background;
					if (theme->get_blockquote_style().is_valid()) {
						background.kind = PaintPrimitiveKind::STYLE_BOX;
						background.style_box = theme->get_blockquote_style();
					} else {
						background.kind = PaintPrimitiveKind::ROUNDED_PANEL;
						background.fill_color = theme->get_blockquote_bg_color();
						background.corner_radius = theme->get_corner_radius();
					}
					background.document_char_threshold = threshold;
					result.primitives.push_back(background);
				}

				size_t accent_index = result.primitives.size();
				bool has_accent = accent_width > 0.0f && theme->get_blockquote_accent_color().a > 0.0f;
				if (has_accent) {
					PaintPrimitive accent;
					accent.kind = PaintPrimitiveKind::ACCENT_BAR;
					accent.fill_color = theme->get_blockquote_accent_color();
					accent.corner_radius = std::min(accent_width * 0.5f, 2.0f);
					accent.document_char_threshold = threshold;
					result.primitives.push_back(accent);
				}

				float inner_x = box_x + accent_width + padding;
				float inner_width = std::max(1.0f, box_width - accent_width - padding * 2.0f);
				Color quote_text_color = text_color.lerp(theme->get_muted_color(), 0.2f);
				float content_end_y = layout_block_list(
						block.children,
						inner_x,
						start_y + padding,
						inner_width,
						quote_text_color,
						list_depth,
						false);

				float total_height = std::max(padding * 2.0f + static_cast<float>(base_font_size), content_end_y + padding - start_y);
				if (has_background) {
					result.primitives[background_index].rectangle = Rect2(box_x, start_y, box_width, total_height);
				}
				if (has_accent) {
					result.primitives[accent_index].rectangle = Rect2(box_x, start_y, accent_width, total_height);
				}
				return start_y + total_height;
			}

			case BlockKind::CALLOUT: {
				String kind_string = utf8_to_godot_string(block.callout_kind).strip_edges().to_upper();
				Ref<MarkdownCalloutStyle> callout_style = theme->get_callout_style(kind_string);
				Color accent_color = callout_style->get_accent_color();
				Color title_color = callout_style->resolve_title_color();

				String header_title = utf8_to_godot_string(block.callout_custom_title).strip_edges();
				if (header_title.is_empty()) {
					header_title = callout_style->get_title();
				}
				if (header_title.is_empty()) {
					header_title = kind_string.is_empty() ? String("Note") : kind_string.to_lower().capitalize();
				}

				float padding = theme->get_callout_padding();
				float accent_width = theme->get_callout_accent_width();
				float icon_size = theme->get_callout_icon_size();
				int threshold = running_character_count;

				size_t background_index = result.primitives.size();
				PaintPrimitive background;
				if (callout_style->get_panel_style().is_valid()) {
					background.kind = PaintPrimitiveKind::STYLE_BOX;
					background.style_box = callout_style->get_panel_style();
				} else {
					background.kind = PaintPrimitiveKind::ROUNDED_PANEL;
					background.fill_color = Color(accent_color.r, accent_color.g, accent_color.b, 0.11f);
					background.border_color = Color(accent_color.r, accent_color.g, accent_color.b, 0.28f);
					background.border_width = 1.0f;
					background.corner_radius = theme->get_corner_radius();
				}
				background.document_char_threshold = threshold;
				result.primitives.push_back(background);

				size_t accent_index = result.primitives.size();
				bool has_accent = accent_width > 0.0f && accent_color.a > 0.0f;
				if (has_accent) {
					PaintPrimitive accent;
					accent.kind = PaintPrimitiveKind::ACCENT_BAR;
					accent.fill_color = accent_color;
					accent.corner_radius = std::min(accent_width * 0.5f, 2.0f);
					accent.document_char_threshold = threshold;
					result.primitives.push_back(accent);
				}

				float inner_x = origin_x + accent_width + padding;
				float inner_width = std::max(1.0f, clamped_width - accent_width - padding * 2.0f);
				float header_y = start_y + padding;
				float icon_gap = 8.0f;
				float title_x = inner_x + icon_size + icon_gap;
				float title_width = std::max(1.0f, inner_width - icon_size - icon_gap);

				size_t icon_index = result.primitives.size();
				PaintPrimitive icon_primitive;
				icon_primitive.kind = PaintPrimitiveKind::CALLOUT_ICON;
				icon_primitive.callout_kind = kind_string;
				icon_primitive.texture = callout_style->get_icon();
				icon_primitive.fill_color = title_color;
				icon_primitive.document_char_threshold = threshold;
				result.primitives.push_back(icon_primitive);

				int title_slot = add_plain_text_paragraph(
						header_title,
						Vector2(title_x, header_y),
						title_width,
						base_font_size,
						fonts.select(INLINE_STYLE_BOLD),
						title_color,
						HORIZONTAL_ALIGNMENT_LEFT,
						TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE,
						true,
						true);

				const LaidOutParagraph &title_paragraph = result.paragraphs[title_slot];
				float first_line_height = title_paragraph.paragraph->get_line_count() > 0
						? title_paragraph.paragraph->get_line_size(0).y
						: static_cast<float>(base_font_size);
				float icon_y = header_y + std::max(0.0f, (first_line_height - icon_size) * 0.5f);
				result.primitives[icon_index].rectangle = Rect2(inner_x, icon_y, icon_size, icon_size);

				float header_bottom_y = header_y + std::max(icon_size, title_paragraph.size.y);
				float current_y = header_bottom_y;
				if (!block.children.empty()) {
					current_y += std::max(4.0f, padding * 0.55f);
					current_y = layout_block_list(
							block.children,
							inner_x,
							current_y,
							inner_width,
							text_color,
							list_depth,
							false);
				}

				float total_height = current_y + padding - start_y;
				result.primitives[background_index].rectangle = Rect2(origin_x, start_y, clamped_width, total_height);
				if (has_accent) {
					result.primitives[accent_index].rectangle = Rect2(origin_x, start_y, accent_width, total_height);
				}
				return start_y + total_height;
			}

			case BlockKind::LIST: {
				float indent = static_cast<float>(base_font_size) * theme->get_list_indent_em();
				float item_gap = block.list_tight
						? (static_cast<float>(base_font_size) * theme->get_list_item_spacing_em())
						: (static_cast<float>(base_font_size) * theme->get_paragraph_spacing_em());
				float content_x = origin_x + indent;
				float content_width = std::max(1.0f, clamped_width - indent);
				float current_y = start_y;

				for (size_t item_index = 0; item_index < block.list_items.size(); ++item_index) {
					if (item_index > 0) {
						current_y += item_gap;
					}
					const ListItem &item = block.list_items[item_index];
					float item_start_y = current_y;
					int item_threshold = running_character_count;
					size_t paragraphs_before = result.paragraphs.size();

					float item_end_y = layout_block_list(
							item.children,
							content_x,
							item_start_y,
							content_width,
							text_color,
							list_depth + 1,
							block.list_tight);

					float first_line_height = static_cast<float>(base_font_size) * 1.25f;
					if (result.paragraphs.size() > paragraphs_before) {
						const LaidOutParagraph &first_paragraph = result.paragraphs[paragraphs_before];
						if (first_paragraph.paragraph->get_line_count() > 0) {
							first_line_height = first_paragraph.paragraph->get_line_size(0).y;
						}
					}

					if (item.is_task) {
						float box_size = std::clamp(static_cast<float>(base_font_size) * 0.92f, 12.0f, 20.0f);
						float box_x = origin_x + std::max(0.0f, indent - box_size - 6.0f);
						float box_y = item_start_y + std::max(0.0f, (first_line_height - box_size) * 0.5f);

						PaintPrimitive checkbox;
						checkbox.kind = PaintPrimitiveKind::TASK_CHECKBOX;
						checkbox.rectangle = Rect2(box_x, box_y, box_size, box_size);
						checkbox.task_index = item.task_index;
						checkbox.document_char_threshold = item_threshold;
						result.primitives.push_back(checkbox);

						TaskHitBox hit_box;
						hit_box.rectangle = Rect2(box_x - 4.0f, box_y - 4.0f, box_size + 8.0f, box_size + 8.0f);
						hit_box.task_index = item.task_index;
						result.task_hit_boxes.push_back(hit_box);
					} else {
						String marker_text;
						if (block.list_ordered) {
							marker_text = String::num_int64(static_cast<int64_t>(block.list_start) + static_cast<int64_t>(item_index)) + ".";
						} else if (list_depth % 3 == 0) {
							marker_text = String::utf8("\xE2\x80\xA2");
						} else if (list_depth % 3 == 1) {
							marker_text = String::utf8("\xE2\x97\xA6");
						} else {
							marker_text = String::utf8("\xE2\x96\xAA");
						}

						float marker_width = std::max(1.0f, indent - 6.0f);
						int marker_slot = add_plain_text_paragraph(
								marker_text,
								Vector2(origin_x, item_start_y),
								marker_width,
								base_font_size,
								fonts.regular,
								theme->get_muted_color(),
								HORIZONTAL_ALIGNMENT_RIGHT,
								TextServer::BREAK_NONE,
								false,
								true);
						result.paragraphs[marker_slot].document_char_start = item_threshold;
						result.paragraphs[marker_slot].document_char_end = item_threshold;
						result.primitives.back().document_char_threshold = item_threshold;
					}

					current_y = std::max(item_end_y, item_start_y + first_line_height);
				}
				return current_y;
			}

			case BlockKind::CODE_BLOCK: {
				float padding = theme->get_code_block_padding();
				float inner_width = std::max(1.0f, clamped_width - padding * 2.0f);
				int code_font_size = std::max(1, static_cast<int>(std::round(static_cast<float>(base_font_size) * theme->get_code_font_scale())));
				int threshold = running_character_count;

				std::string trimmed_code = block.code_text;
				if (!trimmed_code.empty() && trimmed_code.back() == '\n') {
					trimmed_code.pop_back();
				}
				String code_string = utf8_to_godot_string(trimmed_code);

				size_t background_index = result.primitives.size();
				PaintPrimitive background;
				if (theme->get_code_block_style().is_valid()) {
					background.kind = PaintPrimitiveKind::STYLE_BOX;
					background.style_box = theme->get_code_block_style();
				} else {
					background.kind = PaintPrimitiveKind::ROUNDED_PANEL;
					background.fill_color = theme->get_code_bg_color();
					background.border_color = theme->get_rule_color();
					background.border_width = 1.0f;
					background.corner_radius = theme->get_corner_radius();
				}
				background.document_char_threshold = threshold;
				result.primitives.push_back(background);

				if (options.code_block_wrap) {
					int slot = add_plain_text_paragraph(
							code_string,
							Vector2(origin_x + padding, start_y + padding),
							inner_width,
							code_font_size,
							fonts.monospace,
							theme->get_code_text_color(),
							HORIZONTAL_ALIGNMENT_LEFT,
							TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE,
							true,
							true);
					float content_height = std::max(static_cast<float>(code_font_size), result.paragraphs[slot].size.y);
					float total_height = content_height + padding * 2.0f;
					result.primitives[background_index].rectangle = Rect2(origin_x, start_y, clamped_width, total_height);
					return start_y + total_height;
				}

				int slot = add_plain_text_paragraph(
						code_string,
						Vector2(origin_x + padding, start_y + padding),
						-1.0f,
						code_font_size,
						fonts.monospace,
						theme->get_code_text_color(),
						HORIZONTAL_ALIGNMENT_LEFT,
						TextServer::BREAK_MANDATORY,
						true,
						false);

				Vector2 natural_size = result.paragraphs[slot].size;
				float content_height = std::max(static_cast<float>(code_font_size), natural_size.y);
				bool needs_scroll = natural_size.x > inner_width + 0.5f;

				if (!needs_scroll) {
					PaintPrimitive paragraph_primitive;
					paragraph_primitive.kind = PaintPrimitiveKind::PARAGRAPH;
					paragraph_primitive.rectangle = Rect2(Vector2(origin_x + padding, start_y + padding), natural_size);
					paragraph_primitive.paragraph_slot = slot;
					paragraph_primitive.document_char_threshold = threshold;
					result.primitives.push_back(paragraph_primitive);

					float total_height = content_height + padding * 2.0f;
					result.primitives[background_index].rectangle = Rect2(origin_x, start_y, clamped_width, total_height);
					return start_y + total_height;
				}

				float scrollbar_margin = 6.0f;
				float total_height = content_height + padding * 2.0f + scrollbar_margin;
				Rect2 outer_rectangle(origin_x, start_y, clamped_width, total_height);
				Rect2 inner_rectangle(origin_x + padding, start_y + padding, inner_width, content_height);

				int scroll_slot = static_cast<int>(result.code_scroll_regions.size());
				CodeScrollRegion region;
				region.outer_rectangle = outer_rectangle;
				region.inner_rectangle = inner_rectangle;
				region.paragraph_slot = slot;
				region.content_width = natural_size.x;
				region.max_scroll_x = std::max(0.0f, natural_size.x - inner_width);
				region.scroll_x = 0.0f;
				result.code_scroll_regions.push_back(region);

				PaintPrimitive scroll_primitive;
				scroll_primitive.kind = PaintPrimitiveKind::SCROLLABLE_CODE_BLOCK;
				scroll_primitive.rectangle = outer_rectangle;
				scroll_primitive.paragraph_slot = slot;
				scroll_primitive.code_scroll_slot = scroll_slot;
				scroll_primitive.document_char_threshold = threshold;
				result.primitives.push_back(scroll_primitive);

				result.primitives[background_index].rectangle = outer_rectangle;
				return start_y + total_height;
			}

			case BlockKind::TABLE: {
				size_t column_count = block.table_alignments.size();
				for (const TableRow &row : block.table_header_rows) {
					column_count = std::max(column_count, row.cells.size());
				}
				for (const TableRow &row : block.table_body_rows) {
					column_count = std::max(column_count, row.cells.size());
				}
				if (column_count == 0) {
					return start_y;
				}

				float padding_horizontal = theme->get_table_cell_padding_horizontal();
				float padding_vertical = theme->get_table_cell_padding_vertical();
				float minimum_floor = 48.0f + 2.0f * padding_horizontal;

				std::vector<TableColumnBounds> bounds(column_count);
				auto measure_row = [&](const TableRow &row, bool is_header) {
					uint8_t flags = is_header ? INLINE_STYLE_BOLD : INLINE_STYLE_NONE;
					for (size_t column_index = 0; column_index < row.cells.size(); ++column_index) {
						const TableCell &cell = row.cells[column_index];
						// Measure each cell twice: unwrapped (width = -1) for natural max
						// width, and word-wrapped at width = 1 to estimate the longest
						// unbreakable word (capped at 160 px so a single URL cannot starve
						// sibling columns).
						Vector2 unwrapped_size = measure_inlines(
								cell.inlines,
								-1.0f,
								base_font_size,
								flags,
								TextServer::BREAK_MANDATORY);
						Vector2 word_wrapped_size = measure_inlines(
								cell.inlines,
								1.0f,
								base_font_size,
								flags,
								TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND);

						float cell_maximum = unwrapped_size.x + 2.0f * padding_horizontal;
						float longest_word = std::min(word_wrapped_size.x, 160.0f);
						float cell_minimum = std::min(cell_maximum, std::max(minimum_floor, longest_word + 2.0f * padding_horizontal));

						bounds[column_index].maximum_width = std::max(bounds[column_index].maximum_width, cell_maximum);
						bounds[column_index].minimum_width = std::max(bounds[column_index].minimum_width, cell_minimum);
					}
				};

				for (const TableRow &row : block.table_header_rows) {
					measure_row(row, true);
				}
				for (const TableRow &row : block.table_body_rows) {
					measure_row(row, false);
				}

				for (TableColumnBounds &column_bound : bounds) {
					column_bound.maximum_width = std::max(column_bound.maximum_width, 24.0f + 2.0f * padding_horizontal);
					column_bound.minimum_width = std::clamp(column_bound.minimum_width, 24.0f, column_bound.maximum_width);
				}

				TableSizingResult sizing = compute_table_column_widths(bounds, clamped_width, padding_horizontal, false);
				if (sizing.columns.empty()) {
					return start_y;
				}

				int threshold = running_character_count;
				size_t grid_primitive_index = result.primitives.size();
				PaintPrimitive grid_primitive;
				grid_primitive.kind = PaintPrimitiveKind::TABLE_GRID;
				grid_primitive.border_color = theme->get_table_border_color();
				grid_primitive.border_width = 1.0f;
				grid_primitive.corner_radius = theme->get_corner_radius();
				grid_primitive.style_box = theme->get_table_style();
				grid_primitive.table_header_row_count = static_cast<int>(block.table_header_rows.size());
				grid_primitive.table_header_bg = theme->get_table_header_bg_color();
				grid_primitive.table_alt_row_bg = theme->get_table_alt_row_bg_color();
				grid_primitive.document_char_threshold = threshold;

				std::vector<float> column_offsets;
				column_offsets.reserve(sizing.columns.size() + 1);
				float cumulative_x = 0.0f;
				column_offsets.push_back(cumulative_x);
				for (const TableColumnLayout &column_layout : sizing.columns) {
					cumulative_x += column_layout.column_width;
					column_offsets.push_back(cumulative_x);
				}
				grid_primitive.table_column_offsets = column_offsets;
				result.primitives.push_back(grid_primitive);

				std::vector<float> row_offsets;
				row_offsets.push_back(0.0f);
				float current_y = start_y;

				auto layout_row = [&](const TableRow &row, bool is_header) {
					uint8_t flags = is_header ? INLINE_STYLE_BOLD : INLINE_STYLE_NONE;
					Color cell_color = is_header ? theme->get_heading_color() : text_color;
					float maximum_cell_height = static_cast<float>(base_font_size);

					for (size_t column_index = 0; column_index < sizing.columns.size(); ++column_index) {
						if (column_index >= row.cells.size()) {
							continue;
						}
						const TableCell &cell = row.cells[column_index];
						const TableColumnLayout &column_layout = sizing.columns[column_index];
						TableAlignment effective_alignment = cell.alignment;
						if (effective_alignment == TableAlignment::DEFAULT && column_index < block.table_alignments.size()) {
							effective_alignment = block.table_alignments[column_index];
						}

						float cell_x = origin_x + column_offsets[column_index] + column_layout.effective_padding_horizontal;
						float cell_y = current_y + padding_vertical;
						int cell_slot = add_paragraph(
								cell.inlines,
								Vector2(cell_x, cell_y),
								std::max(1.0f, column_layout.text_width),
								base_font_size,
								cell_color,
								flags,
								to_horizontal_alignment(effective_alignment),
								true,
								true);
						maximum_cell_height = std::max(maximum_cell_height, result.paragraphs[cell_slot].size.y);
					}

					float row_height = maximum_cell_height + 2.0f * padding_vertical;
					current_y += row_height;
					row_offsets.push_back(current_y - start_y);
				};

				for (const TableRow &row : block.table_header_rows) {
					layout_row(row, true);
				}
				for (const TableRow &row : block.table_body_rows) {
					layout_row(row, false);
				}

				PaintPrimitive &stored_grid = result.primitives[grid_primitive_index];
				stored_grid.rectangle = Rect2(origin_x, start_y, sizing.total_width, current_y - start_y);
				stored_grid.table_row_offsets = std::move(row_offsets);
				return current_y;
			}

			case BlockKind::THEMATIC_BREAK: {
				float margin_vertical = std::max(4.0f, static_cast<float>(base_font_size) * 0.4f);
				float thickness = theme->get_rule_thickness();
				PaintPrimitive rule;
				rule.kind = PaintPrimitiveKind::HORIZONTAL_RULE;
				rule.rectangle = Rect2(origin_x, start_y + margin_vertical, clamped_width, thickness);
				rule.fill_color = theme->get_rule_color();
				rule.document_char_threshold = running_character_count;
				result.primitives.push_back(rule);
				return start_y + margin_vertical * 2.0f + thickness;
			}

			case BlockKind::IMAGE_BLOCK: {
				String image_uri = utf8_to_godot_string(block.image_source);
				String link_url = utf8_to_godot_string(block.image_link_url);
				Ref<Texture2D> texture = resolve_image_uri(image_uri);

				if (texture.is_valid() && texture->get_width() > 0 && texture->get_height() > 0) {
					float texture_width = static_cast<float>(texture->get_width());
					float texture_height = static_cast<float>(texture->get_height());
					float maximum_height = theme->get_max_image_height();
					float scale = 1.0f;
					if (texture_width > clamped_width) {
						scale = std::min(scale, clamped_width / texture_width);
					}
					if (texture_height * scale > maximum_height) {
						scale = std::min(scale, maximum_height / texture_height);
					}
					Vector2 display_size(std::max(1.0f, texture_width * scale), std::max(1.0f, texture_height * scale));

					PaintPrimitive image_primitive;
					image_primitive.kind = PaintPrimitiveKind::BLOCK_IMAGE;
					image_primitive.rectangle = Rect2(Vector2(origin_x, start_y), display_size);
					image_primitive.texture = texture;
					image_primitive.document_char_threshold = running_character_count;
					result.primitives.push_back(image_primitive);

					if (!link_url.is_empty()) {
						LinkHitBox hit_box;
						hit_box.rectangle = image_primitive.rectangle;
						hit_box.url = link_url;
						result.link_hit_boxes.push_back(hit_box);
					}

					return start_y + display_size.y;
				}

				String fallback_text = utf8_to_godot_string(block.image_alt);
				if (fallback_text.is_empty()) {
					fallback_text = !image_uri.is_empty() ? image_uri : String("[image]");
				}
				InlineNode fallback_inline;
				fallback_inline.kind = InlineKind::TEXT;
				fallback_inline.text = std::string(fallback_text.utf8().get_data());
				fallback_inline.style_flags = INLINE_STYLE_ITALIC;
				fallback_inline.link_url = block.image_link_url;

				std::vector<InlineNode> fallback_inlines;
				fallback_inlines.push_back(fallback_inline);
				int slot = add_paragraph(
						fallback_inlines,
						Vector2(origin_x, start_y),
						clamped_width,
						base_font_size,
						theme->get_muted_color(),
						INLINE_STYLE_NONE,
						HORIZONTAL_ALIGNMENT_LEFT,
						true,
						true);
				return start_y + result.paragraphs[slot].size.y;
			}
		}

		return start_y;
	}

public:
	explicit LayoutBuilder(const LayoutOptions &layout_options) :
			options(layout_options) {
		theme = options.theme.is_valid() ? options.theme : MarkdownTheme::create_dark();
		fonts.initialize(theme, options.fallback_font);
	}

	LayoutDocument build(const Document &document) {
		float width = std::max(1.0f, options.available_width);
		result.layout_width = width;
		result.total_height = layout_block_list(document.blocks, 0.0f, 0.0f, width, theme->get_text_color(), 0, false);
		result.total_character_count = running_character_count;
		return result;
	}
};

PackedColorArray make_uniform_colors(int count, const Color &color) {
	PackedColorArray colors;
	colors.resize(count);
	for (int index = 0; index < count; ++index) {
		colors.set(index, color);
	}
	return colors;
}

void add_circle_outline(
		RenderingServer *rendering_server,
		RID canvas_item,
		const Vector2 &center,
		float radius,
		int segments,
		const Color &color,
		float width) {
	PackedVector2Array points;
	points.resize(segments + 1);
	for (int index = 0; index <= segments; ++index) {
		float angle = (static_cast<float>(index) / static_cast<float>(segments)) * 2.0f * PI_VALUE;
		points.set(index, center + Vector2(std::cos(angle) * radius, std::sin(angle) * radius));
	}
	rendering_server->canvas_item_add_polyline(canvas_item, points, make_uniform_colors(segments + 1, color), width, true);
}

} // namespace

LayoutDocument build_layout(const Document &document, const LayoutOptions &options) {
	LayoutBuilder builder(options);
	return builder.build(document);
}

void draw_rounded_panel(
		RID canvas_item,
		const Rect2 &rectangle,
		const Color &fill_color,
		const Color &border_color,
		float border_width,
		float corner_radius) {
	if (!canvas_item.is_valid() || rectangle.size.x <= 0.0f || rectangle.size.y <= 0.0f) {
		return;
	}
	bool draw_fill = fill_color.a > 0.0f;
	bool draw_border = border_width > 0.0f && border_color.a > 0.0f;
	if (!draw_fill && !draw_border) {
		return;
	}

	// Delegate to an ephemeral StyleBoxFlat so procedural panels inherit
	// Godot's anti-aliased rounded-corner tessellation and border alignment.
	Ref<StyleBoxFlat> style_box;
	style_box.instantiate();
	style_box->set_draw_center(draw_fill);
	if (draw_fill) {
		style_box->set_bg_color(fill_color);
	}
	if (draw_border) {
		style_box->set_border_color(border_color);
		style_box->set_border_width_all(std::max(1, static_cast<int32_t>(std::round(border_width))));
	}
	int32_t radius = std::max(0, static_cast<int32_t>(std::round(corner_radius)));
	style_box->set_corner_radius_all(radius);
	style_box->set_corner_detail(8);
	style_box->set_anti_aliased(true);
	style_box->draw(canvas_item, rectangle);
}

void draw_task_checkbox(
		RID canvas_item,
		const Rect2 &box_rectangle,
		bool checked,
		const Color &checked_color,
		const Color &unchecked_color) {
	if (!canvas_item.is_valid() || box_rectangle.size.x <= 0.0f || box_rectangle.size.y <= 0.0f) {
		return;
	}
	float corner_radius = std::min(4.0f, box_rectangle.size.x * 0.24f);

	if (checked) {
		draw_rounded_panel(canvas_item, box_rectangle, checked_color, Color(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, corner_radius);

		RenderingServer *rendering_server = RenderingServer::get_singleton();
		if (rendering_server == nullptr) {
			return;
		}
		PackedVector2Array checkmark;
		checkmark.resize(3);
		checkmark.set(0, box_rectangle.position + Vector2(box_rectangle.size.x * 0.23f, box_rectangle.size.y * 0.52f));
		checkmark.set(1, box_rectangle.position + Vector2(box_rectangle.size.x * 0.43f, box_rectangle.size.y * 0.72f));
		checkmark.set(2, box_rectangle.position + Vector2(box_rectangle.size.x * 0.78f, box_rectangle.size.y * 0.30f));

		float stroke_width = std::max(1.8f, box_rectangle.size.x * 0.13f);
		rendering_server->canvas_item_add_polyline(
				canvas_item,
				checkmark,
				make_uniform_colors(3, Color(1.0f, 1.0f, 1.0f, 1.0f)),
				stroke_width,
				true);
	} else {
		float border_width = std::max(1.5f, box_rectangle.size.x * 0.11f);
		draw_rounded_panel(canvas_item, box_rectangle, Color(0.0f, 0.0f, 0.0f, 0.0f), unchecked_color, border_width, corner_radius);
	}
}

void draw_procedural_callout_icon(
		RID canvas_item,
		const Rect2 &icon_rectangle,
		const String &callout_kind,
		const Color &color) {
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	if (rendering_server == nullptr || !canvas_item.is_valid()) {
		return;
	}
	if (icon_rectangle.size.x <= 0.0f || icon_rectangle.size.y <= 0.0f) {
		return;
	}

	auto point = [&](float normalized_x, float normalized_y) -> Vector2 {
		return icon_rectangle.position + Vector2(normalized_x * icon_rectangle.size.x, normalized_y * icon_rectangle.size.y);
	};

	float size = std::min(icon_rectangle.size.x, icon_rectangle.size.y);
	float stroke_width = std::max(1.4f, size * 0.105f);
	float dot_radius = std::max(1.1f, size * 0.075f);
	String normalized_kind = callout_kind.strip_edges().to_upper();

	if (normalized_kind == "NOTE") {
		add_circle_outline(rendering_server, canvas_item, point(0.5f, 0.5f), size * 0.43f, 24, color, stroke_width);
		rendering_server->canvas_item_add_circle(canvas_item, point(0.5f, 0.31f), dot_radius, color, true);
		rendering_server->canvas_item_add_line(canvas_item, point(0.5f, 0.45f), point(0.5f, 0.72f), color, stroke_width * 1.15f, true);
		return;
	}

	if (normalized_kind == "TIP") {
		PackedVector2Array bulb;
		constexpr int arc_segments = 16;
		bulb.resize(arc_segments + 3);
		bulb.set(0, point(0.37f, 0.67f));
		for (int index = 0; index <= arc_segments; ++index) {
			float start_angle = 0.75f * PI_VALUE;
			float end_angle = 2.25f * PI_VALUE;
			float angle = start_angle + (end_angle - start_angle) * (static_cast<float>(index) / static_cast<float>(arc_segments));
			Vector2 vertex = point(0.5f, 0.40f) + Vector2(std::cos(angle) * size * 0.28f, std::sin(angle) * size * 0.28f);
			bulb.set(index + 1, vertex);
		}
		bulb.set(arc_segments + 2, point(0.63f, 0.67f));
		rendering_server->canvas_item_add_polyline(canvas_item, bulb, make_uniform_colors(bulb.size(), color), stroke_width, true);
		rendering_server->canvas_item_add_line(canvas_item, point(0.37f, 0.76f), point(0.63f, 0.76f), color, stroke_width, true);
		rendering_server->canvas_item_add_line(canvas_item, point(0.42f, 0.88f), point(0.58f, 0.88f), color, stroke_width, true);
		return;
	}

	if (normalized_kind == "IMPORTANT") {
		PackedVector2Array bubble;
		bubble.resize(8);
		bubble.set(0, point(0.14f, 0.14f));
		bubble.set(1, point(0.86f, 0.14f));
		bubble.set(2, point(0.86f, 0.68f));
		bubble.set(3, point(0.50f, 0.68f));
		bubble.set(4, point(0.29f, 0.88f));
		bubble.set(5, point(0.29f, 0.68f));
		bubble.set(6, point(0.14f, 0.68f));
		bubble.set(7, point(0.14f, 0.14f));
		rendering_server->canvas_item_add_polyline(canvas_item, bubble, make_uniform_colors(bubble.size(), color), stroke_width, true);
		rendering_server->canvas_item_add_line(canvas_item, point(0.50f, 0.26f), point(0.50f, 0.46f), color, stroke_width * 1.15f, true);
		rendering_server->canvas_item_add_circle(canvas_item, point(0.50f, 0.56f), dot_radius, color, true);
		return;
	}

	if (normalized_kind == "WARNING") {
		PackedVector2Array triangle;
		triangle.resize(4);
		triangle.set(0, point(0.50f, 0.11f));
		triangle.set(1, point(0.91f, 0.85f));
		triangle.set(2, point(0.09f, 0.85f));
		triangle.set(3, point(0.50f, 0.11f));
		rendering_server->canvas_item_add_polyline(canvas_item, triangle, make_uniform_colors(triangle.size(), color), stroke_width, true);
		rendering_server->canvas_item_add_line(canvas_item, point(0.50f, 0.37f), point(0.50f, 0.60f), color, stroke_width * 1.15f, true);
		rendering_server->canvas_item_add_circle(canvas_item, point(0.50f, 0.72f), dot_radius, color, true);
		return;
	}

	if (normalized_kind == "CAUTION") {
		PackedVector2Array octagon;
		octagon.resize(9);
		octagon.set(0, point(0.32f, 0.10f));
		octagon.set(1, point(0.68f, 0.10f));
		octagon.set(2, point(0.90f, 0.32f));
		octagon.set(3, point(0.90f, 0.68f));
		octagon.set(4, point(0.68f, 0.90f));
		octagon.set(5, point(0.32f, 0.90f));
		octagon.set(6, point(0.10f, 0.68f));
		octagon.set(7, point(0.10f, 0.32f));
		octagon.set(8, point(0.32f, 0.10f));
		rendering_server->canvas_item_add_polyline(canvas_item, octagon, make_uniform_colors(octagon.size(), color), stroke_width, true);
		rendering_server->canvas_item_add_line(canvas_item, point(0.50f, 0.28f), point(0.50f, 0.54f), color, stroke_width * 1.15f, true);
		rendering_server->canvas_item_add_circle(canvas_item, point(0.50f, 0.68f), dot_radius, color, true);
		return;
	}

	// Custom fallback badge: diamond outline with center gem dot
	PackedVector2Array diamond;
	diamond.resize(5);
	diamond.set(0, point(0.50f, 0.10f));
	diamond.set(1, point(0.90f, 0.50f));
	diamond.set(2, point(0.50f, 0.90f));
	diamond.set(3, point(0.10f, 0.50f));
	diamond.set(4, point(0.50f, 0.10f));
	rendering_server->canvas_item_add_polyline(canvas_item, diamond, make_uniform_colors(diamond.size(), color), stroke_width, true);
	rendering_server->canvas_item_add_circle(canvas_item, point(0.50f, 0.50f), dot_radius * 1.35f, color, true);
}

void draw_laid_out_paragraph(
		RID canvas_item,
		const LaidOutParagraph &laid_out,
		const Vector2 &draw_offset,
		const Ref<MarkdownTheme> &theme,
		const String &hovered_link_url,
		int visible_characters) {
	if (!canvas_item.is_valid() || laid_out.paragraph.is_null()) {
		return;
	}

	bool has_cutoff = false;
	int local_cutoff = laid_out.local_char_count;
	if (visible_characters >= 0) {
		if (laid_out.document_char_start == laid_out.document_char_end) {
			if (visible_characters <= laid_out.document_char_start) {
				return;
			}
		} else {
			if (visible_characters <= laid_out.document_char_start) {
				return;
			}
			if (visible_characters < laid_out.document_char_end) {
				has_cutoff = true;
				local_cutoff = visible_characters - laid_out.document_char_start;
			}
		}
	}

	// 1. Inline code pill backgrounds (behind glyphs)
	if (!laid_out.inline_code_pills.empty() && theme.is_valid()) {
		Ref<StyleBox> custom_pill_style = theme->get_inline_code_style();
		Color pill_fill = theme->get_inline_code_bg_color();
		float pill_radius = std::min(theme->get_corner_radius(), 4.0f);

		for (const DecorationRect &pill : laid_out.inline_code_pills) {
			if (has_cutoff && pill.char_start >= local_cutoff) {
				continue;
			}
			Rect2 target_rectangle(pill.rectangle.position + draw_offset, pill.rectangle.size);
			if (has_cutoff && pill.char_end > local_cutoff && pill.char_end > pill.char_start) {
				float reveal_fraction = static_cast<float>(local_cutoff - pill.char_start) / static_cast<float>(pill.char_end - pill.char_start);
				target_rectangle.size.x *= std::clamp(reveal_fraction, 0.0f, 1.0f);
			}
			if (target_rectangle.size.x <= 0.0f) {
				continue;
			}
			if (custom_pill_style.is_valid()) {
				custom_pill_style->draw(canvas_item, target_rectangle);
			} else {
				draw_rounded_panel(canvas_item, target_rectangle, pill_fill, Color(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, pill_radius);
			}
		}
	}

	// 2. Text glyphs
	// Fast path: let TextParagraph submit all glyphs in one call when no
	// per-span coloring, hover highlighting, or typewriter cutoff is needed.
	// Slow path: walk shaped glyphs per line so each glyph gets its span/hover
	// color and stops cleanly at local_cutoff, skipping embedded spacer/image
	// placeholders (GRAPHEME_IS_EMBEDDED_OBJECT).
	if (!laid_out.has_rich_spans && !has_cutoff) {
		laid_out.paragraph->draw(canvas_item, draw_offset, laid_out.base_color);
	} else {
		TextServerManager *manager = TextServerManager::get_singleton();
		Ref<TextServer> text_server = manager != nullptr ? manager->get_primary_interface() : Ref<TextServer>();
		if (text_server.is_valid()) {
			const Ref<TextParagraph> &paragraph = laid_out.paragraph;
			int line_count = paragraph->get_line_count();
			float line_spacing = paragraph->get_line_spacing();
			float line_y = 0.0f;

			for (int line_index = 0; line_index < line_count; ++line_index) {
				RID line_rid = paragraph->get_line_rid(line_index);
				Vector2 line_size = paragraph->get_line_size(line_index);
				float line_ascent = paragraph->get_line_ascent(line_index);
				float line_offset_x = compute_line_offset_x(paragraph, line_size.x);

				Vector2 pen(draw_offset.x + line_offset_x, draw_offset.y + line_y + line_ascent);
				TypedArray<Dictionary> glyphs = text_server->shaped_text_get_glyphs(line_rid);

				for (int glyph_slot = 0; glyph_slot < glyphs.size(); ++glyph_slot) {
					Dictionary glyph = glyphs[glyph_slot];
					int glyph_start = static_cast<int>(glyph["start"]);
					int repeat_count = static_cast<int>(glyph["repeat"]);
					float advance = static_cast<float>(glyph["advance"]);
					int64_t flags = static_cast<int64_t>(glyph["flags"]);
					RID font_rid = glyph["font_rid"];
					int64_t font_size = static_cast<int64_t>(glyph["font_size"]);
					int64_t glyph_index = static_cast<int64_t>(glyph["index"]);
					Vector2 glyph_offset = glyph["offset"];

					bool draw_glyph = (!has_cutoff || glyph_start < local_cutoff) &&
							font_rid.is_valid() &&
							glyph_index != 0 &&
							(flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) == 0;

					if (draw_glyph && repeat_count > 0) {
						Color glyph_color = resolve_glyph_color(laid_out, glyph_start, hovered_link_url, theme);
						for (int repeat_index = 0; repeat_index < repeat_count; ++repeat_index) {
							Vector2 glyph_position = pen + glyph_offset + Vector2(advance * static_cast<float>(repeat_index), 0.0f);
							text_server->font_draw_glyph(font_rid, canvas_item, font_size, glyph_position, glyph_index, glyph_color);
						}
					}

					pen.x += advance * static_cast<float>(repeat_count);
				}

				line_y += line_size.y + line_spacing;
			}
		}
	}

	// 3. Inline images
	for (const InlineImagePlacement &placement : laid_out.inline_images) {
		if (has_cutoff && placement.char_index >= local_cutoff) {
			continue;
		}
		if (placement.texture.is_valid()) {
			Rect2 target_rectangle(placement.rectangle.position + draw_offset, placement.rectangle.size);
			placement.texture->draw_rect(canvas_item, target_rectangle, false);
		}
	}

	// 4. Link underlines and strikethrough lines
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	if (rendering_server == nullptr) {
		return;
	}

	auto draw_decoration_lines = [&](const std::vector<DecorationLine> &lines, bool use_hover_color) {
		for (const DecorationLine &line : lines) {
			if (has_cutoff && line.char_start >= local_cutoff) {
				continue;
			}
			Vector2 from_point = line.start_point + draw_offset;
			Vector2 to_point = line.end_point + draw_offset;
			if (has_cutoff && line.char_end > local_cutoff && line.char_end > line.char_start) {
				float reveal_fraction = static_cast<float>(local_cutoff - line.char_start) / static_cast<float>(line.char_end - line.char_start);
				to_point = from_point.lerp(to_point, std::clamp(reveal_fraction, 0.0f, 1.0f));
			}
			Color line_color = line.color;
			if (use_hover_color && !line.link_url.is_empty() && !hovered_link_url.is_empty() && line.link_url == hovered_link_url && theme.is_valid()) {
				line_color = theme->get_link_hover_color();
			}
			rendering_server->canvas_item_add_line(canvas_item, from_point, to_point, line_color, line.thickness, true);
		}
	};

	draw_decoration_lines(laid_out.link_underlines, true);
	draw_decoration_lines(laid_out.strikethrough_lines, false);
}

} // namespace gdmarkdown
