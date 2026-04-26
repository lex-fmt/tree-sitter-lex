<!-- Release notes for the next version. -->
<!-- Updated as work is done; consumed by scripts/create-release. -->

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
