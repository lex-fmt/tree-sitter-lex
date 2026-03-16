# tree-sitter-lex

Tree-sitter grammar for the [Lex](https://lex.ing) document format.

Provides syntax highlighting, embedded language injection, and structural
navigation (textobjects) for editors that support tree-sitter.

## What this provides

| Feature | Query file |
|---------|-----------|
| Syntax highlighting | `queries/highlights.scm` |
| Embedded language injection | `queries/injections.scm` |
| Structural selection (nvim) | `queries/textobjects.scm` |

This grammar does NOT replace the Lex LSP server (`lex-lsp`), which provides
semantic features like diagnostics, go-to-definition, hover, completion, and
formatting via `lex-core`.

## Editor integration

### Neovim (nvim-treesitter)

Add to your nvim-treesitter config:

```lua
require("nvim-treesitter.parsers").get_parser_configs().lex = {
  install_info = {
    url = "https://github.com/lex-fmt/tree-sitter-lex",
    files = { "src/parser.c", "src/scanner.c" },
  },
  filetype = "lex",
}
```

Then `:TSInstall lex`. For the full Lex editing experience (LSP, commands,
themes), use [lex-fmt/nvim](https://github.com/lex-fmt/nvim) instead — it
handles tree-sitter setup automatically.

### VS Code

The [lex-fmt/vscode](https://github.com/lex-fmt/vscode) extension bundles a
pre-built WASM module from this repo's release artifacts. No manual setup needed.

### Other editors

The release artifact `tree-sitter.tar.gz` contains everything needed:

- `src/parser.c`, `src/scanner.c` — C sources (compile locally)
- `tree-sitter-lex.wasm` — pre-built WASM module
- `queries/*.scm` — highlight, injection, and textobject queries

## Development

```sh
npm install                     # install tree-sitter CLI (one time)
npx tree-sitter generate       # regenerate parser.c from grammar.js
npx tree-sitter test           # run corpus tests (test/corpus/*.txt)
./scripts/error-check.sh       # parse all spec fixtures, check for errors
```

### Parity testing

The parity check compares tree-sitter's CST with lex-core's AST to verify
structural agreement. It requires the `lex` CLI binary:

```sh
./scripts/download-lex-cli.sh  # download pinned lex-cli version
LEX_CLI_PATH=./bin/lex ./scripts/parity-check.sh
```

### Pre-commit hook

```sh
ln -sf ../../scripts/pre-commit .git/hooks/pre-commit
```

Runs: generate, corpus tests, error-check. Parity check runs if `lex` is available.

### Architecture: two parsers, different jobs

```
Tree-sitter (sync, in editor)       lex-core via lex-lsp (async)
- Syntax highlighting               - Semantic tokens (overrides TS)
- Embedded language injection        - Diagnostics
- nvim textobjects                   - Go-to-definition
- bat / GitHub / difftastic          - Hover, completion, formatting
```

No converter between them. Each serves its own purpose.

## Release

Tag with `vX.Y.Z` and push. CI builds `tree-sitter.tar.gz` containing the
parser sources, WASM module, and query files. Editor repos
([vscode](https://github.com/lex-fmt/vscode),
[nvim](https://github.com/lex-fmt/nvim)) download this artifact using the
version pinned in their `shared/lex-deps.json`.

## Related repos

| Repo | Purpose |
|------|---------|
| [lex-fmt/lex](https://github.com/lex-fmt/lex) | Rust workspace: parser, LSP, CLI, format conversion |
| [lex-fmt/comms](https://github.com/lex-fmt/comms) | Grammar specs, docs, assets (submoduled here) |
| [lex-fmt/vscode](https://github.com/lex-fmt/vscode) | VS Code extension |
| [lex-fmt/nvim](https://github.com/lex-fmt/nvim) | Neovim plugin |
| [lex-fmt/lexed](https://github.com/lex-fmt/lexed) | Electron desktop editor |
