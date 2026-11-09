# Architecture

The addon is a three-stage pipeline. Each stage has one job, one input type and
one output type, so each can be tested on its own.

```text
Markdown text
    │
    ├─ MarkdownBlockParser ──→ MarkdownDocument   (block tree, raw inline text on leaves)
    │
    ├─ MarkdownInlineParser ─→ MarkdownDocument   (same tree, leaves expanded into inline nodes)
    │
    └─ MarkdownBbcodeEmitter ─→ String            (BBCode for a RichTextLabel)
```

`Markdown.parse()` runs the first two stages; `Markdown.to_bbcode()` runs all
three. `MarkdownLabel` is a thin node wrapper over `to_bbcode()`.

## Why a tree and not a direct translation

A single-pass Markdown-to-BBCode translator is smaller, and it was considered.
It fails on nesting: a task list inside a block quote, emphasis spanning a code
span, a link whose label contains strong emphasis. Each of those becomes a
special case that can only be tested end to end, and every later fix risks a
regression somewhere else in the string-rewriting order.

With a tree, the block parser is tested by asserting a shape, the emitter by
feeding it a shape, and nesting falls out of recursion. It also means a second
emitter — plain text for accessibility, or an outline for a table of contents —
is additive rather than a rewrite.

## MarkdownNode: one enum-tagged type

GDScript has no sum types. The tree is therefore one class with a `kind` enum
and the union of every construct's fields, rather than nineteen subclasses.

The trade-off is explicit: not every field is meaningful for every `kind`. In
exchange, tests compare whole subtrees through one `to_debug_string()`, adding a
construct adds no file, and no test has to assemble an object graph to state an
expectation. A class per node type would be more precisely typed and
considerably more code for the same behavior.

## Block parsing

`MarkdownBlockParser.parse_sequence()` takes already-de-indented lines and
dispatches on what the line starts. Container blocks — block quotes and list
items — strip their own marker and **recurse** on the remaining lines. That is
the whole nesting story: there is no container stack to keep in sync, and a list
inside a quote inside a list needs no code of its own.

Leaf blocks (paragraphs, headings, table cells) keep their raw Markdown in
`MarkdownNode.text` and are expanded later. Link reference definitions are
document-scoped and may appear after the paragraph that uses them, so the block
parser collects them into `MarkdownDocument.link_references` first and the inline
parser resolves against that map afterwards.

## Inline parsing

`MarkdownInlineParser` scans a character range in CommonMark's precedence order:
backslash escapes and code spans bind tightest, then autolinks, then links and
images, then emphasis runs. Each `_take_*` helper returns how many characters it
consumed, or zero to leave the character as literal text.

Emphasis is resolved by searching forward for a matching delimiter run while
counting nested openers of the same character, rather than by CommonMark's
delimiter stack with its linked list and rule-of-three. Every ordinary nesting
resolves identically, `***both***` included. Pathological overlapping runs such
as `*foo**bar*baz**` render as literal text instead of being guessed at. That is
a deliberate ceiling: the delimiter stack is a significant amount of
index-shifting code in a language without a linked list, and the inputs it buys
do not occur in game content.

## Emission

`MarkdownBbcodeEmitter` walks the tree and writes BBCode, escaping `[` as
`[lb]` — the only character BBCode treats as markup — and percent-encoding
destinations, where a space would end a tag attribute.

Lists are rendered with explicit markers and `[indent]` rather than with
BBCode's own `[ul]` and `[ol]`. Those tags split items on newlines, which makes
them unable to carry a nested list, and they offer no control over task glyphs
or an ordered list that starts at something other than one.

A code block is wrapped in a single-cell BBCode table rather than a background
tag. A background tag covers only the glyphs it wraps, so a code block drawn
with one ends up with a ragged right edge, and padding the lines does not fix
it: the text server trims trailing whitespace during shaping, and padding with
non-breaking spaces falls out of the monospace font into a fallback with a
different advance. A cell is a true rectangle and takes real padding. The cost
is that a cell does not wrap, which `MarkdownStyle.solid_code_background`
exists to turn off.

Images are the one place the emitter touches the engine: a `res://` or `user://`
path is checked with `ResourceLoader.exists()` before becoming an `[img]`. A
path that does not resolve falls back to its alternate text, and a remote URL
links its alternate text instead, because the addon ships no HTTP client.

## Strict typing

`project.godot` sets every GDScript warning Godot 4.7 offers to **error**, and
`gdkit lint` additionally enables `require-return-type`,
`require-argument-type`, `require-variable-type`, `require-typed-collection`,
`require-typed-loop-variable`, `require-signal-argument-type`,
`inconsistent-return-statements` and `unused-private-member`.

Two consequences are worth knowing before editing:

- **`:=` is an error.** `inferred_declaration` is on, so every declaration
  states its type.
- **Discarded return values are errors.** `PackedStringArray.append()` returns a
  `bool` and `Signal.connect()` returns an `int`, so neither can be called
  bare. The parsers accumulate into `Array[String]` — whose `append` is
  `void` — and join through `MarkdownScanner.join()`, which exists for exactly
  that reason.

## The demo

`demo/demo.tscn` is the project's main scene: a document list, a
`MarkdownLabel`, and a style picker over the three presets in
`demo/demo_presets.gd`. `demo/capture.gd` reuses those presets to render the
README screenshots into `docs/images`, through `scripts/capture-screenshots`.

The capture renders into a `SubViewport` of an exact size rather than into the
window, so the images do not depend on the display the capture ran on, and a
document shot can be resized to the height its content measures.

## Verification

```sh
task lint   # gdkit format check, lint check, uid check
task test   # ./addons/gdtest/gdtest run
task bench  # ./addons/gdtest/gdtest bench
```

Benchmarks live in `tests/markdown.bench.gd` and measure each stage separately,
because the stages regress for different reasons. Timings are informational and
never fail a build.

`tests/fixtures/kitchen_sink.md` exercises every supported construct and
`kitchen_sink.bbcode` holds its reviewed output. Any change that moves a single
character of the rendered result shows up there as a diff. When a change to the
pipeline is intended, review that diff rather than regenerating the fixture
without reading it.
