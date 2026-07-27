#!/usr/bin/env bash
# build-grammar.sh — this repo's BUILD step: produce every file the release
# payload declares as ours to make.
#
# .shipit.toml's [artifacts.tree-sitter.bundle] payload names two build
# outputs — the generated C parser under `src/` and `tree-sitter-lex.wasm` —
# and shipit's bundle stage COLLECTS that payload, it never produces it
# (conda-direct, shipit ADR-0077 / #1092: the producer repo states its own
# contents and builds them). A required entry that is absent at bundle time is
# a loud release failure, so both outputs must exist when the bundle runs.
#
# This script is the [toolchains] build slot, so `shipit build` (the release's
# build stage) and `pixi run build-grammar` (a laptop) execute the same bytes.
# It replaces the two commands the retired `arthur-debert/release`
# `tree-sitter.yml@v3` workflow ran: `tree-sitter generate` was already the
# toolchain's default build slot, but the wasm half was the workflow's and was
# dropped by the shipit cutover with nothing put in its place.
#
# Idempotent: both commands recreate their outputs from `grammar.js` and
# `src/`, so a clean checkout and a repeated run produce identical bytes. The
# committed `src/` is regenerated in place; it stays clean only while
# `tree-sitter.json`'s `metadata.version` matches what the committed parser
# embeds (they are reconciled — regenerate and commit if you change it).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_DIR"

# The wasm backend is a pixi dependency (emscripten, target-scoped — see
# pixi.toml), never installed at run time. Fail HERE, naming the cause, rather
# than letting `tree-sitter build --wasm` fall through to its docker path and
# either hang or die with a message about containers.
if ! command -v emcc >/dev/null 2>&1; then
	echo "error: emcc not found — the wasm backend is provisioned by pixi" >&2
	echo "       (emscripten in pixi.toml). Run this via \`pixi run build-grammar\`" >&2
	echo "       or \`shipit build\` inside the pixi env. Note emscripten has no" >&2
	echo "       conda-forge linux-aarch64 build, so that platform cannot build" >&2
	echo "       the wasm." >&2
	exit 1
fi

echo "→ tree-sitter generate"
tree-sitter generate

echo "→ tree-sitter build --wasm"
tree-sitter build --wasm --output tree-sitter-lex.wasm

echo "✓ built src/parser.c + tree-sitter-lex.wasm"
