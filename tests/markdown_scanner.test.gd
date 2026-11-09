# gdkit:license:begin
# Copyright (c) 2026 Cafecito Games LLC
# SPDX-License-Identifier: MIT
# gdkit:license:end

extends GDTest
## The line and character helpers the parsers are built on.


func get_suite_name() -> String:
	return "MarkdownScanner"


func test_measures_leading_indent(line: String, expected: int) -> void:
	expect_equal(MarkdownScanner.leading_indent(line), expected)


func test_measures_leading_indent__cases() -> Dictionary[String, Variant]:
	return {
		"no indent": ["text", 0],
		"two spaces": ["  text", 2],
		"one tab": ["\ttext", 4],
		"space then tab advances to the stop": [" \ttext", 4],
		"tab then space": ["\t text", 5],
		"blank line of spaces": ["   ", 3],
	}


func test_strips_indent(line: String, columns: int, expected: String) -> void:
	expect_equal(MarkdownScanner.strip_indent(line, columns), expected)


func test_strips_indent__cases() -> Dictionary[String, Variant]:
	return {
		"exact": ["    text", 4, "text"],
		"fewer columns than asked": ["  text", 4, "text"],
		"more indent than asked": ["      text", 4, "  text"],
		"none to strip": ["text", 4, "text"],
	}


func test_detects_blank_lines(line: String, expected: bool) -> void:
	expect_equal(MarkdownScanner.is_blank(line), expected)


func test_detects_blank_lines__cases() -> Dictionary[String, Variant]:
	return {
		"empty": ["", true],
		"spaces": ["   ", true],
		"tab": ["\t", true],
		"text": ["a", false],
	}


func test_expands_leading_tabs() -> void:
	expect_equal(MarkdownScanner.expand_leading_tabs("\t- item"), "    - item")


func test_leaves_inner_tabs_alone() -> void:
	expect_equal(MarkdownScanner.expand_leading_tabs("a\tb"), "a\tb")


func test_normalizes_line_endings() -> void:
	var lines: Array[String] = MarkdownScanner.split_lines("a\r\nb\rc")

	expect_equal(lines, ["a", "b", "c"] as Array[String])


func test_strips_a_byte_order_mark() -> void:
	var lines: Array[String] = MarkdownScanner.split_lines("﻿# Title")

	expect_equal(lines, ["# Title"] as Array[String])


func test_joins_parts() -> void:
	expect_equal(MarkdownScanner.join(", ", ["a", "b", "c"] as Array[String]), "a, b, c")


func test_joins_an_empty_list_to_an_empty_string() -> void:
	expect_empty(MarkdownScanner.join(", ", [] as Array[String]))


func test_recognizes_punctuation(character: String, expected: bool) -> void:
	expect_equal(MarkdownScanner.is_punctuation(character), expected)


func test_recognizes_punctuation__cases() -> Dictionary[String, Variant]:
	return {
		"asterisk": ["*", true],
		"backslash": ["\\", true],
		"letter": ["a", false],
		"space": [" ", false],
		"empty": ["", false],
	}


func test_recognizes_digits(character: String, expected: bool) -> void:
	expect_equal(MarkdownScanner.is_digit(character), expected)


func test_recognizes_digits__cases() -> Dictionary[String, Variant]:
	return {
		"zero": ["0", true],
		"nine": ["9", true],
		"letter": ["a", false],
		"empty": ["", false],
	}


func test_normalizes_reference_labels(label: String, expected: String) -> void:
	expect_equal(MarkdownDocument.normalize_label(label), expected)


func test_normalizes_reference_labels__cases() -> Dictionary[String, Variant]:
	return {
		"case folded": ["Docs", "docs"],
		"edges trimmed": ["  docs  ", "docs"],
		"inner whitespace collapsed": ["the\t docs", "the docs"],
		"newlines collapsed": ["the\ndocs", "the docs"],
		"empty": ["", ""],
	}
