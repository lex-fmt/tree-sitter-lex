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
  pre-commit        Pre-commit hook
  error-check.sh    Validates all spec fixtures parse without ERROR nodes
  parity-check.sh   Compares CST with lex-core AST (needs lex-cli binary)
  parity-print.js   Converts tree-sitter XML output to parity format
  download-lex-cli.sh  Downloads lex-cli binary for parity testing
comms/              Submodule → lex-fmt/comms (grammar specs, test fixtures)
shared/
  lex-deps.json     Pins lex-cli version for parity testing
```

## Development

```sh
npm install                      # install tree-sitter CLI (one time)
npx tree-sitter generate        # regenerate parser.c from grammar.js
npx tree-sitter test            # run corpus tests
./scripts/error-check.sh        # parse spec fixtures, check for ERROR nodes
./scripts/download-lex-cli.sh   # download lex-cli for parity testing
LEX_CLI_PATH=./bin/lex ./scripts/parity-check.sh  # compare CST vs AST
```

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
