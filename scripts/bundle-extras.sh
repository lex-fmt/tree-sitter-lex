#!/usr/bin/env bash
# bundle-extras.sh — convention hook for arthur-debert/release/.github/workflows/tree-sitter.yml.
#
# Runs after the canonical tree-sitter bundle has been assembled in
# ${BUNDLE_DIR}, BEFORE the tarball is created. Use to add consumer-
# specific files to the bundle and to gate the release on consumer-
# specific invariants.
#
# For tree-sitter-lex, we ship `shared/embedded-grammars.json` so
# downstream editors can drive their grammar-fetch logic from this
# upstream manifest. We also run scripts/smoke-grammars.sh as a
# release-gate: a release that ships a manifest pointing at
# missing WASM / queries / license would silently break the
# editors at install time, which is the worst possible failure
# mode.
#
# Env contract (set by tree-sitter.yml's build job):
#   BUNDLE_DIR  absolute path to the in-progress dist/ directory
#   PARSER      bare parser name (e.g. `lex`)

set -euo pipefail

# Smoke-check the embedded-grammars manifest BEFORE bundling it.
# Failure here aborts the build → no tag-push-vs-build-failure window
# (corpus-test already ran successfully in a prior job; this is the
# remaining release-time invariant).
if [ -f scripts/smoke-grammars.sh ]; then
  echo "→ smoke-checking shared/embedded-grammars.json"
  bash scripts/smoke-grammars.sh
fi

# Ship the manifest itself so editors can read it at install time.
if [ -f shared/embedded-grammars.json ]; then
  mkdir -p "${BUNDLE_DIR}/shared"
  cp shared/embedded-grammars.json "${BUNDLE_DIR}/shared/"
  echo "→ added shared/embedded-grammars.json to bundle"
fi
