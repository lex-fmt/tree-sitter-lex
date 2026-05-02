<!-- Release notes for the next version. -->
<!-- Updated as work is done; consumed by scripts/create-release. -->

### Added

- Quarterly automated grammar-bump workflow (`bump-grammars.sh`) for keeping the embedded-grammars manifest in sync with upstream tree-sitter releases.

### Changed

- Repo onboarded to the canonical lex-fmt CI standardization: added `.github/CODEOWNERS` and `.github/workflows/copilot-review.yml` to auto-trigger Copilot review on PRs. (#13)
- Bumped `comms` submodule to v0.15.0 (catches up several minor releases — v0.12, v0.13, v0.14, v0.15 — including: structural-parser escape rule docs, table cell nesting docs, font ligatures + Unicode symbols doc, EDITORS.lex parity reference, footnotes.docs per-form sample set, includes feature proposal, footnote-table-scope sample, `:: notes ::` annotation spec split out from footnotes, annotation reference syntax `[::label]`, redundant `:: lex ::` wrapper cleanup, and the canonical Lex monochrome theme at `shared/theming/`).
- `scripts/parity-ignored.txt`: acknowledged 11 new parity divergences from the comms `footnotes.docs/` per-form sample set (DocumentTitle/Subtitle confusion + `:: notes ::` annotation handling, mirroring existing acknowledged categories). Tree-sitter has not yet grown `:: notes ::` awareness; the pinned lex-cli (v0.6.0) also predates the annotation rework.
