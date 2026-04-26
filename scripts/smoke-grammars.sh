#!/usr/bin/env bash
set -euo pipefail

# Smoke-check every grammar listed in shared/embedded-grammars.json.
#
# For each entry, verifies that:
#   1. <repo>/releases/download/<version>/<wasm_asset>  → 200 OK
#   2. raw.githubusercontent.com/<repo>/<version>/<queries_path>  → 200 OK
#   3. raw.githubusercontent.com/<repo>/<version>/LICENSE          → 200 OK
#
# This catches the most common upstream breakage modes before downstream
# editors discover them at build time: a release that didn't ship the WASM,
# a tag that was retracted, queries renamed/moved, license file removed.
#
# It is intentionally a HEAD-only check, not a load test. A future
# enhancement could download each WASM and parse a minimal fixture via
# web-tree-sitter to catch ABI mismatches as well.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MANIFEST="$REPO_DIR/shared/embedded-grammars.json"

if [[ ! -f "$MANIFEST" ]]; then
  echo "error: $MANIFEST not found" >&2
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "error: jq is required (apt-get install jq / brew install jq)" >&2
  exit 1
fi

curl_opts=(-s -o /dev/null -w "%{http_code}" -L --max-time 20)
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
  curl_opts+=(-H "Authorization: Bearer $GITHUB_TOKEN")
fi

check_url() {
  local url="$1"
  local code
  code=$(curl "${curl_opts[@]}" "$url" || echo "000")
  if [[ "$code" == "200" ]]; then
    return 0
  fi
  printf '    [%s] %s\n' "$code" "$url" >&2
  return 1
}

count=$(jq '.grammars | length' "$MANIFEST")
failed=0

echo "Smoke-checking $count grammar(s) from $MANIFEST"
echo

for i in $(seq 0 $((count - 1))); do
  name=$(jq -r ".grammars[$i].name" "$MANIFEST")
  version=$(jq -r ".grammars[$i].version" "$MANIFEST")
  repo=$(jq -r ".grammars[$i].repo" "$MANIFEST")
  wasm_asset=$(jq -r ".grammars[$i].wasm_asset" "$MANIFEST")
  queries_path=$(jq -r ".grammars[$i].queries_path" "$MANIFEST")

  wasm_url="https://github.com/$repo/releases/download/$version/$wasm_asset"
  queries_url="https://raw.githubusercontent.com/$repo/$version/$queries_path"
  license_url="https://raw.githubusercontent.com/$repo/$version/LICENSE"

  printf '  %-12s %-10s ' "$name" "$version"

  ok=true
  check_url "$wasm_url"     || ok=false
  check_url "$queries_url"  || ok=false
  check_url "$license_url"  || ok=false

  if $ok; then
    echo "OK"
  else
    echo "FAIL"
    failed=$((failed + 1))
  fi
done

echo
if [[ $failed -gt 0 ]]; then
  echo "smoke-grammars: $failed of $count grammar(s) failed" >&2
  exit 1
fi
echo "smoke-grammars: all $count grammar(s) reachable"
