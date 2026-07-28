# tree-sitter-lex

Tree-sitter grammar for the [Lex](https://lex.ing) document format.

Provides syntax highlighting, embedded language injection, and structural
navigation (textobjects) for editors that support tree-sitter.

## What this provides

| Feature | Query file |
| --------- | ----------- |
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

- `src/` — the generated C parser sources (`parser.c`, `scanner.c`,
  `grammar.json`, `node-types.json`, `tree_sitter/`) to compile locally
- `queries/` — highlight, injection, fold, and textobject queries
- `grammar.js` — the grammar source `src/` is generated from
- `package.json` — grammar metadata (scope, file types, query paths)
- `tree-sitter-lex.wasm` — pre-built WASM module

That is the whole payload: the Lex grammar and its queries, nothing else. It
ships **no** third-party grammars for the languages injected into verbatim
blocks — resolving `:: python ::` to a Python parser is the host editor's job,
through its own parser index. See
[ADR-0001](docs/adr/0001-injected-language-resolution-belongs-to-the-host.md).

## Development

```sh
npm install                  # install tree-sitter CLI (one time)
app-bin/test-all             # umbrella check script (the same entry CI's test lane runs)
app-bin/test-all --quick     # skip parity (for rapid iteration)
```

`app-bin/test-all` regenerates the parser, runs the corpus tests, runs the
generated bats `no-errors` suite, and runs parity. See `CLAUDE.md` for finer-
grained invocations (single bats files, filter patterns, etc.).

### Parity testing

The parity check compares tree-sitter's CST with lex-core's AST to verify
structural agreement. It requires the `lexd` CLI binary, which `bin/check`
downloads automatically (pinned version from `shared/lex-deps.json`). To
pre-fetch it manually, or to use an existing `lexd` binary:

```sh
fetch-deps --if-missing lexd-cli      # download pinned lexd into ./deps/lexd
LEX_CLI_PATH=/path/to/lexd bin/check  # or point at an existing lexd
```

### Architecture: two parsers, different jobs

```text
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
| ------ | --------- |
| [lex-fmt/lex](https://github.com/lex-fmt/lex) | Rust workspace: parser, LSP, CLI, format conversion |
| [lex-fmt/comms](https://github.com/lex-fmt/comms) | Grammar specs, docs, assets (submoduled here) |
| [lex-fmt/vscode](https://github.com/lex-fmt/vscode) | VS Code extension |
| [lex-fmt/nvim](https://github.com/lex-fmt/nvim) | Neovim plugin |
| [lex-fmt/lexed](https://github.com/lex-fmt/lexed) | Electron desktop editor |
