// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#pragma once

#include <vector>

namespace gdmarkdown {

// Measured intrinsic width constraints for a single table column (in pixels,
// including horizontal cell padding).
struct TableColumnBounds {
	float minimum_width = 0.0f;
	float maximum_width = 0.0f;
};

// Resolved pixel widths for a single table column after fitting to available_width.
struct TableColumnLayout {
	float column_width = 0.0f;
	// Horizontal cell padding clamped so text_width stays >= 1.0f in narrow layouts.
	float effective_padding_horizontal = 0.0f;
	// Inner wrapping width passed to TextParagraph (>= 1.0f).
	float text_width = 1.0f;
};

// Output of compute_table_column_widths() for an entire table.
struct TableSizingResult {
	std::vector<TableColumnLayout> columns;
	float total_width = 0.0f;
};

// Computes table column widths that fit within available_width while clamping
// horizontal cell padding so every column's inner text_width stays strictly
// positive (>= 1.0f), preventing unconstrained wrapping in narrow containers.
TableSizingResult compute_table_column_widths(
		const std::vector<TableColumnBounds> &column_bounds,
		float available_width,
		float cell_padding_horizontal,
		bool fill_available_width = false);

} // namespace gdmarkdown
