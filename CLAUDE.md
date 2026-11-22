# gdmarkdown

Native C++ GDExtension Markdown renderer (`godot-cpp` + vendored `md4c`) for
Godot 4.6+ (verified on 4.7.2). Read `docs/architecture.md` before changing the
native pipeline, public Godot API, or packaging scripts.

## Native builds & common tasks

```sh
task macos               # build addons/gdmarkdown/bin/macos_arm64/gdmarkdown.framework
task ios                 # build & sign device + simulator dynamic ios/gdmarkdown.xcframework
task android             # build Android arm64, arm32, and x86_64 .so libraries
task linux               # build Linux x86_64 / arm64 .so libraries
task native              # build all native targets supported by the current host
task demo                # launch the interactive showcase
task screenshots         # regenerate docs/images/*.png (requires a GPU/rendering context)
task vendor DEST=/path   # rsync staged runtime addon into a consumer project
```

## Verification gate

Every relevant gate must pass before any change is complete:

```sh
gdkit format check . && gdkit lint check . && gdkit license check --minimum-version 0.11.0 . && gdkit uid check .
task test:native
task test:extension-load
task test:gdscript
```

`task` runs `lint` and `test` (all three test suites). When modifying packaging,
`gdmarkdown.gdextension`, or `scripts/`, also run:

```sh
task verify:package
./scripts/release-notes-present
```

Run `gdkit format write .`, `gdkit license write --minimum-version 0.11.0 .`,
and `gdkit uid write .` (or `task format`, `task license`, `task uid`) to fix
GDScript formatting, headers, and sidecars cleanly.

## Packaging rules

- `packaging/gpm-package.toml` lives intentionally out of the repository root so
  a bare `gpm package` fails loudly instead of publishing `native/`, `demo/`, or
  `tests/`.
- `scripts/stage-addon-tree` is the single source of truth for what ships.
- `scripts/package-addon` stages the addon tree, injects the version into
  `plugin.cfg`, and cuts 8 `gpm` slices inferred from
  `addons/gdmarkdown/gdmarkdown.gdextension`:
  `core`, `ios`, `macos.arm64`, `android.arm64`, `android.arm32`,
  `android.x86_64`, `linux.x86_64`, `linux.arm64`.
- The `core` slice must never contain `.framework`, `.xcframework`, `.so`,
  `.dylib`, `.a`, `.dll`, or `native/` files.
- Never bump a version by hand: the Release workflow derives the version from
  the latest tag and `scripts/set-version` injects it into `plugin.cfg` and
  `CHANGELOG.md`.
- Keep `## [Unreleased]` in `CHANGELOG.md` up to date — `scripts/release-notes-present`
  fails a release when that section is empty, and its contents become the GitHub
  release body.

## Constraints that bite

- Every GDScript warning in `project.godot` is an **error**, including
  `inferred_declaration`, so `:=` does not compile anywhere in `addons/`,
  `demo/`, or `tests/`. Declare every type explicitly.
- `return_value_discarded` is an error. Capture return values from
  `Signal.connect()`, `PackedStringArray.resize()`, `Image.save_png()`, etc.
- Every GDScript file carries an MIT header between `# gdkit:license:begin` and
  `# gdkit:license:end` managed by `gdkit license` (`.gdkit/license.json`).
  Never hand-edit inside the markers.
- `addons/gdtest/` is installed by `gpm install` (`task install`) and is not
  committed. Never edit it or hand-edit `addons.lock`.
- `docs/images/*.png` are rendered by `./scripts/capture-screenshots`, which
  needs a real rendering context (headless runs write blank images). Re-capture
  and inspect them after visual rendering changes.
- `main` is protected: pull requests required, `verify` must pass, no force
  pushes or deletions. Never push to `main` directly; see `docs/releasing.md`.
