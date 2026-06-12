<!-- BEGIN release-managed orientation — managed by release-sync; do not edit -->
This repo's quality gate, build, release, and PR/dev flow are provided by
`release-core` (installed at session start; not stored in this repo).

- **Start here:** run `release-core how-to` — the task playbook for *this* repo.
- Reference: `release-core --help`, `release-core <cmd> --help`, `release-core detect-kind`.
- Quality gate (run every loop, after `git add`): `release-core gate`.
<!-- END release-managed orientation -->

# tree-sitter-lex

Tree-sitter grammar for the Lex document format — syntax highlighting,
language injection, and structural navigation for editors.

## Repo Structure

```
grammar.js          Grammar rules (block structure, inline elements)
src/scanner.c       External scanner (indentation, emphasis flanking)
src/parser.c        Generated parser (run `npx tree-sitter generate`)
queries/
  highlights.scm    Highlight capture groups
  injections.scm    Embedded language injection (verbatim blocks)
  textobjects.scm   nvim-treesitter structural selection
test/corpus/        Tree-sitter corpus tests
bin/
  check             Single entry point — runs all checks (used by CI)
app-bin/
  parity-print.js   Converts tree-sitter XML to parity format
  parity-ignored.txt  Acknowledged parity divergences (bats skip)
  download-lexd-cli.sh Downloads lexd binary
  bump-grammars.sh  Quarterly grammar dependency bump
test/
  corpus/           Tree-sitter corpus tests
  helpers.bash      Shared bats helpers (assert_no_errors, assert_parity)
  generate-tests.sh Generates bats tests from spec fixtures
  generated/        Auto-generated .bats files (gitignored)
comms/              Submodule → lex-fmt/comms (grammar specs, test fixtures)
shared/
  lex-deps.json     Pins lexd version
```

## Development

```sh
npm install                  # install tree-sitter CLI (one time)
bin/check                                             # run ALL checks (same as CI)
bin/check --quick                                     # skip parity (for rapid iteration)
npx tree-sitter test                                  # just corpus tests
npx bats test/generated/no-errors.bats                # just error-free parsing (after generate)
npx bats test/generated/parity.bats                   # just parity (after generate, needs LEX_CLI)
npx bats --filter "annotation-01" test/generated/     # single file by name
npx bats --filter "fullwidth" test/generated/         # pattern match
```

## Testing Philosophy

One entry point (`bin/check`) runs the same checks everywhere — CI
and manual. No silent skips, no context-dependent behavior. If a dependency is
needed, it's fetched automatically.

Three checks, clear semantics:
- **tree-shape** (inline in bin/check): does the grammar produce expected tree structures? (corpus tests via `npx tree-sitter test`)
- **test-no-errors**: can tree-sitter parse all spec documents without ERROR nodes? (bats)
- **test-parity**: does tree-sitter's CST match lex-core's AST? (bats)

Pass means pass, fail means fail. Parity divergences in `parity-ignored.txt` are
acknowledged failures — bats reports them as "skipped", not "passing."

test-no-errors and test-parity use [bats-core](https://github.com/bats-core/bats-core)
— each spec fixture is an individual test case with TAP output.

## Releasing

This repo participates in the lex release cascade. Cutting a release here is triggered automatically when comms releases (via the `on-upstream-released` handler workflow). Once cut, it cascades to vscode + nvim + lexed via `notify-downstreams`.

For a manual cut: push an annotated tag (`git tag -a vX.Y.Z -m "..." && git push origin vX.Y.Z`). CI builds `tree-sitter.tar.gz` (parser sources, WASM module, query files); editor repos download this artifact via `shared/lex-deps.json`.

Design + ops + gotchas: [arthur-debert/release/docs/lex-release-cascade.md](https://github.com/arthur-debert/release/blob/main/docs/lex-release-cascade.md).

## Architecture

Two-parser design: tree-sitter for CST features (sync, in-editor), lex-core
for AST/semantic features (async, via LSP). No converter between them.

## Related repos

- [lex-fmt/lex](https://github.com/lex-fmt/lex) — Rust workspace (parser, LSP, CLI)
- [lex-fmt/comms](https://github.com/lex-fmt/comms) — Specs, docs (submoduled here)
- [lex-fmt/vscode](https://github.com/lex-fmt/vscode) — VS Code extension
- [lex-fmt/nvim](https://github.com/lex-fmt/nvim) — Neovim plugin
