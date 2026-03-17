# Shared helpers for bats tests.
# Loaded via: load '../helpers' in .bats files (from test/generated/).

# Resolve repo root from THIS file's location (test/helpers.bash → repo root)
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Parse a .lex file and fail if any ERROR node is found.
assert_no_errors() {
    local file="$1"
    local parse_output=""
    parse_output="$(npx tree-sitter parse "$REPO_DIR/$file" 2>&1 || :)"
    if echo "$parse_output" | grep "ERROR" >/dev/null 2>&1; then
        local count
        count="$(echo "$parse_output" | grep -c "ERROR" || :)"
        echo "Found $count ERROR node(s) in $file" >&2
        echo "$parse_output" | grep -B1 "ERROR" | head -20 >&2
        return 1
    fi
}

# Compare tree-sitter parity output with lex-core.
# Requires LEX_CLI to be set (test-all handles this).
assert_parity() {
    local file="$1"
    local lex_output="" ts_output=""

    lex_output="$("$LEX_CLI" inspect "$REPO_DIR/$file" parity 2>/dev/null || :)"
    if [[ -z "$lex_output" ]]; then
        echo "lex-cli produced no output for $file" >&2
        return 1
    fi

    ts_output="$(cd "$REPO_DIR" && npx tree-sitter parse -x "$file" 2>/dev/null \
        | node "$REPO_DIR/scripts/parity-print.js" 2>/dev/null || :)"
    if [[ -z "$ts_output" ]]; then
        echo "tree-sitter/parity-print produced no output for $file" >&2
        return 1
    fi

    if ! diff <(echo "$lex_output") <(echo "$ts_output") >/dev/null 2>&1; then
        echo "Parity mismatch for $file" >&2
        diff --unified=3 <(echo "$lex_output") <(echo "$ts_output") | head -30 >&2
        return 1
    fi
}
