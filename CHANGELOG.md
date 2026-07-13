<!-- generated - do not edit; fragments live in CHANGELOG/ (`shipit changelog render` regenerates this file) -->

# Changelog

## Unreleased

- Fix on-upstream-released cascade startup_failure by granting the handler contents/pull-requests write (release#805)
- Download lexd binary into deps/ instead of bin/
- Migrate the editor cascade fan-out to tree-sitter.yml's notify-downstreams input (drop the hand-rolled job; needs release@v3.8.0+)
- ci: migrate release reusable-workflow callers from @v2 to @v3
- Add corpus tests for nested sessions, multiple top-level sessions, inline-formatted titles, and subject-style titles
- ci: adopt the shipit release pipeline (shipit-release.yml caller +
  declared tree-sitter artifact) — releases still ship the same
  `tree-sitter.tar.gz` asset and notify the editor repos
  (vscode/nvim/lexed) on real releases; the legacy release.yml caller
  remains until the release-candidate proof

## 0.11.2 - 2026-06-01

- Migrate changelog handling to the fragment-directory model
  (arthur-debert/release#201). Future entries go in
  CHANGELOG/unreleased-<slug>.md fragments via `bin/changelog add`.


All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [0.11.0] - 2026-05-21

### Changed

- Spell-check captures in `queries/highlights.scm` rewritten to match the canonical policy: spell-check all prose by default; suppress only annotation labels/params, verbatim bodies, references, and non-prose inline atoms (code spans, math spans, escape sequences). Verbatim block *subjects* and the trailing descriptor after `:: label ::` are now spell-checked. Uses `#has-ancestor?` + `#not-has-parent?` so the verbatim's own subject field stays prose-checked while nested prose inside the body is suppressed at arbitrary depth (#45).

### Added

- `test/spellcheck-fixture.lex` — a fixture with deliberate typos seeded at every prose / non-prose position. Downstream editor e2e tests (`nvim`, `lexed`, `vscode`, `zed-lex`) mirror this file and assert against the same matrix (#45).

## [0.10.4] - 2026-05-18

### Changed

- Release pipeline migrated to `arthur-debert/release/.github/workflows/tree-sitter.yml@v1` (see #42). Trigger contract is now `workflow_dispatch` (matches the cross-repo cascade-handler pattern). Corpus tests run as a release-time gate before the tag is pushed; missing WASM or empty `queries/` now hard-fails the build rather than producing a silently-incomplete tarball.
- CHANGELOG.md reformatted to strict Keep-a-Changelog spec (bracketed `[Unreleased]` / `[X.Y.Z] - DATE` headings) so the canonical `prepare-release-npm` composite action can auto-roll release notes.

## [0.10.3] - 2026-05-17

- (no release notes recorded)

## [0.10.2] - 2026-05-17

- (no release notes recorded)

## Historical (pre-migration unrolled section)

The `lex` CLI rename to `lexd` work below was prepared as an unrolled `## Unreleased` section in the pre-Keep-a-Changelog format and never cut as its own version. Preserved here for history. The non-bracketed heading is intentional — `prepare-release-npm`'s `roll-changelog.sh` only matches `^## \[` so this section is invisible to release automation.

### Changed

- Bumped `lex` CLI pin past the v0.7.0 → v0.8.0 binary rename to `lexd` v0.14.1 in `shared/lex-deps.json` (now keyed as `lexd-cli`). `scripts/download-lex-cli.sh` renamed to `scripts/download-lexd-cli.sh`, fetches `lexd-<target>.tar.gz` and extracts the `lexd` binary from the directory-wrapped archive layout introduced in v0.8.0+. Parity harness (`scripts/test-all`, `test/helpers.bash`) updated to look for `bin/lexd` and to pass `--no-includes` to `lexd inspect` so include fixtures with non-existent targets don't abort. (#29)
- Bumped `comms` submodule to v0.16.2 (adds `lex.include.docs/` per-form sample set, plus several other docs and fixture updates).
- `scripts/parity-ignored.txt` re-baselined against `lexd` v0.14.1 and comms v0.16.2: removed 13 previously-acknowledged divergences that the bump closes (3 verbatim, 10 table); added 6 new acknowledged divergences (2 annotation alias resolution, 1 pipe-row content classification, 3 indented `lex.include` annotations inside sessions).

## [0.10.1] - 2026-05-02

### Added

- Quarterly automated grammar-bump workflow (`bump-grammars.sh`) for keeping the embedded-grammars manifest in sync with upstream tree-sitter releases.

### Changed

- Repo onboarded to the canonical lex-fmt CI standardization: added `.github/CODEOWNERS` and `.github/workflows/copilot-review.yml` to auto-trigger Copilot review on PRs. (#13)
- Bumped `comms` submodule to v0.15.0 (catches up several minor releases — v0.12, v0.13, v0.14, v0.15 — including: structural-parser escape rule docs, table cell nesting docs, font ligatures + Unicode symbols doc, EDITORS.lex parity reference, footnotes.docs per-form sample set, includes feature proposal, footnote-table-scope sample, `:: notes ::` annotation spec split out from footnotes, annotation reference syntax `[::label]`, redundant `:: lex ::` wrapper cleanup, and the canonical Lex monochrome theme at `shared/theming/`).
- `scripts/parity-ignored.txt`: acknowledged 11 new parity divergences from the comms `footnotes.docs/` per-form sample set (DocumentTitle/Subtitle confusion + `:: notes ::` annotation handling, mirroring existing acknowledged categories). Tree-sitter has not yet grown `:: notes ::` awareness; the pinned lex-cli (v0.6.0) also predates the annotation rework.

## [0.10.0] - 2026-04-26

- Added `shared/embedded-grammars.json`: a curated manifest of tree-sitter
  grammars editor packagers should bundle to render `:: lang ::` injection
  inside Lex verbatim blocks. Initial list: python, javascript, json,
  rust, bash. The manifest is shipped inside `tree-sitter.tar.gz` so
  downstream editors (vscode, lexed) consume one source of truth instead
  of each maintaining their own array.
- Added `scripts/smoke-grammars.sh`: HEAD-checks every manifest entry's
  WASM, highlights query, and LICENSE against upstream GitHub. Wired
  into both the CI workflow and the release pipeline so a broken upstream
  pin fails fast rather than at downstream build time.

## [0.9.1] - 2026-04-24

### Added

- Allow directly-nested inline formatting markers (e.g. `**_bold-italic_**`).
