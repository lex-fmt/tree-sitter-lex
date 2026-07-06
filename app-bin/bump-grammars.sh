#!/usr/bin/env bash
set -euo pipefail

# Bumps every grammar in shared/embedded-grammars.json to the latest
# upstream non-prerelease tag. Prints one human-readable line per grammar
# that actually changed (`<lang>: <old> -> <new>`); prints nothing when
# every entry is already current.
#
# Driven by .github/workflows/quarterly-grammar-bump.yml on the first of
# Jan/Apr/Jul/Oct, so a stale upstream version surfaces as a PR rather
# than rotting silently. Smoke-grammars.sh runs after this script in the
# same workflow to catch breakage before the PR opens.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MANIFEST="$REPO_DIR/shared/embedded-grammars.json"

if [[ ! -f "$MANIFEST" ]]; then
	echo "error: $MANIFEST not found" >&2
	exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
	echo "error: jq is required" >&2
	exit 1
fi

curl_opts=(-fsSL --max-time 20)
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
	curl_opts+=(-H "Authorization: Bearer $GITHUB_TOKEN")
fi

count=$(jq '.grammars | length' "$MANIFEST")

for i in $(seq 0 $((count - 1))); do
	name=$(jq -r ".grammars[$i].name" "$MANIFEST")
	current=$(jq -r ".grammars[$i].version" "$MANIFEST")
	repo=$(jq -r ".grammars[$i].repo" "$MANIFEST")

	# /releases/latest skips prereleases and drafts, which is what we want
	# — we only bump to versions upstream considers shippable.
	latest=$(curl "${curl_opts[@]}" \
		"https://api.github.com/repos/$repo/releases/latest" |
		jq -r '.tag_name // empty')

	if [[ -z "$latest" ]]; then
		echo "warning: could not resolve latest release for $repo (skipping)" >&2
		continue
	fi

	if [[ "$current" != "$latest" ]]; then
		tmp=$(mktemp)
		jq ".grammars[$i].version = \"$latest\"" "$MANIFEST" >"$tmp"
		mv "$tmp" "$MANIFEST"
		echo "$name: $current -> $latest"
	fi
done
