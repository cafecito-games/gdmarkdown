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

1. Refuses if `main` advanced since the run started, or if the tag now exists.
2. `scripts/set-version` injects the version into
   `addons/gdmarkdown/plugin.cfg` and rolls `## [Unreleased]` into a dated
   section for it.
3. Confirms those are the only two files that changed.
4. `scripts/package-addon --version` cuts the archive from a staged tree with
   the version injected, and `scripts/verify-package` checks the cut: the index
   declares this version and exactly one `core` slice, the packaged
   `plugin.cfg` carries the version, no repository file outside the addon
   reached the slice, and two cuts are byte-identical.
5. Uploads `dist/*` as a workflow artifact.
6. Commits the bump to an ephemeral `release/vX.Y.Z` branch and tags **that**
   commit, so the release source archive already carries the bumped
   `plugin.cfg`.
7. Creates the GitHub release, with the changelog section as the body and
   `gdmarkdown-<version>-core.zip` plus `gpm-index.toml` as assets.
8. Opens a pull request from the release branch to `main` and enables
   auto-merge, so the bump lands on `main` once the required checks pass.

The order matters: everything that can fail does so **before** anything is
pushed. A failed build leaves no branch, no tag, and no release.

## Why the bump never lands on main directly

`main` is protected: pull requests are required, and no bypass actor is
configured. Rather than weakening that so a workflow can push, the bump is
committed to a short-lived `release/vX.Y.Z` branch, which is what the tag and
the release point at, and a pull request brings it back. The same pattern is
used by `cafecito-games/AuthenticationKit`.

The workflow authenticates as the **cafecito-ci-bot** GitHub App rather than
with `GITHUB_TOKEN`. A pull request opened with `GITHUB_TOKEN` does not trigger
workflows, so its required checks would never start and auto-merge would wait
forever. The app is installed across the organization and
`CAFECITO_CI_BOT_APP_ID` and `CAFECITO_CI_BOT_PRIVATE_KEY` are organization
secrets visible to every repository, so nothing needs configuring per repo.

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

git checkout -b release/v1.2.0
git commit -am "Release v1.2.0"
git tag v1.2.0
git push origin release/v1.2.0 v1.2.0
gh release create v1.2.0 dist/* --title v1.2.0 --notes-file /tmp/notes.md
gh pr create --base main --head release/v1.2.0 --title "Release v1.2.0" --fill
```

## Branch protection

`main` carries the same classic protection as every other addon repository in
the organization:

| Setting | Value |
| --- | --- |
| Require a pull request before merging | yes, 0 required approvals |
| Required status checks | `verify` (from `ci.yml`), not strict |
| Allow force pushes | no |
| Allow deletions | no |
| Include administrators | no — you can still push directly when you must |

`release/*` branches are deliberately unprotected: they are ephemeral, exist
only between a release build and its pull request merging, and are deleted
automatically.

Auto-merge must stay enabled on the repository, otherwise the final step of a
release fails and the version bump sits in an open pull request. The release
itself is already published at that point, so the fix is simply to merge it.
