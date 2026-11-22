# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

class_name MarkdownDemoIcons
extends RefCounted
## Procedural icon and banner generator used by the interactive demo and screenshots.
##
## Produces crisp, anti-aliased [ImageTexture] icons on demand so the demo has
## zero external binary asset dependencies while showcasing inline item icons,
## custom callout badges, and block banners through [member MarkdownView.image_resolver].

static var _cache: Dictionary[String, Texture2D] = {}


## Resolves custom [code]icon://[/code] URIs for [member MarkdownView.image_resolver].
##
## Supports [code]icon://gold[/code], [code]icon://sword[/code],
## [code]icon://potion[/code], [code]icon://shield[/code], and
## [code]icon://banner[/code]. Generated textures are memoized in an internal
## cache; unknown URIs return [code]null[/code] so [MarkdownView] falls back to
## alt text.
static func resolve(uri: String) -> Texture2D:
	if _cache.has(uri):
		return _cache[uri]

	var texture: Texture2D = null
	match uri:
		"icon://gold":
			texture = _create_gold_icon()
		"icon://sword":
			texture = _create_sword_icon()
		"icon://potion":
			texture = _create_potion_icon()
		"icon://shield":
			texture = _create_shield_icon()
		"icon://banner":
			texture = _create_banner_emblem()
		_:
			return null

	_cache[uri] = texture
	return texture


## Generates and caches a tinted 24x24 icon for custom callout kinds such as
## [code]QUEST[/code] (compass star) and [code]LORE[/code] (open codex book).
static func create_callout_icon(kind: String, tint: Color) -> Texture2D:
	var cache_key: String = "callout:%s:%s" % [kind.to_upper(), tint.to_html(true)]
	if _cache.has(cache_key):
		return _cache[cache_key]

	var texture: Texture2D = null
	if kind.to_upper() == "QUEST":
		texture = _create_quest_callout_icon(tint)
	else:
		texture = _create_lore_callout_icon(tint)

	_cache[cache_key] = texture
	return texture


## Generates a 32x32 anti-aliased gold coin icon for [code]icon://gold[/code].
static func _create_gold_icon() -> ImageTexture:
	var dimension: int = 32
	var image: Image = Image.create(dimension, dimension, false, Image.FORMAT_RGBA8)
	var center: Vector2 = Vector2(15.5, 15.5)
	var outer_rim: Color = Color(0.76, 0.52, 0.12)
	var coin_face: Color = Color(0.98, 0.80, 0.28)
	var coin_highlight: Color = Color(1.0, 0.94, 0.62)
	var coin_shadow: Color = Color(0.84, 0.60, 0.14)

	for pixel_y: int in range(dimension):
		for pixel_x: int in range(dimension):
			var point: Vector2 = Vector2(float(pixel_x), float(pixel_y))
			var distance: float = point.distance_to(center)
			if distance > 13.8:
				continue

			var coverage: float = clampf(13.8 - distance, 0.0, 1.0)
			var color: Color = coin_face
			if distance > 11.2:
				color = outer_rim
			elif distance > 9.8:
				color = coin_shadow
			else:
				var diagonal: float = point.x - center.x + (point.y - center.y)
				if diagonal < -3.5:
					color = coin_highlight
				elif diagonal > 5.0:
					color = coin_shadow
				var diamond: float = absf(point.x - center.x) + absf(point.y - center.y)
				if diamond < 4.5:
					color = coin_highlight if diamond < 2.2 else outer_rim

			color.a = coverage
			image.set_pixel(pixel_x, pixel_y, color)

	return ImageTexture.create_from_image(image)


## Generates a 32x32 diagonal steel sword icon for [code]icon://sword[/code].
static func _create_sword_icon() -> ImageTexture:
	var dimension: int = 32
	var image: Image = Image.create(dimension, dimension, false, Image.FORMAT_RGBA8)
	var steel_light: Color = Color(0.90, 0.94, 0.99)
	var steel_dark: Color = Color(0.56, 0.64, 0.74)
	var steel_outline: Color = Color(0.28, 0.34, 0.44)
	var guard_gold: Color = Color(0.93, 0.74, 0.28)
	var hilt_leather: Color = Color(0.52, 0.31, 0.18)
	var pommel_ruby: Color = Color(0.88, 0.26, 0.32)

	for pixel_y: int in range(dimension):
		for pixel_x: int in range(dimension):
			var coordinate_x: float = float(pixel_x)
			var coordinate_y: float = float(pixel_y)
			# Diagonal blade axis from bottom-left (6, 26) to top-right (27, 5).
			var axis_along: float = (coordinate_x - 6.0 + (26.0 - coordinate_y)) * 0.7071
			var axis_across: float = (coordinate_x - 6.0 - (26.0 - coordinate_y)) * 0.7071

			var color: Color = Color(0.0, 0.0, 0.0, 0.0)

			# Blade body
			if axis_along >= 9.0 and axis_along <= 28.5:
				var half_width: float = 2.6
				if axis_along > 23.0:
					half_width = maxf(0.0, 2.6 * (28.5 - axis_along) / 5.5)
				if absf(axis_across) <= half_width:
					if absf(axis_across) > half_width - 0.85:
						color = steel_outline
					elif axis_across < 0.0:
						color = steel_light
					else:
						color = steel_dark

			# Crossguard
			if absf(axis_along - 9.0) <= 1.6 and absf(axis_across) <= 6.2:
				color = guard_gold if absf(axis_along - 9.0) <= 1.0 else Color(0.68, 0.48, 0.14)

			# Grip
			if axis_along >= 2.6 and axis_along < 7.4 and absf(axis_across) <= 1.6:
				color = hilt_leather

			# Pommel
			var pommel_distance: float = Vector2(
					coordinate_x,
					coordinate_y
			).distance_to(Vector2(6.5, 25.5))
			if pommel_distance <= 2.7:
				color = pommel_ruby if pommel_distance <= 1.5 else guard_gold

			if color.a > 0.0:
				image.set_pixel(pixel_x, pixel_y, color)

	return ImageTexture.create_from_image(image)


## Generates a 32x32 ruby potion flask icon for [code]icon://potion[/code].
static func _create_potion_icon() -> ImageTexture:
	var dimension: int = 32
	var image: Image = Image.create(dimension, dimension, false, Image.FORMAT_RGBA8)
	var flask_center: Vector2 = Vector2(15.5, 19.5)
	var glass_rim: Color = Color(0.76, 0.86, 0.96, 0.92)
	var glass_fill: Color = Color(0.24, 0.30, 0.40, 0.45)
	var liquid_main: Color = Color(0.90, 0.22, 0.34, 1.0)
	var liquid_deep: Color = Color(0.66, 0.12, 0.24, 1.0)
	var liquid_shine: Color = Color(1.0, 0.64, 0.70, 0.95)
	var cork_color: Color = Color(0.74, 0.54, 0.32, 1.0)

	for pixel_y: int in range(dimension):
		for pixel_x: int in range(dimension):
			var coordinate_x: float = float(pixel_x)
			var coordinate_y: float = float(pixel_y)
			var color: Color = Color(0.0, 0.0, 0.0, 0.0)

			# Cork stopper
			if coordinate_y >= 4.0 and coordinate_y <= 8.0 and absf(coordinate_x - 15.5) <= 2.4:
				color = cork_color

			# Bottle neck and lip
			if coordinate_y >= 7.5 and coordinate_y <= 13.0 and absf(coordinate_x - 15.5) <= 3.4:
				if absf(coordinate_x - 15.5) >= 2.2 or coordinate_y <= 9.2:
					color = glass_rim
				else:
					color = glass_fill

			# Rounded flask body
			var distance: float = Vector2(coordinate_x, coordinate_y).distance_to(flask_center)
			if distance <= 9.6:
				var coverage: float = clampf(9.6 - distance, 0.0, 1.0)
				if distance >= 8.0:
					color = glass_rim
				elif coordinate_y >= 17.0:
					color = (
							liquid_deep if coordinate_y > 23.0 or coordinate_x > 18.0
							else liquid_main
					)
					if Vector2(coordinate_x, coordinate_y).distance_to(Vector2(12.2, 19.5)) < 2.2:
						color = liquid_shine
				else:
					color = glass_fill
				color.a *= coverage

			if color.a > 0.01:
				image.set_pixel(pixel_x, pixel_y, color)

	return ImageTexture.create_from_image(image)


## Generates a 32x32 heraldic shield icon for [code]icon://shield[/code].
static func _create_shield_icon() -> ImageTexture:
	var dimension: int = 32
	var image: Image = Image.create(dimension, dimension, false, Image.FORMAT_RGBA8)
	var silver_rim: Color = Color(0.84, 0.89, 0.95)
	var silver_shadow: Color = Color(0.54, 0.61, 0.70)
	var field_blue: Color = Color(0.18, 0.42, 0.78)
	var field_deep: Color = Color(0.12, 0.28, 0.56)
	var emblem_gold: Color = Color(0.97, 0.78, 0.28)

	for pixel_y: int in range(dimension):
		for pixel_x: int in range(dimension):
			var coordinate_x: float = float(pixel_x) - 15.5
			var coordinate_y: float = float(pixel_y)
			if coordinate_y < 4.0 or coordinate_y > 28.0:
				continue

			var normalized_y: float = (coordinate_y - 4.0) / 24.0
			var half_width: float = 10.5
			if normalized_y > 0.45:
				var taper: float = (normalized_y - 0.45) / 0.55
				half_width = 10.5 * (1.0 - taper * taper)

			if absf(coordinate_x) > half_width:
				continue

			var edge_distance: float = minf(
					half_width - absf(coordinate_x),
					minf(coordinate_y - 4.0, 28.0 - coordinate_y)
			)
			var color: Color = field_blue if coordinate_x < 0.0 else field_deep

			if edge_distance < 2.2:
				color = silver_rim if coordinate_x < 0.0 else silver_shadow
			elif absf(coordinate_x) < 1.8 and coordinate_y >= 8.5 and coordinate_y <= 22.0:
				color = emblem_gold
			elif absf(coordinate_y - 14.0) < 1.8 and absf(coordinate_x) <= 5.8:
				color = emblem_gold

			image.set_pixel(pixel_x, pixel_y, color)

	return ImageTexture.create_from_image(image)


## Generates a 24x24 compass-star icon tinted with [param tint] for
## [code]QUEST[/code] callouts.
static func _create_quest_callout_icon(tint: Color) -> ImageTexture:
	var dimension: int = 24
	var image: Image = Image.create(dimension, dimension, false, Image.FORMAT_RGBA8)
	var center: Vector2 = Vector2(11.5, 11.5)

	for pixel_y: int in range(dimension):
		for pixel_x: int in range(dimension):
			var offset_x: float = float(pixel_x) - center.x
			var offset_y: float = float(pixel_y) - center.y
			var absolute_x: float = absf(offset_x)
			var absolute_y: float = absf(offset_y)

			# Four-point compass star silhouette
			var star_metric: float = minf(
					absolute_x * 0.45 + absolute_y,
					absolute_x + absolute_y * 0.45
			)
			var inner_diamond: float = absolute_x + absolute_y
			var alpha: float = 0.0

			if star_metric <= 9.2 and inner_diamond <= 10.8:
				alpha = clampf(9.2 - star_metric, 0.0, 1.0)
			if inner_diamond <= 3.1:
				alpha = 0.0
			if Vector2(offset_x, offset_y).length() <= 1.8:
				alpha = 1.0

			if alpha > 0.01:
				var pixel_color: Color = Color(tint.r, tint.g, tint.b, tint.a * alpha)
				image.set_pixel(pixel_x, pixel_y, pixel_color)

	return ImageTexture.create_from_image(image)


## Generates a 24x24 open-codex icon tinted with [param tint] for
## [code]LORE[/code] callouts.
static func _create_lore_callout_icon(tint: Color) -> ImageTexture:
	var dimension: int = 24
	var image: Image = Image.create(dimension, dimension, false, Image.FORMAT_RGBA8)

	for pixel_y: int in range(dimension):
		for pixel_x: int in range(dimension):
			var coordinate_x: float = float(pixel_x)
			var coordinate_y: float = float(pixel_y)
			var draw_pixel: bool = false

			# Open codex book outline with spine and page lines
			var within_book_y: bool = coordinate_y >= 5.0 and coordinate_y <= 19.0
			var within_book_x: bool = coordinate_x >= 3.0 and coordinate_x <= 20.0
			if within_book_x and within_book_y:
				var on_outer_edge: bool = (
						coordinate_x <= 4.5
						or coordinate_x >= 18.5
						or coordinate_y <= 6.5
						or coordinate_y >= 17.5
				)
				var on_spine: bool = absf(coordinate_x - 11.5) <= 0.9
				var on_text_line: bool = (
						(absf(coordinate_y - 10.0) <= 0.7 or absf(coordinate_y - 13.5) <= 0.7)
						and (
								coordinate_x >= 6.0 and coordinate_x <= 9.5
								or coordinate_x >= 13.5 and coordinate_x <= 17.0
						)
				)
				draw_pixel = on_outer_edge or on_spine or on_text_line

			if draw_pixel:
				image.set_pixel(pixel_x, pixel_y, tint)

	return ImageTexture.create_from_image(image)


## Generates a 520x112 decorative compass-rose banner for [code]icon://banner[/code].
static func _create_banner_emblem() -> ImageTexture:
	var width: int = 520
	var height: int = 112
	var image: Image = Image.create(width, height, false, Image.FORMAT_RGBA8)

	var backdrop_left: Color = Color(0.08, 0.11, 0.17)
	var backdrop_center: Color = Color(0.12, 0.17, 0.25)
	var border_color: Color = Color(0.30, 0.39, 0.54)
	var gold_accent: Color = Color(0.93, 0.75, 0.34)
	var gold_soft: Color = Color(0.56, 0.45, 0.24)
	var center: Vector2 = Vector2(float(width) * 0.5, float(height) * 0.5)

	for pixel_y: int in range(height):
		for pixel_x: int in range(width):
			var coordinate_x: float = float(pixel_x)
			var coordinate_y: float = float(pixel_y)

			# Rounded corner mask (radius 8px)
			var corner_radius: float = 8.0
			var clamped_x: float = clampf(
					coordinate_x,
					corner_radius,
					float(width) - 1.0 - corner_radius
			)
			var clamped_y: float = clampf(
					coordinate_y,
					corner_radius,
					float(height) - 1.0 - corner_radius
			)
			var corner_distance: float = Vector2(
					coordinate_x - clamped_x,
					coordinate_y - clamped_y
			).length()
			if corner_distance > corner_radius:
				continue

			var horizontal_blend: float = 1.0 - absf(coordinate_x - center.x) / (float(width) * 0.5)
			var color: Color = backdrop_left.lerp(backdrop_center, horizontal_blend * 0.85)

			# Subtle outer and inner frame lines
			var on_outer_border: bool = (
					pixel_x == 0
					or pixel_x == width - 1
					or pixel_y == 0
					or pixel_y == height - 1
					or corner_distance > corner_radius - 1.2
			)
			var on_inner_frame: bool = (
					(
							pixel_x >= 8
							and pixel_x <= width - 9
							and (pixel_y == 8 or pixel_y == height - 9)
					)
					or (
							pixel_y >= 8
							and pixel_y <= height - 9
							and (pixel_x == 8 or pixel_x == width - 9)
					)
			)

			if on_outer_border:
				color = border_color
			elif on_inner_frame:
				color = Color(gold_soft.r, gold_soft.g, gold_soft.b, 0.55).blend(color)

			# Flanking decorative filigree rules
			var distance_from_center_x: float = absf(coordinate_x - center.x)
			var distance_from_center_y: float = absf(coordinate_y - center.y)
			if distance_from_center_x >= 56.0 and distance_from_center_x <= 216.0:
				if distance_from_center_y <= 1.0:
					color = gold_accent.lerp(
							backdrop_center,
							(distance_from_center_x - 56.0) / 160.0
					)
				elif absf(distance_from_center_y - 8.0) <= 0.6 and distance_from_center_x <= 156.0:
					color = gold_soft

			# Flanking diamond nodes
			var flank_diamond: float = absf(distance_from_center_x - 56.0) + distance_from_center_y
			if flank_diamond <= 5.5:
				color = gold_accent if flank_diamond <= 3.0 else gold_soft

			# Central compass rose emblem
			var radial_distance: float = Vector2(
					coordinate_x - center.x,
					coordinate_y - center.y
			).length()
			if absf(radial_distance - 30.0) <= 1.4 or absf(radial_distance - 22.0) <= 0.9:
				color = gold_accent
			elif radial_distance < 22.0:
				var star_arm: float = minf(
						distance_from_center_x * 0.34 + distance_from_center_y,
						distance_from_center_x + distance_from_center_y * 0.34
				)
				if star_arm <= 19.5 and distance_from_center_x + distance_from_center_y <= 21.0:
					color = gold_accent if radial_distance > 5.0 else Color(1.0, 0.94, 0.72)

			image.set_pixel(pixel_x, pixel_y, color)

	return ImageTexture.create_from_image(image)
