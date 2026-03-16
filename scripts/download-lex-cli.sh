#!/usr/bin/env bash
set -euo pipefail

# Downloads lex-cli binary from GitHub releases for parity testing.
# Reads version from shared/lex-deps.json.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEPS_FILE="$REPO_DIR/shared/lex-deps.json"
BIN_DIR="$REPO_DIR/bin"

if [[ ! -f "$DEPS_FILE" ]]; then
    echo "Error: $DEPS_FILE not found" >&2
    exit 1
fi

LEX_VERSION="$(jq -r '.["lex-cli"]' "$DEPS_FILE")"
LEX_REPO="$(jq -r '.["lex-cli-repo"]' "$DEPS_FILE")"

# Detect platform
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
    Linux)
        case "$ARCH" in
            x86_64)  TARGET="x86_64-unknown-linux-gnu" ;;
            aarch64) TARGET="aarch64-unknown-linux-gnu" ;;
            *)       echo "Unsupported Linux arch: $ARCH" >&2; exit 1 ;;
        esac
        ARCHIVE_EXT="tar.gz"
        BINARY_NAME="lex"
        ;;
    Darwin)
        case "$ARCH" in
            x86_64)  TARGET="x86_64-apple-darwin" ;;
            arm64)   TARGET="aarch64-apple-darwin" ;;
            *)       echo "Unsupported macOS arch: $ARCH" >&2; exit 1 ;;
        esac
        ARCHIVE_EXT="tar.gz"
        BINARY_NAME="lex"
        ;;
    *)
        echo "Unsupported OS: $OS" >&2
        exit 1
        ;;
esac

ARCHIVE_NAME="lex-${TARGET}.${ARCHIVE_EXT}"
OUTPUT="$BIN_DIR/$BINARY_NAME"

# Check if already downloaded
if [[ -f "$OUTPUT" ]]; then
    echo "lex-cli already exists at $OUTPUT"
    echo "$OUTPUT"
    exit 0
fi

echo "Downloading lex-cli $LEX_VERSION for $TARGET..."

DOWNLOAD_URL="https://github.com/${LEX_REPO}/releases/download/${LEX_VERSION}/${ARCHIVE_NAME}"

mkdir -p "$BIN_DIR"
TMP_DIR="$(mktemp -d)"

CURL_OPTS=(-fsSL -o "$TMP_DIR/$ARCHIVE_NAME")
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    CURL_OPTS+=(-H "Authorization: Bearer $GITHUB_TOKEN")
fi

if ! curl "${CURL_OPTS[@]}" "$DOWNLOAD_URL"; then
    echo "Failed to download $DOWNLOAD_URL" >&2
    rm -rf "$TMP_DIR"
    exit 1
fi

tar -xzf "$TMP_DIR/$ARCHIVE_NAME" -C "$TMP_DIR"
cp "$TMP_DIR/$BINARY_NAME" "$OUTPUT"
chmod +x "$OUTPUT"
rm -rf "$TMP_DIR"

echo "lex-cli $LEX_VERSION installed to $OUTPUT"
echo "$OUTPUT"
