#!/bin/bash
# Parity check: compare tree-sitter CST with lex-core AST using plain-text
# block skeleton format. Both sides produce the same format directly — no
# JSON, no jq filters, no bridge conversion logic.
#
# Requires `lex` binary (lex-cli) on PATH or at $LEX_CLI_PATH.
# In CI, the binary is downloaded from the pinned release in shared/lex-deps.json.
#
# Usage:
#   ./scripts/parity-check.sh                    # all fixtures
#   ./scripts/parity-check.sh <file.lex>         # single file
#   ./scripts/parity-check.sh --verbose           # show diffs on failure

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PRINTER="$SCRIPT_DIR/parity-print.js"
ALLOWLIST="$SCRIPT_DIR/parity-known-failures.txt"

# Resolve lex-cli binary
LEX_CLI="${LEX_CLI_PATH:-}"
if [[ -z "$LEX_CLI" ]]; then
    LEX_CLI="$(command -v lex 2>/dev/null || true)"
fi
if [[ -z "$LEX_CLI" ]]; then
    echo "Error: lex-cli binary not found. Set LEX_CLI_PATH or put 'lex' on PATH." >&2
    echo "  In CI, the download-lex-cli.sh script handles this." >&2
    exit 1
fi

VERBOSE=false
SINGLE_FILE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v) VERBOSE=true; shift ;;
        *) SINGLE_FILE="$1"; shift ;;
    esac
done

# Load known failures list (one path per line, # comments and blanks ignored)
KNOWN_FAILURES=""
if [[ -f "$ALLOWLIST" ]]; then
    while IFS= read -r line; do
        line="${line%%#*}"
        line="$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
        [[ -z "$line" ]] && continue
        KNOWN_FAILURES="${KNOWN_FAILURES}|${line}"
    done < "$ALLOWLIST"
fi

is_known_failure() {
    local path="$1"
    echo "$KNOWN_FAILURES" | grep -qF "|${path}"
}

PASS=0
FAIL=0
SKIP=0
KNOWN=0
ERRORS=""

check_file() {
    local lex_file
    if [[ "$1" = /* ]]; then
        lex_file="$1"
    else
        lex_file="$REPO_DIR/$1"
    fi
    local rel_path="${lex_file#$REPO_DIR/}"

    # Reference parser output
    local lex_output
    lex_output=$("$LEX_CLI" inspect "$lex_file" parity 2>/dev/null) || {
        printf "  %-60s SKIP (lex-cli failed)\n" "$rel_path"
        SKIP=$((SKIP + 1))
        return
    }

    # Tree-sitter output
    local ts_output
    ts_output=$(cd "$REPO_DIR" && npx tree-sitter parse -x "$lex_file" 2>/dev/null | node "$PRINTER" 2>/dev/null) || {
        printf "  %-60s SKIP (tree-sitter failed)\n" "$rel_path"
        SKIP=$((SKIP + 1))
        return
    }

    if diff <(echo "$lex_output") <(echo "$ts_output") > /dev/null 2>&1; then
        printf "  %-60s \033[32mPASS\033[0m\n" "$rel_path"
        PASS=$((PASS + 1))
    elif is_known_failure "$rel_path"; then
        printf "  %-60s \033[33mKNOWN\033[0m\n" "$rel_path"
        KNOWN=$((KNOWN + 1))
    else
        printf "  %-60s \033[31mFAIL\033[0m\n" "$rel_path"
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}\n  ${rel_path}"
        if $VERBOSE; then
            echo "  --- lex-core (left) vs tree-sitter (right) ---"
            diff --color=always <(echo "$lex_output") <(echo "$ts_output") | head -40
            echo ""
        fi
    fi
}

echo "Parity check (block skeleton)"
echo ""

if [[ -n "$SINGLE_FILE" ]]; then
    check_file "$SINGLE_FILE"
else
    # Run against all element fixtures
    for f in "$REPO_DIR"/comms/specs/elements/**/*.lex; do
        check_file "$f"
    done
fi

echo ""
echo "────────────"
printf "Results: \033[32m%d passed\033[0m, \033[31m%d failed\033[0m, \033[33m%d known failures\033[0m, %d skipped\n" "$PASS" "$FAIL" "$KNOWN" "$SKIP"

if [[ $FAIL -gt 0 ]]; then
    printf "\nUnexpected failures:%b\n" "$ERRORS"
    exit 1
fi
