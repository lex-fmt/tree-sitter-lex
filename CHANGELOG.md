# Changelog

## v0.8.1 (2026-05-17)

- (no release notes recorded)

## Unreleased

### Changed

- Bumped `lex` CLI pin past the v0.7.0 → v0.8.0 binary rename to `lexd` v0.14.1 in `shared/lex-deps.json` (now keyed as `lexd-cli`). `scripts/download-lex-cli.sh` renamed to `scripts/download-lexd-cli.sh`, fetches `lexd-<target>.tar.gz` and extracts the `lexd` binary from the directory-wrapped archive layout introduced in v0.8.0+. Parity harness (`scripts/test-all`, `test/helpers.bash`) updated to look for `bin/lexd` and to pass `--no-includes` to `lexd inspect` so include fixtures with non-existent targets don't abort. (#29)
- Bumped `comms` submodule to v0.16.2 (adds `lex.include.docs/` per-form sample set, plus several other docs and fixture updates).
- `scripts/parity-ignored.txt` re-baselined against `lexd` v0.14.1 and comms v0.16.2: removed 13 previously-acknowledged divergences that the bump closes (3 verbatim, 10 table); added 6 new acknowledged divergences (2 annotation alias resolution, 1 pipe-row content classification, 3 indented `lex.include` annotations inside sessions).

## v0.10.1 (2026-05-02)

### Added

- Quarterly automated grammar-bump workflow (`bump-grammars.sh`) for keeping the embedded-grammars manifest in sync with upstream tree-sitter releases.

### Changed

- Repo onboarded to the canonical lex-fmt CI standardization: added `.github/CODEOWNERS` and `.github/workflows/copilot-review.yml` to auto-trigger Copilot review on PRs. (#13)
- Bumped `comms` submodule to v0.15.0 (catches up several minor releases — v0.12, v0.13, v0.14, v0.15 — including: structural-parser escape rule docs, table cell nesting docs, font ligatures + Unicode symbols doc, EDITORS.lex parity reference, footnotes.docs per-form sample set, includes feature proposal, footnote-table-scope sample, `:: notes ::` annotation spec split out from footnotes, annotation reference syntax `[::label]`, redundant `:: lex ::` wrapper cleanup, and the canonical Lex monochrome theme at `shared/theming/`).
- `scripts/parity-ignored.txt`: acknowledged 11 new parity divergences from the comms `footnotes.docs/` per-form sample set (DocumentTitle/Subtitle confusion + `:: notes ::` annotation handling, mirroring existing acknowledged categories). Tree-sitter has not yet grown `:: notes ::` awareness; the pinned lex-cli (v0.6.0) also predates the annotation rework.

## v0.10.0 (2026-04-26)

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

## v0.9.1 (2026-04-24)

### Added

- Allow directly-nested inline formatting markers (e.g. `**_bold-italic_**`).
