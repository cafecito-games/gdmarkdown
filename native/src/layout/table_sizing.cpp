// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#include "layout/table_sizing.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gdmarkdown {

TableSizingResult compute_table_column_widths(
		const std::vector<TableColumnBounds> &column_bounds,
		float available_width,
		float cell_padding_horizontal,
		bool fill_available_width) {
	TableSizingResult result;
	const size_t column_count = column_bounds.size();
	if (column_count == 0) {
		return result;
	}

	const float safe_padding_horizontal = std::max(0.0f, cell_padding_horizontal);
	std::vector<float> minimum_widths(column_count, 1.0f);
	std::vector<float> maximum_widths(column_count, 1.0f);
	float total_minimum_width = 0.0f;
	float total_maximum_width = 0.0f;

	for (size_t index = 0; index < column_count; ++index) {
		const float clamped_minimum = std::max(1.0f, column_bounds[index].minimum_width);
		const float clamped_maximum = std::max(clamped_minimum, column_bounds[index].maximum_width);
		minimum_widths[index] = clamped_minimum;
		maximum_widths[index] = clamped_maximum;
		total_minimum_width += clamped_minimum;
		total_maximum_width += clamped_maximum;
	}

	std::vector<float> computed_widths(column_count, 0.0f);

	// Four-regime column distribution:
	// 1. Degenerate container (<= 0): collapse all columns to 0.
	// 2. Fits comfortably at unwrapped max widths: keep natural widths (or
	//    proportionally expand to fill_available_width when requested).
	// 3. Fits between min and max totals: distribute surplus above minimums in
	//    proportion to each column's flexibility (max - min).
	// 4. Narrower than total minimums: scale all columns down proportionally so
	//    the table never overflows its container.
	if (available_width <= 0.0f) {
		for (size_t index = 0; index < column_count; ++index) {
			computed_widths[index] = 0.0f;
		}
	} else if (total_maximum_width <= available_width) {
		if (fill_available_width && total_maximum_width < available_width) {
			const float extra_width = available_width - total_maximum_width;
			for (size_t index = 0; index < column_count; ++index) {
				const float weight = maximum_widths[index] / total_maximum_width;
				computed_widths[index] = maximum_widths[index] + extra_width * weight;
			}
		} else {
			computed_widths = maximum_widths;
		}
	} else if (total_minimum_width <= available_width) {
		const float extra_width = available_width - total_minimum_width;
		const float total_flexibility = total_maximum_width - total_minimum_width;
		for (size_t index = 0; index < column_count; ++index) {
			if (total_flexibility > 0.0f) {
				const float column_flexibility = maximum_widths[index] - minimum_widths[index];
				computed_widths[index] = minimum_widths[index] + extra_width * (column_flexibility / total_flexibility);
			} else {
				computed_widths[index] = minimum_widths[index];
			}
		}
	} else {
		const float scale_factor = available_width / total_minimum_width;
		for (size_t index = 0; index < column_count; ++index) {
			computed_widths[index] = minimum_widths[index] * scale_factor;
		}
	}

	result.columns.resize(column_count);
	result.total_width = 0.0f;

	// Godot's TextParagraph treats width <= 0 as unconstrained (no wrapping).
	// Clamp horizontal cell padding first so text_width always stays >= 1.0f.
	for (size_t index = 0; index < column_count; ++index) {
		const float column_width = std::max(0.0f, computed_widths[index]);
		const float effective_padding_horizontal = std::min(
				safe_padding_horizontal,
				std::max(0.0f, (column_width - 1.0f) * 0.5f));
		const float text_width = std::max(1.0f, column_width - 2.0f * effective_padding_horizontal);

		result.columns[index].column_width = column_width;
		result.columns[index].effective_padding_horizontal = effective_padding_horizontal;
		result.columns[index].text_width = text_width;
		result.total_width += column_width;
	}

	return result;
}

} // namespace gdmarkdown
