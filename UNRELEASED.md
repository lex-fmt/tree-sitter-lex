<!-- Release notes for the next version. -->
<!-- Updated as work is done; consumed by scripts/create-release. -->

- release: the grammar `.conda` now carries the full editor **union** —
  `tree-sitter-lex.wasm`, `queries/`, `shared/embedded-grammars.json` and the C
  `src/` — under `$PREFIX/share/tree-sitter/`, restoring the v0.11.2 payload the
  first shipit-managed cut dropped. Editors consume one resolvable package
  instead of a wasm fetch plus a source tarball (conda-direct, ADR-0077 /
  shipit#1092 T4).
- release: pixi provisions an **emscripten wasm backend** (`3.1.58`, target-scoped
  to linux-64/osx-64/osx-arm64) so the bundle stage's `tree-sitter build --wasm`
  has a compiler. Pinned off 4.0.9 deliberately: conda-forge's 4.0.9 declares
  `binaryen >=117,<118` while its emcc requires binaryen 123, so every wasm build
  dies in `wasm-opt` on an unknown `--no-stack-ir`.
