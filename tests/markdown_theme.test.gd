# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends GDTest
## Unit tests for [MarkdownTheme] and [MarkdownCalloutStyle] resources.
##
## Covers built-in dark/light presets, case-insensitive callout registration and
## fallback synthesis, heading scale bounds, metric clamping, and live
## [signal Resource.changed] signal forwarding.


## Returns the test suite name reported by [code]gdtest[/code].
func get_suite_name() -> String:
	return "MarkdownTheme"


## Verifies that [method MarkdownTheme.create_dark] and
## [method MarkdownTheme.create_light] produce distinct, well-contrasted palettes.
func test_create_dark_and_light_presets_have_distinct_palettes() -> void:
	var dark_theme: MarkdownTheme = MarkdownTheme.create_dark()
	var light_theme: MarkdownTheme = MarkdownTheme.create_light()

	expect_not_null(dark_theme)
	expect_not_null(light_theme)
	expect_equal(dark_theme.base_font_size, 16)
	expect_equal(light_theme.base_font_size, 16)
	expect_true(dark_theme.text_color.get_luminance() > light_theme.text_color.get_luminance())
	expect_true(
			dark_theme.code_bg_color.get_luminance() < light_theme.code_bg_color.get_luminance()
	)
	expect_not_equal(dark_theme.link_color, light_theme.link_color)


## Verifies all five GitHub alert callout kinds are pre-seeded on both dark and
## light presets with distinct theme-matched accent colors.
func test_default_callouts_are_seeded_on_presets() -> void:
	var dark_theme: MarkdownTheme = MarkdownTheme.create_dark()
	var light_theme: MarkdownTheme = MarkdownTheme.create_light()
	var default_kinds: Array[String] = ["NOTE", "TIP", "IMPORTANT", "WARNING", "CAUTION"]
	var expected_titles: Array[String] = ["Note", "Tip", "Important", "Warning", "Caution"]

	for index: int in range(default_kinds.size()):
		var kind: String = default_kinds[index]
		var expected_title: String = expected_titles[index]
		var dark_style: MarkdownCalloutStyle = dark_theme.get_callout_style(kind)
		var light_style: MarkdownCalloutStyle = light_theme.get_callout_style(kind)

		expect_not_null(dark_style)
		expect_not_null(light_style)
		expect_equal(dark_style.title, expected_title)
		expect_equal(light_style.title, expected_title)
		expect_not_equal(dark_style.accent_color, light_style.accent_color)


## Verifies [method MarkdownTheme.get_callout_style] normalizes case and strips
## surrounding whitespace before lookup.
func test_get_callout_style_is_case_insensitive_and_trims_whitespace() -> void:
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	var canonical_note: MarkdownCalloutStyle = theme.get_callout_style("NOTE")
	var lowercase_note: MarkdownCalloutStyle = theme.get_callout_style("note")
	var padded_note: MarkdownCalloutStyle = theme.get_callout_style("  NoTe  ")
	var mixed_tip: MarkdownCalloutStyle = theme.get_callout_style("TiP")

	expect_same(lowercase_note, canonical_note)
	expect_same(padded_note, canonical_note)
	expect_equal(mixed_tip.title, "Tip")


## Verifies registering a custom [MarkdownCalloutStyle] and clearing it with
## [code]null[/code] reverts lookups to a synthesized fallback.
func test_custom_callout_registration_and_removal() -> void:
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	var quest_accent: Color = Color(0.92, 0.72, 0.22, 1.0)
	var quest_style: MarkdownCalloutStyle = MarkdownCalloutStyle.create("Quest", quest_accent)

	theme.set_callout_style("quest", quest_style)

	var fetched_upper: MarkdownCalloutStyle = theme.get_callout_style("QUEST")
	var fetched_mixed: MarkdownCalloutStyle = theme.get_callout_style("QuEsT")
	expect_same(fetched_upper, quest_style)
	expect_same(fetched_mixed, quest_style)
	expect_equal(fetched_upper.accent_color, quest_accent)

	theme.set_callout_style("QUEST", null)

	var after_removal: MarkdownCalloutStyle = theme.get_callout_style("QUEST")
	expect_not_same(after_removal, quest_style)
	expect_equal(after_removal.title, "Quest")


## Verifies unregistered callout kinds synthesize title-cased fallback styles
## using [member MarkdownTheme.blockquote_accent_color].
func test_unregistered_callout_synthesizes_fallback_style() -> void:
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	var custom_fallback: MarkdownCalloutStyle = theme.get_callout_style("lore_entry")
	var dashed_fallback: MarkdownCalloutStyle = theme.get_callout_style("side-quest")
	var empty_fallback: MarkdownCalloutStyle = theme.get_callout_style("")

	expect_not_null(custom_fallback)
	expect_equal(custom_fallback.title, "Lore Entry")
	expect_equal(custom_fallback.accent_color, theme.blockquote_accent_color)
	expect_equal(dashed_fallback.title, "Side Quest")
	expect_equal(empty_fallback.title, "Note")


## Verifies [method MarkdownCalloutStyle.create] defaults and that
## [method MarkdownCalloutStyle.resolve_title_color] falls back to
## [member MarkdownCalloutStyle.accent_color] when title alpha is zero.
func test_callout_style_create_and_resolve_title_color() -> void:
	var accent: Color = Color(0.25, 0.75, 0.55, 1.0)
	var explicit_title: Color = Color(0.95, 0.88, 0.40, 1.0)
	var panel: StyleBoxFlat = StyleBoxFlat.new()
	var image: Image = Image.create(8, 8, false, Image.FORMAT_RGBA8)
	var icon_texture: ImageTexture = ImageTexture.create_from_image(image)

	var default_title_style: MarkdownCalloutStyle = MarkdownCalloutStyle.create("Lore", accent)
	expect_equal(default_title_style.title, "Lore")
	expect_equal(default_title_style.accent_color, accent)
	expect_equal(default_title_style.title_color.a, 0.0)
	expect_equal(default_title_style.resolve_title_color(), accent)
	expect_null(default_title_style.icon)
	expect_null(default_title_style.panel_style)

	var explicit_style: MarkdownCalloutStyle = MarkdownCalloutStyle.create(
			"Boss",
			accent,
			explicit_title,
			icon_texture,
			panel
	)
	expect_equal(explicit_style.title, "Boss")
	expect_equal(explicit_style.resolve_title_color(), explicit_title)
	expect_same(explicit_style.icon, icon_texture)
	expect_same(explicit_style.panel_style, panel)


## Verifies mutations on a registered [MarkdownCalloutStyle] forward
## [signal Resource.changed] to the owning [MarkdownTheme], skip no-op writes,
## and disconnect cleanly upon removal.
func test_callout_style_mutations_emit_changed_and_propagate_to_theme() -> void:
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	var custom_style: MarkdownCalloutStyle = MarkdownCalloutStyle.create(
			"Lore",
			Color(0.5, 0.4, 0.9, 1.0)
	)
	var style_probe: GDTestSignalProbe = watch_signal(custom_style.changed)
	var theme_probe: GDTestSignalProbe = watch_signal(theme.changed)

	theme.set_callout_style("lore", custom_style)
	expect_equal(theme_probe.emission_count(), 1)

	custom_style.title = "Ancient Lore"
	expect_equal(style_probe.emission_count(), 1)
	expect_equal(theme_probe.emission_count(), 2)

	custom_style.accent_color = Color(0.9, 0.5, 0.2, 1.0)
	expect_equal(style_probe.emission_count(), 2)
	expect_equal(theme_probe.emission_count(), 3)

	# Setting an identical value should not emit a redundant changed signal.
	custom_style.title = "Ancient Lore"
	expect_equal(style_probe.emission_count(), 2)
	expect_equal(theme_probe.emission_count(), 3)

	# Removing the callout style disconnects forwarding into the theme.
	theme.set_callout_style("lore", null)
	expect_equal(theme_probe.emission_count(), 4)
	custom_style.title = "Forgotten Lore"
	expect_equal(style_probe.emission_count(), 3)
	expect_equal(theme_probe.emission_count(), 4)


## Verifies direct [MarkdownTheme] property setters emit [signal Resource.changed]
## only when the value actually changes.
func test_direct_theme_property_mutations_emit_changed() -> void:
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	var probe: GDTestSignalProbe = watch_signal(theme.changed)

	theme.base_font_size = 20
	expect_equal(probe.emission_count(), 1)

	# Identical assignment is a no-op.
	theme.base_font_size = 20
	expect_equal(probe.emission_count(), 1)

	theme.text_color = Color(0.8, 0.8, 0.8, 1.0)
	expect_equal(probe.emission_count(), 2)

	theme.line_spacing = 8.0
	expect_equal(probe.emission_count(), 3)

	theme.corner_radius = 10.0
	expect_equal(probe.emission_count(), 4)


## Verifies bulk-assigning [member MarkdownTheme.callouts] uppercases valid keys,
## drops empty or non-style entries, and wires change forwarding.
func test_set_callouts_normalizes_keys_and_forwards_signals() -> void:
	var theme: MarkdownTheme = MarkdownTheme.create_dark()
	var quest_style: MarkdownCalloutStyle = MarkdownCalloutStyle.create(
			"Quest",
			Color(0.9, 0.7, 0.2, 1.0)
	)
	var mapping: Dictionary[String, Variant] = {
		" quest ": quest_style,
		"": MarkdownCalloutStyle.create("Ignored", Color.WHITE),
		"invalid_entry": "not_a_style",
	}

	theme.callouts = mapping

	var stored: Dictionary[String, MarkdownCalloutStyle] = {}
	stored.assign(theme.callouts)
	expect_equal(stored.size(), 1)
	expect_true(stored.has("QUEST"))
	expect_same(theme.get_callout_style("quest"), quest_style)

	var probe: GDTestSignalProbe = watch_signal(theme.changed)
	quest_style.title = "Updated Quest"
	expect_equal(probe.emission_count(), 1)


## Verifies [method MarkdownTheme.get_heading_scale] clamps out-of-range levels
## to [code]1..6[/code] and falls back to defaults for short or non-positive arrays.
func test_heading_scales_and_bounds_handling() -> void:
	var theme: MarkdownTheme = MarkdownTheme.create_dark()

	expect_approx(theme.get_heading_scale(1), 1.85, 0.001)
	expect_approx(theme.get_heading_scale(2), 1.5, 0.001)
	expect_approx(theme.get_heading_scale(3), 1.25, 0.001)
	expect_approx(theme.get_heading_scale(4), 1.1, 0.001)
	expect_approx(theme.get_heading_scale(5), 1.0, 0.001)
	expect_approx(theme.get_heading_scale(6), 0.9, 0.001)

	# Out-of-bounds heading levels clamp to [1, 6].
	expect_approx(theme.get_heading_scale(0), theme.get_heading_scale(1), 0.001)
	expect_approx(theme.get_heading_scale(-5), theme.get_heading_scale(1), 0.001)
	expect_approx(theme.get_heading_scale(7), theme.get_heading_scale(6), 0.001)
	expect_approx(theme.get_heading_scale(99), theme.get_heading_scale(6), 0.001)

	# Short or non-positive custom scales fall back gracefully to defaults.
	var custom_scales: PackedFloat32Array = PackedFloat32Array([2.5, 0.0])
	theme.heading_scales = custom_scales
	expect_approx(theme.get_heading_scale(1), 2.5, 0.001)
	expect_approx(theme.get_heading_scale(2), 1.5, 0.001)
	expect_approx(theme.get_heading_scale(3), 1.25, 0.001)


## Verifies numeric metric setters clamp invalid or negative values to safe minimums.
func test_metric_setters_clamp_invalid_values() -> void:
	var theme: MarkdownTheme = MarkdownTheme.create_dark()

	theme.base_font_size = 0
	expect_equal(theme.base_font_size, 1)

	theme.code_font_scale = -2.0
	expect_approx(theme.code_font_scale, 0.1, 0.001)

	theme.line_spacing = -4.0
	expect_approx(theme.line_spacing, 0.0, 0.001)

	theme.callout_icon_size = 1.0
	expect_approx(theme.callout_icon_size, 4.0, 0.001)

	theme.rule_thickness = 0.0
	expect_approx(theme.rule_thickness, 0.5, 0.001)

	theme.max_image_height = -10.0
	expect_approx(theme.max_image_height, 1.0, 0.001)

	theme.inline_image_scale = 0.0
	expect_approx(theme.inline_image_scale, 0.1, 0.001)
