<!-- Release notes for the next version. -->
<!-- Updated as work is done; consumed by scripts/create-release. -->

- release: the grammar package now ships **`tree-sitter-lex.wasm`** again, plus
  `shared/embedded-grammars.json`. `lex-fmt/vscode` and `lex-fmt/lexed` load the
  compiled parser — they cannot compile the C sources the way `lex-fmt/nvim`
  does — so those two entries are what let the editors consume this repo as one
  resolvable conda package. The wasm build was supplied by the retired
  `arthur-debert/release` `tree-sitter.yml@v3` workflow and was dropped by the
  shipit cutover; `app-bin/build-grammar.sh` is now this repo's build step and
  pixi provisions its emscripten backend (`3.1.58`, target-scoped to
  linux-64/osx-64/osx-arm64 — conda-forge has no linux-aarch64 build, and its
  4.0.9 is internally inconsistent: `binaryen >=117,<118` against an emcc that
  requires 123).
- release: the package's contents are now **declared** in `.shipit.toml`
  (`[artifacts.tree-sitter.bundle]` `leg` + `payload`) rather than coming from a
  built-in list inside shipit (conda-direct, shipit ADR-0077 / #1092). Shipping
  one more file is an edit to that list. Reconciled to shipit v1.6.0, the first
  release carrying the producer-declared payload.
- ci: a `build` lane runs `pixi run build-grammar` on every PR, so a broken wasm
  build fails on the PR instead of on a release tag.
- chore: `app-bin/bundle-extras.sh` deleted — the hook for the retired
  `tree-sitter.yml@v3` workflow that nothing had called since the shipit
  cutover. Its two jobs are both covered: the payload declaration ships
  `shared/embedded-grammars.json`, and the test lane already runs the
  `app-bin/smoke-grammars.sh` release gate.
- chore: regenerated `src/parser.c` so its embedded grammar metadata matches
  `tree-sitter.json` (it read `0.6.0` against a declared `0.10.3`, so every
  `tree-sitter generate` dirtied the tree).
