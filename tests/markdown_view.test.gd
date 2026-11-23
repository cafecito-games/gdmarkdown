# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends GDTest
## Integration tests for [MarkdownView], [MarkdownTheme], and
## [MarkdownCalloutStyle].
##
## Verifies the zero-minimum-width sizing contract, signal emission, live theme
## and callout propagation, task-list lifecycle and click handling, link
## hover/click/exit behavior, typewriter reveal synchronization, code-block
## wrapping, narrow-table stability, [member MarkdownView.image_resolver]
## caching, and kitchen-sink document rendering.

const KITCHEN_SINK_PATH: String = "res://tests/fixtures/kitchen_sink.md"


## Returns the test suite name reported by [code]gdtest[/code].
func get_suite_name() -> String:
	return "MarkdownView"


## Verifies initial defaults and confirms clearing [member MarkdownView.markdown]
## resets plain text, character counts, and minimum layout height back to zero.
func test_defaults_and_empty_document_handling() -> void:
	var view: MarkdownView = _mount_view(400.0)
	await sleep()

	expect_empty(view.markdown)
	expect_empty(view.get_plain_text())
	expect_equal(view.get_total_character_count(), 0)
	expect_equal(view.get_task_count(), 0)
	expect_equal(view.visible_characters, -1)
	expect_approx(view.visible_ratio, 1.0, 0.001)
	expect_false(view.interactive_tasks)
	expect_true(view.code_block_wrap)
	expect_equal(view.get_combined_minimum_size().x, 0.0)
	expect_equal(view.get_combined_minimum_size().y, 0.0)

	view.markdown = "Temporary content"
	expect_true(view.get_total_character_count() > 0)
	expect_true(view.get_combined_minimum_size().y > 0.0)

	view.markdown = ""
	expect_empty(view.get_plain_text())
	expect_equal(view.get_total_character_count(), 0)
	expect_equal(view.get_combined_minimum_size().y, 0.0)


## Verifies Markdown inline formatting is stripped in
## [method MarkdownView.get_plain_text] while preserving total character accounting.
func test_markdown_assignment_extracts_plain_text_and_character_count() -> void:
	var view: MarkdownView = autofree(MarkdownView.new())

	view.markdown = "# Hello\n\nSome **bold** and *italic* `code` text."

	expect_equal(view.markdown, "# Hello\n\nSome **bold** and *italic* `code` text.")
	var plain_text: String = view.get_plain_text()
	expect_contains(plain_text, "Hello")
	expect_contains(plain_text, "Some bold and italic code text.")
	expect_does_not_contain(plain_text, "**")
	expect_does_not_contain(plain_text, "`")
	expect_true(view.get_total_character_count() >= plain_text.length())


## Verifies [MarkdownView] always reports a zero minimum width so containers such
## as [ScrollContainer] can both shrink and expand horizontally without lockup.
func test_control_sizing_contract_wraps_and_shrinks_freely() -> void:
	var view: MarkdownView = _mount_view(400.0)
	view.markdown = (
			"This is a moderately long paragraph of prose designed to wrap across multiple "
			+ "lines whenever the view width shrinks inside a responsive container or pane."
	)
	await sleep()

	var wide_minimum: Vector2 = view.get_combined_minimum_size()
	expect_equal(wide_minimum.x, 0.0)
	expect_true(wide_minimum.y > 0.0)

	view.size = Vector2(140.0, wide_minimum.y)
	await sleep()
	var narrow_minimum: Vector2 = view.get_combined_minimum_size()
	expect_equal(narrow_minimum.x, 0.0)
	expect_true(narrow_minimum.y > wide_minimum.y)

	# Widening again must reduce height without locking to the previous width.
	view.size = Vector2(500.0, narrow_minimum.y)
	await sleep()
	var expanded_minimum: Vector2 = view.get_combined_minimum_size()
	expect_equal(expanded_minimum.x, 0.0)
	expect_true(expanded_minimum.y < narrow_minimum.y)


## Verifies [signal MarkdownView.layout_updated] fires on content assignment,
## theme swaps, live theme mutations, and control width changes.
func test_layout_updated_signal_fires_on_content_theme_and_resize() -> void:
	var view: MarkdownView = _mount_view(320.0)
	await sleep()
	var updates: GDTestSignalProbe = watch_signal(view.layout_updated)

	view.markdown = "First paragraph."
	expect_true(updates.emission_count() >= 1)
	var after_markdown: int = updates.emission_count()

	var theme: MarkdownTheme = MarkdownTheme.create_light()
	view.markdown_theme = theme
	expect_true(updates.emission_count() > after_markdown)
	var after_theme: int = updates.emission_count()

	theme.base_font_size = 24
	expect_true(updates.emission_count() > after_theme)
	var after_theme_mutation: int = updates.emission_count()

	view.size = Vector2(200.0, view.get_combined_minimum_size().y)
	await sleep()
	expect_true(updates.emission_count() > after_theme_mutation)


## Verifies live [MarkdownTheme] and nested [MarkdownCalloutStyle] edits trigger
## relayout, and detaching an old theme stops forwarding its signals.
func test_theme_live_updates_and_callout_propagation() -> void:
	var view: MarkdownView = _mount_view(360.0)
	var first_theme: MarkdownTheme = MarkdownTheme.create_dark()
	var second_theme: MarkdownTheme = MarkdownTheme.create_light()
	var quest_style: MarkdownCalloutStyle = MarkdownCalloutStyle.create(
			"Quest",
			Color(0.95, 0.72, 0.25, 1.0)
	)
	first_theme.set_callout_style("quest", quest_style)
	view.markdown_theme = first_theme
	view.markdown = "> [!QUEST]\n> Find the hidden shrine.\n\nLine one\n\nLine two"
	await sleep()

	var initial_height: float = view.get_combined_minimum_size().y
	first_theme.base_font_size = 32
	var enlarged_height: float = view.get_combined_minimum_size().y
	expect_true(enlarged_height > initial_height)

	var callout_updates: GDTestSignalProbe = watch_signal(view.layout_updated)
	quest_style.title = "Main Quest"
	expect_equal(callout_updates.emission_count(), 1)
	expect_contains(view.get_plain_text(), "Main Quest")

	view.markdown_theme = second_theme
	var stale_updates: GDTestSignalProbe = watch_signal(view.layout_updated)
	first_theme.base_font_size = 12
	expect_equal(stale_updates.emission_count(), 0)


## Verifies task count queries, programmatic toggling, and safe no-op behavior
## for out-of-range task indices.
func test_task_list_query_programmatic_toggle_and_bounds_safety() -> void:
	var view: MarkdownView = autofree(MarkdownView.new())
	view.markdown = "- [x] Done\n- [ ] Pending\n- [X] Also done"

	expect_equal(view.get_task_count(), 3)
	expect_true(view.is_task_checked(0))
	expect_false(view.is_task_checked(1))
	expect_true(view.is_task_checked(2))

	view.set_task_checked(1, true)
	expect_true(view.is_task_checked(1))
	view.set_task_checked(0, false)
	expect_false(view.is_task_checked(0))

	# Out-of-range indices must be handled safely without errors.
	expect_false(view.is_task_checked(-1))
	expect_false(view.is_task_checked(99))
	view.set_task_checked(-1, true)
	view.set_task_checked(99, true)
	expect_equal(view.get_task_count(), 3)


## Verifies replacing [member MarkdownView.markdown] clears toggled runtime task
## states so previous checkboxes never leak into a new document.
func test_task_states_reset_when_document_is_replaced() -> void:
	var view: MarkdownView = autofree(MarkdownView.new())
	view.markdown = "- [ ] Original task\n- [ ] Second task"
	expect_equal(view.get_task_count(), 2)
	expect_false(view.is_task_checked(0))

	view.set_task_checked(0, true)
	expect_true(view.is_task_checked(0))

	# Replacing markdown must clear toggled runtime state from the prior document.
	view.markdown = "- [ ] Fresh unchecked task"
	expect_equal(view.get_task_count(), 1)
	expect_false(view.is_task_checked(0))

	# Replacing with a non-task document must reset task count to zero.
	view.markdown = "Just a plain paragraph."
	expect_equal(view.get_task_count(), 0)
	expect_false(view.is_task_checked(0))


## Verifies clicking a task checkbox toggles state and emits
## [signal MarkdownView.task_toggled] only when
## [member MarkdownView.interactive_tasks] is enabled.
func test_interactive_tasks_click_toggles_and_respects_flag() -> void:
	var view: MarkdownView = _mount_view(360.0)
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	view.markdown_theme = theme
	view.markdown = "- [ ] Clickable task"
	await sleep()

	var toggled: GDTestSignalProbe = watch_signal(view.task_toggled)
	var checkbox_position: Vector2 = _first_task_checkbox_center(theme)

	# With interactive_tasks == false, clicking must not toggle or emit.
	view.interactive_tasks = false
	_send_left_click(view, checkbox_position)
	expect_false(view.is_task_checked(0))
	expect_equal(toggled.emission_count(), 0)

	# With interactive_tasks == true, clicking toggles and emits task_toggled.
	view.interactive_tasks = true
	_send_left_click(view, checkbox_position)
	expect_true(view.is_task_checked(0))
	expect_equal(toggled.emission_count(), 1)
	expect_equal(toggled.arguments_at(0), [0, true])

	_send_left_click(view, checkbox_position)
	expect_false(view.is_task_checked(0))
	expect_equal(toggled.emission_count(), 2)
	expect_equal(toggled.arguments_at(1), [0, false])


## Verifies link hover and click hit-testing plus idempotent hover reset on
## [constant Control.NOTIFICATION_MOUSE_EXIT].
func test_link_hover_click_and_mouse_exit() -> void:
	var view: MarkdownView = _mount_view(360.0)
	view.markdown = "[Example Link](https://example.com/docs)"
	await sleep()

	var hovered: GDTestSignalProbe = watch_signal(view.link_hovered)
	var clicked: GDTestSignalProbe = watch_signal(view.link_clicked)
	var link_position: Vector2 = Vector2(14.0, 8.0)

	_send_mouse_motion(view, link_position)
	expect_equal(hovered.emission_count(), 1)
	expect_equal(hovered.arguments_at(0), ["https://example.com/docs"])

	_send_left_click(view, link_position)
	expect_equal(clicked.emission_count(), 1)
	expect_equal(clicked.arguments_at(0), ["https://example.com/docs"])

	# Exiting the control clears hovered state cleanly and emits empty URL.
	view.notification(Control.NOTIFICATION_MOUSE_EXIT)
	expect_equal(hovered.emission_count(), 2)
	expect_equal(hovered.arguments_at(1), [""])

	# A second mouse exit when nothing is hovered must be a safe no-op.
	view.notification(Control.NOTIFICATION_MOUSE_EXIT)
	expect_equal(hovered.emission_count(), 2)


## Verifies two-way synchronization and clamping between
## [member MarkdownView.visible_characters] and
## [member MarkdownView.visible_ratio], including document length changes.
func test_typewriter_reveal_synchronization_and_clamping() -> void:
	var view: MarkdownView = autofree(MarkdownView.new())
	view.markdown = "0123456789"
	var total: int = view.get_total_character_count()
	expect_equal(total, 10)

	# Full reveal defaults: -1 <-> 1.0.
	expect_equal(view.visible_characters, -1)
	expect_approx(view.visible_ratio, 1.0, 0.001)

	# Zero reveal: 0 <-> 0.0.
	view.visible_characters = 0
	expect_equal(view.visible_characters, 0)
	expect_approx(view.visible_ratio, 0.0, 0.001)

	# Mid-character count updates ratio; mid-ratio updates character count.
	view.visible_characters = 4
	expect_equal(view.visible_characters, 4)
	expect_approx(view.visible_ratio, 0.4, 0.001)

	view.visible_ratio = 0.5
	expect_approx(view.visible_ratio, 0.5, 0.001)
	expect_equal(view.visible_characters, 5)

	# Changing markdown recomputes visible_ratio against the new document length.
	view.markdown = "01234567890123456789"
	expect_equal(view.visible_characters, 5)
	expect_approx(view.visible_ratio, 0.25, 0.001)

	# Clamping out-of-range ratios and negative character counts.
	view.visible_ratio = -0.75
	expect_approx(view.visible_ratio, 0.0, 0.001)
	expect_equal(view.visible_characters, 0)

	view.visible_ratio = 2.5
	expect_approx(view.visible_ratio, 1.0, 0.001)
	expect_equal(view.visible_characters, -1)

	view.visible_characters = 3
	view.visible_characters = -42
	expect_equal(view.visible_characters, -1)
	expect_approx(view.visible_ratio, 1.0, 0.001)


## Verifies disabling [member MarkdownView.code_block_wrap] keeps long lines on
## a single horizontally scrollable row, reducing vertical height.
func test_code_block_wrap_toggle_changes_height_for_long_line() -> void:
	var view: MarkdownView = _mount_view(180.0)
	var long_line: String = "var long_identifier_value = " + "1234567890".repeat(8)
	view.markdown = "```gdscript\n" + long_line + "\n```"
	await sleep()

	view.code_block_wrap = true
	var wrapped_height: float = view.get_combined_minimum_size().y

	view.code_block_wrap = false
	var unwrapped_height: float = view.get_combined_minimum_size().y

	expect_true(wrapped_height > unwrapped_height)
	expect_true(unwrapped_height > 0.0)


## Verifies a deliberate trailing blank line inside a fenced code block is
## preserved in layout rather than stripped a second time.
func test_code_block_preserves_trailing_blank_line() -> void:
	var view: MarkdownView = _mount_view(360.0)
	view.markdown = "```\nline one\n```"
	await sleep()
	var single_line_height: float = view.get_combined_minimum_size().y

	view.markdown = "```\nline one\n\n```"
	await sleep()
	var trailing_blank_height: float = view.get_combined_minimum_size().y

	expect_true(trailing_blank_height > single_line_height)


## Verifies linked inline images on a second line are positioned and hit-tested
## on that second line rather than overlapping the first line.
func test_multiline_linked_inline_image_hit_testing() -> void:
	var view: MarkdownView = _mount_view(360.0)
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	view.markdown_theme = theme
	var icon_texture: ImageTexture = _make_solid_texture(24, 24, Color.GOLD)
	view.image_resolver = func(uri: String) -> Texture2D:
		if uri == "icon://badge":
			return icon_texture
		return null
	view.markdown = "First line<br>[![Badge](icon://badge)](https://example.com/line2)"
	await sleep()

	var clicked: GDTestSignalProbe = watch_signal(view.link_clicked)

	# Clicking the first line must not trigger the second-line linked image.
	_send_left_click(view, Vector2(10.0, 8.0))
	expect_equal(clicked.emission_count(), 0)

	# Clicking the second line hits the linked inline image at its paragraph-local y.
	var second_line_y: float = float(theme.base_font_size) + theme.line_spacing + 10.0
	_send_left_click(view, Vector2(10.0, second_line_y))
	expect_equal(clicked.emission_count(), 1)
	expect_equal(clicked.arguments_at(0), ["https://example.com/line2"])


## Verifies extreme narrow-width tables clamp cell padding so laid-out height
## stays positive and finite without NaN or unconstrained wrapping.
func test_narrow_table_layout_remains_finite_and_positive() -> void:
	var view: MarkdownView = _mount_view(30.0)
	view.markdown = (
			"| Column Alpha | Column Beta | Column Gamma |\n"
			+ "| :--- | :---: | ---: |\n"
			+ "| Supercalifragilistic | 123456789 | RightAlignedValue |\n"
			+ "| Row 2 | Value | End |"
	)
	await sleep()

	var minimum_size: Vector2 = view.get_combined_minimum_size()
	expect_equal(minimum_size.x, 0.0)
	expect_false(is_nan(minimum_size.y))
	expect_false(is_inf(minimum_size.y))
	expect_true(minimum_size.y > 0.0)


## Verifies [member MarkdownView.image_resolver] memoizes resolved textures
## across re-layouts and re-invokes after [method MarkdownView.clear_image_cache].
func test_inline_image_resolver_caches_and_clear_cache_reinvokes() -> void:
	var view: MarkdownView = _mount_view(360.0)
	var requested_uris: Array[String] = []
	var gold_texture: ImageTexture = _make_solid_texture(16, 16, Color(1.0, 0.85, 0.2, 1.0))

	view.image_resolver = func(uri: String) -> Texture2D:
		requested_uris.append(uri)
		if uri == "icon://gold":
			return gold_texture
		return null

	view.markdown = "Earned 50 ![gold](icon://gold) coins!"
	await sleep()

	expect_equal(requested_uris.size(), 1)
	expect_equal(requested_uris[0], "icon://gold")
	expect_true(view.get_combined_minimum_size().y > 0.0)

	# Re-layouts (resize or theme change) must reuse the cached texture.
	view.size = Vector2(220.0, view.get_combined_minimum_size().y)
	await sleep()
	expect_equal(requested_uris.size(), 1)

	# Clearing the image cache must re-invoke image_resolver on re-layout.
	view.clear_image_cache()
	expect_equal(requested_uris.size(), 2)
	expect_equal(requested_uris[1], "icon://gold")


## Verifies standalone block images scale down to honor
## [member MarkdownTheme.max_image_height].
func test_block_image_resolver_respects_max_image_height() -> void:
	var view: MarkdownView = _mount_view(400.0)
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	theme.max_image_height = 64.0
	view.markdown_theme = theme

	var tall_texture: ImageTexture = _make_solid_texture(200, 400, Color(0.2, 0.5, 0.9, 1.0))
	view.image_resolver = func(uri: String) -> Texture2D:
		if uri == "icon://banner":
			return tall_texture
		return null

	view.markdown = "![Banner](icon://banner)"
	await sleep()

	var height: float = view.get_combined_minimum_size().y
	expect_true(height > 0.0)
	expect_true(height <= theme.max_image_height + 2.0)


## Verifies unresolved inline and block images degrade cleanly to alt text
## without producing NaN heights or layout errors.
func test_unresolved_images_fall_back_cleanly_to_alt_text() -> void:
	var view: MarkdownView = _mount_view(360.0)
	view.image_resolver = func(_uri: String) -> Texture2D:
		return null

	view.markdown = (
			"Inline ![Gold Coin](icon://missing) icon.\n\n"
			+ "![Missing Banner](res://does/not/exist.png)"
	)
	await sleep()

	var minimum_size: Vector2 = view.get_combined_minimum_size()
	expect_false(is_nan(minimum_size.y))
	expect_false(is_inf(minimum_size.y))
	expect_true(minimum_size.y > 0.0)
	expect_contains(view.get_plain_text(), "Gold Coin")


## Verifies all five built-in GitHub alerts, custom titles, and registered game
## callouts render their headers and body copy.
func test_github_alerts_and_custom_callouts_render_titles_and_bodies() -> void:
	var view: MarkdownView = _mount_view(420.0)
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	theme.set_callout_style(
			"quest",
			MarkdownCalloutStyle.create("Active Quest", Color(0.95, 0.72, 0.25, 1.0))
	)
	view.markdown_theme = theme
	view.markdown = (
			"> [!NOTE]\n> Standard note body.\n\n"
			+ "> [!TIP] Custom Tip Title\n> Helpful tip body.\n\n"
			+ "> [!IMPORTANT]\n> Important detail.\n\n"
			+ "> [!WARNING]\n> Watch out.\n\n"
			+ "> [!CAUTION]\n> Dangerous action.\n\n"
			+ "> [!QUEST]\n> Defeat the guardian."
	)
	await sleep()

	var plain_text: String = view.get_plain_text()
	expect_contains(plain_text, "Note")
	expect_contains(plain_text, "Custom Tip Title")
	expect_contains(plain_text, "Important")
	expect_contains(plain_text, "Warning")
	expect_contains(plain_text, "Caution")
	expect_contains(plain_text, "Active Quest")
	expect_contains(plain_text, "Defeat the guardian.")
	expect_true(view.get_combined_minimum_size().y > 0.0)


## Verifies inline [code]<br/>[/code] tags produce hard line breaks while all
## other raw HTML tags and blocks are safely omitted.
func test_raw_html_is_stripped_while_br_creates_hard_break() -> void:
	var single_line_view: MarkdownView = _mount_view(400.0)
	single_line_view.markdown = "Alpha Beta"
	await sleep()

	var broken_view: MarkdownView = _mount_view(400.0)
	broken_view.markdown = (
			"Alpha<br/>Beta\n\n"
			+ "<script>alert(1)</script><div>ignored</div>\n\n"
			+ "Visible"
	)
	await sleep()

	var single_height: float = single_line_view.get_combined_minimum_size().y
	var broken_height: float = broken_view.get_combined_minimum_size().y
	expect_true(broken_height > single_height)
	var plain_text: String = broken_view.get_plain_text()
	expect_contains(plain_text, "Alpha")
	expect_contains(plain_text, "Beta")
	expect_contains(plain_text, "Visible")
	expect_does_not_contain(plain_text, "script")
	expect_does_not_contain(plain_text, "ignored")


## Stress-tests all supported Markdown constructs together across theme switches
## and narrow resizes using [constant KITCHEN_SINK_PATH].
func test_kitchen_sink_document_renders_cleanly() -> void:
	var source: String = FileAccess.get_file_as_string(KITCHEN_SINK_PATH)
	expect_not_empty(source)

	var view: MarkdownView = _mount_view(480.0)
	var gold_texture: ImageTexture = _make_solid_texture(16, 16, Color(1.0, 0.85, 0.2, 1.0))
	var banner_texture: ImageTexture = _make_solid_texture(320, 96, Color(0.25, 0.45, 0.85, 1.0))
	view.image_resolver = func(uri: String) -> Texture2D:
		if uri == "icon://gold":
			return gold_texture
		if uri == "icon://banner":
			return banner_texture
		return null

	view.markdown = source
	await sleep()

	var dark_size: Vector2 = view.get_combined_minimum_size()
	expect_equal(dark_size.x, 0.0)
	expect_false(is_nan(dark_size.y))
	expect_false(is_inf(dark_size.y))
	expect_true(dark_size.y > 200.0)
	expect_equal(view.get_task_count(), 3)
	expect_true(view.is_task_checked(0))
	expect_false(view.is_task_checked(1))
	expect_true(view.is_task_checked(2))
	expect_contains(view.get_plain_text(), "Release Notes v2.0")
	expect_contains(view.get_plain_text(), "Dragon's Hoard")

	# Switching to the light theme and resizing remains valid and finite.
	view.markdown_theme = MarkdownTheme.create_light()
	view.size = Vector2(260.0, dark_size.y)
	await sleep()

	var narrow_light_size: Vector2 = view.get_combined_minimum_size()
	expect_equal(narrow_light_size.x, 0.0)
	expect_false(is_nan(narrow_light_size.y))
	expect_false(is_inf(narrow_light_size.y))
	expect_true(narrow_light_size.y > dark_size.y)


## Creates an autofreed [MarkdownView] attached to the scene root at [param width].
func _mount_view(width: float) -> MarkdownView:
	var view: MarkdownView = autofree(MarkdownView.new())
	view.position = Vector2.ZERO
	view.size = Vector2(width, 200.0)
	get_tree().root.add_child(view)
	return view


## A document taller than its [ScrollContainer] is drawn one visible band at a
## time, so the band the last draw painted stops being the visible one as soon
## as the container scrolls. Verifies the view notices and repaints; without it
## scrolling reveals blank space where the rest of the document should be.
func test_scrolling_repaints_the_culled_visible_band() -> void:
	var paragraphs: PackedStringArray = PackedStringArray()
	for index: int in range(80):
		var appended: int = paragraphs.append("Paragraph %d of a long scrolled document." % index)
		expect_equal(appended, OK)
	var scroll: ScrollContainer = _mount_scrolled_view(120.0, "\n\n".join(paragraphs))
	await sleep()

	var view: MarkdownView = scroll.get_child(0)
	var scroll_bar: VScrollBar = scroll.get_v_scroll_bar()
	var max_scroll: float = maxf(0.0, scroll_bar.max_value - scroll_bar.page)
	expect_true(max_scroll > 0.0)

	var redraws: Array[int] = [0]
	var connected: int = view.draw.connect(func() -> void: redraws[0] += 1)
	expect_equal(connected, OK)
	await sleep()
	redraws[0] = 0

	scroll_bar.value = max_scroll
	await sleep()
	expect_true(redraws[0] > 0)


## Scrolling within the band the last draw already painted needs no repaint, so
## the band comparison must not turn every scroll step into a redraw.
func test_scrolling_inside_the_painted_band_does_not_repaint() -> void:
	var scroll: ScrollContainer = _mount_scrolled_view(120.0, "Short enough to fit the band.")
	await sleep()

	var view: MarkdownView = scroll.get_child(0)
	var redraws: Array[int] = [0]
	var connected: int = view.draw.connect(func() -> void: redraws[0] += 1)
	expect_equal(connected, OK)
	await sleep()
	redraws[0] = 0

	scroll.get_v_scroll_bar().value = 0.0
	await sleep()
	expect_equal(redraws[0], 0)


## Builds a [MarkdownView] inside a [ScrollContainer] shorter than the document,
## which is the arrangement that makes the view cull primitives to the visible
## band. Returns the scroll container so a case can scroll it.
func _mount_scrolled_view(viewport_height: float, markdown_source: String) -> ScrollContainer:
	var scroll: ScrollContainer = autofree(ScrollContainer.new())
	scroll.position = Vector2.ZERO
	scroll.size = Vector2(400.0, viewport_height)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	var view: MarkdownView = MarkdownView.new()
	view.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	view.markdown = markdown_source
	scroll.add_child(view)
	get_tree().root.add_child(scroll)
	return scroll


## Generates a solid-color RGBA8 [ImageTexture] for image-resolver test cases.
func _make_solid_texture(width: int, height: int, color: Color) -> ImageTexture:
	var image: Image = Image.create(width, height, false, Image.FORMAT_RGBA8)
	image.fill(color)
	return ImageTexture.create_from_image(image)


## Computes the expected local center of the first task-list checkbox using the
## same indent and box-sizing formula as the native layout engine.
func _first_task_checkbox_center(theme: MarkdownTheme) -> Vector2:
	var base_size: float = float(theme.base_font_size)
	var indent: float = base_size * theme.list_indent_em
	var box_size: float = clampf(base_size * 0.92, 12.0, 20.0)
	var box_x: float = maxf(0.0, indent - box_size - 6.0)
	return Vector2(box_x + box_size * 0.5, box_size * 0.6)


## Dispatches a synthetic [InputEventMouseMotion] at [param local_position] on
## [param view].
func _send_mouse_motion(view: MarkdownView, local_position: Vector2) -> void:
	var viewport_position: Vector2 = view.global_position + local_position
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = viewport_position
	motion.global_position = viewport_position
	view.get_viewport().push_input(motion, true)


## Dispatches a synthetic left mouse button press and release at
## [param local_position] on [param view].
func _send_left_click(view: MarkdownView, local_position: Vector2) -> void:
	var viewport_position: Vector2 = view.global_position + local_position
	var press: InputEventMouseButton = InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = viewport_position
	press.global_position = viewport_position
	view.get_viewport().push_input(press, true)

	var release: InputEventMouseButton = InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	release.position = viewport_position
	release.global_position = viewport_position
	view.get_viewport().push_input(release, true)
