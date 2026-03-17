#!/bin/bash
# Generate .bats test files from spec fixtures.
#
# Creates:
#   test/generated/no-errors.bats  — one test per .lex file (assert no ERROR nodes)
#   test/generated/parity.bats     — one test per element .lex file (assert CST matches AST)
#
# The parity test uses parity-ignored.txt: ignored files get a # skip annotation
# so bats reports them as skipped (visible but not blocking).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/generated"
IGNORE_LIST="$REPO_DIR/scripts/parity-ignored.txt"

mkdir -p "$OUT_DIR"

# --- Load ignored files ---
IGNORED=""
if [[ -f "$IGNORE_LIST" ]]; then
    while IFS= read -r line; do
        line="${line%%#*}"
        line="$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
        [[ -z "$line" ]] && continue
        IGNORED="${IGNORED}|${line}"
    done < "$IGNORE_LIST"
fi

is_ignored() {
    echo "$IGNORED" | grep -qF "|${1}"
}

# --- Generate no-errors.bats ---
{
    echo '#!/usr/bin/env bats'
    echo ''
    echo 'load "../helpers"'
    echo ''
    while IFS= read -r f; do
        rel="${f#$REPO_DIR/}"
        name=$(echo "$rel" | sed 's|comms/specs/||; s|\.lex$||')
        echo "@test \"no-errors: $name\" {"
        echo "    assert_no_errors \"$rel\""
        echo "}"
        echo ""
    done < <(find "$REPO_DIR/comms/specs" -name "*.lex" | sort)
} > "$OUT_DIR/no-errors.bats"

# --- Generate parity.bats ---
{
    echo '#!/usr/bin/env bats'
    echo ''
    echo 'load "../helpers"'
    echo ''
    for f in "$REPO_DIR"/comms/specs/elements/**/*.lex; do
        rel="${f#$REPO_DIR/}"
        name=$(echo "$rel" | sed 's|comms/specs/elements/||; s|\.lex$||')
        if is_ignored "$rel"; then
            echo "# bats test_tags=ignored"
            echo "@test \"parity: $name\" {"
            echo "    skip \"acknowledged divergence\""
            echo "}"
        else
            echo "@test \"parity: $name\" {"
            echo "    assert_parity \"$rel\""
            echo "}"
        fi
        echo ""
    done
} > "$OUT_DIR/parity.bats"

echo "Generated: $OUT_DIR/no-errors.bats ($(grep -c '@test' "$OUT_DIR/no-errors.bats") tests)"
echo "Generated: $OUT_DIR/parity.bats ($(grep -c '@test' "$OUT_DIR/parity.bats") tests)"
