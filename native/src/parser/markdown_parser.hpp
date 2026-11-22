// Copyright (c) 2026 Cafecito Games LLC
// SPDX-License-Identifier: MIT

#pragma once

#include "parser/document.hpp"

#include <string_view>

namespace gdmarkdown {

// Parses CommonMark + GitHub Flavored Markdown (tables, strikethrough, task
// lists, permissive autolinks) and promotes GitHub/Obsidian alert blockquotes
// into Callout blocks. Raw HTML blocks and spans are safely omitted except for
// `<br>` tags, which become hard line breaks.
Document parse_markdown(std::string_view source);

} // namespace gdmarkdown
