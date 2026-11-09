# Releasing

Releases are cut by the **Release** workflow. Open *Actions ▸ Release ▸ Run
workflow*, pick `patch`, `minor`, or `major`, and run it from `main`. Nothing
else is typed: the version is derived, injected, committed, tagged, and
published.

## What the workflow does

**`quality`** decides the version and proves the tree is releasable.

1. Refuses to run from any branch but `main`.
2. Reads the latest `v*` tag, applies the chosen bump, and refuses if that tag
   already exists. With no tags at all it starts from `v0.0.0`, so the first
   `minor` release is `v0.1.0`.
3. Refuses if the changelog's `## [Unreleased]` section is empty — a release
   nobody wrote notes for stops here, in seconds.
4. Runs the same gate as CI: `gdkit format check`, `gdkit lint check`,
   `gdkit uid check`, and the full gdtest suite on pinned Godot 4.7.2.

**`publish`** performs the release.

1. `scripts/set-version` injects the version into
   `addons/gdmarkdown/plugin.cfg` and rolls `## [Unreleased]` into a dated
   section for it.
2. Confirms those are the only two files that changed.
3. `scripts/package-addon --version` cuts the archive from a staged tree with
   the version injected, and `scripts/verify-package` checks the cut: the index
   declares this version and exactly one `core` slice, the packaged
   `plugin.cfg` carries the version, no repository file outside the addon
   reached the slice, and two cuts are byte-identical.
4. Uploads `dist/*` as a workflow artifact.
5. Commits the bump to `main`, refusing if `main` advanced since the run
   started.
6. Creates the tag and the GitHub release at that commit, with the changelog
   section as the body and `gdmarkdown-<version>-core.zip` plus
   `gpm-index.toml` as assets.

The order matters: everything that can fail does so **before** anything is
pushed. A failed build leaves no commit, no tag, and no release.

## Why the version is injected rather than read

The bump chosen when the workflow runs is the single source of truth. It is
injected into the staged `plugin.cfg` at package time and committed back to
`main` afterwards, so a release can never ship a stale version because somebody
forgot to edit a file. Running `scripts/package-addon` locally with no
`--version` falls back to the committed value, which is what a test cut wants.

## Consumers

A GitHub release carrying a `gpm-index.toml` asset is discovered as sliced with
no manifest change on the consumer's side:

```sh
gpm add --name gdmarkdown \
  --source github-release \
  --repo cafecito-games/gdmarkdown \
  --version v1.2.0
```

Because the addon is pure GDScript it publishes one mandatory `core` slice and
no platform slices, so every consumer installs the same files.

## Doing it by hand

Only needed if the workflow cannot run.

```sh
scripts/set-version 1.2.0
scripts/package-addon --version 1.2.0
scripts/verify-package 1.2.0
scripts/release-notes 1.2.0 > /tmp/notes.md

git commit -am "Release v1.2.0"
git push origin main
gh release create v1.2.0 dist/* --title v1.2.0 --notes-file /tmp/notes.md
```

## If the push is rejected

The workflow pushes a commit to `main`. If branch protection forbids pushes
from Actions, either allow the `github-actions[bot]` actor to bypass it for
this repository, or run the by-hand sequence above through a pull request and
let the workflow pick up from the merged commit.
