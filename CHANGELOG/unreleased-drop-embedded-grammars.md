- release: `shared/embedded-grammars.json` is **gone from the package**. The
  curated list of five third-party grammars (python, javascript, json, rust,
  bash) that editors were told to bundle for `:: lang ::` injection is deleted,
  along with its `[artifacts.tree-sitter.bundle]` payload entry. This repo now
  ships the Lex grammar and its queries and nothing else, and no longer tracks
  any foreign grammar's release cadence. Injected-language resolution belongs to
  the host editor — see `docs/adr/0001-injected-language-resolution-belongs-to-the-host.md`.
  Consumers were cut over first: `lex-fmt/lexed` reverted to Monaco Monarch
  (lex-fmt/lexed#178, 5 → 82 languages at lower fidelity), and `lex-fmt/vscode`
  now takes the tree-sitter org's official per-language npm packages
  (lex-fmt/vscode#166). `queries/injections.scm` is **byte-identical** — this
  removes packaging, not behaviour; injection still works for any language the
  host can resolve.
- chore: the machinery that existed only to maintain that manifest is deleted —
  `app-bin/bump-grammars.sh`, `app-bin/smoke-grammars.sh`, the quarterly
  `.github/workflows/quarterly-grammar-bump.yml` bump workflow, the
  `smoke-grammars.sh` leg of the `test-full` task, and the `jq` dependency that
  backed it (nothing else in the repo used it).
- docs: ADR-0001 corrected on one empirical point. It recommended
  `@repomix/tree-sitter-wasms` / `@sourcegraph/tree-sitter-wasms` as vscode's
  parser source; both fail to load under the `web-tree-sitter@0.26.8` vscode
  pins (built with tree-sitter-cli 0.20/0.21, rejected in `getDylinkMetadata`),
  and neither covers the needed set. The ADR now names what WS02 actually
  shipped: the official per-language `tree-sitter-<lang>` packages, which carry
  the wasm, the highlights query and the LICENSE. The decision and its reasoning
  are unchanged.
- docs: `README.md`'s `tree-sitter.tar.gz` description now matches the declared
  payload (it listed neither `grammar.js` nor `package.json`), and states
  explicitly that the tarball ships no third-party grammars. Its Development and
  Parity testing blocks pointed at `bin/check`, which has not existed since the
  shipit cutover — now `app-bin/test-all`, as does that script's own usage
  header.
