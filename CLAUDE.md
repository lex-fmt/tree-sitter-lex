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
scripts/
  check             Single entry point — runs all checks (used by pre-commit and CI)
  test-tree-shape   Corpus tests (tree structure correctness)
  test-no-errors    Parse all spec fixtures, fail on any ERROR node
  test-parity       Compare CST with lex-core AST (downloads lex-cli if needed)
  parity-print.js   Converts tree-sitter XML to parity format
  parity-ignored.txt  Acknowledged parity divergences (not blocking CI)
  download-lex-cli.sh  Downloads lex-cli binary
comms/              Submodule → lex-fmt/comms (grammar specs, test fixtures)
shared/
  lex-deps.json     Pins lex-cli version
```

## Development

```sh
npm install                  # install tree-sitter CLI (one time)
./scripts/test-all              # run ALL checks (same as pre-commit and CI)
./scripts/test-all --quick      # skip parity (for rapid iteration)
./scripts/test-tree-shape    # just corpus tests
./scripts/test-no-errors     # just error-free parsing
./scripts/test-parity        # just parity comparison
```

## Testing Philosophy

One entry point (`scripts/test-all`) runs the same checks everywhere — pre-commit,
CI, manual. No silent skips, no context-dependent behavior. If a dependency is
needed, it's fetched automatically.

Three checks, clear semantics:
- **test-tree-shape**: does the grammar produce expected tree structures? (corpus tests)
- **test-no-errors**: can tree-sitter parse all spec documents without ERROR nodes?
- **test-parity**: does tree-sitter's CST match lex-core's AST?

Pass means pass, fail means fail. Parity divergences in `parity-ignored.txt` are
acknowledged failures — they don't block CI but they're not "passing."

## Pre-commit hook

Install: `ln -sf ../../scripts/pre-commit .git/hooks/pre-commit`

## Releasing

Tag with `vX.Y.Z` and push. CI builds `tree-sitter.tar.gz` with parser
sources, WASM module, and query files. Editor repos download this artifact.

## Architecture

Two-parser design: tree-sitter for CST features (sync, in-editor), lex-core
for AST/semantic features (async, via LSP). No converter between them.

## Related repos

- [lex-fmt/lex](https://github.com/lex-fmt/lex) — Rust workspace (parser, LSP, CLI)
- [lex-fmt/comms](https://github.com/lex-fmt/comms) — Specs, docs (submoduled here)
- [lex-fmt/vscode](https://github.com/lex-fmt/vscode) — VS Code extension
- [lex-fmt/nvim](https://github.com/lex-fmt/nvim) — Neovim plugin
