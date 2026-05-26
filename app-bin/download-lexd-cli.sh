#!/usr/bin/env bash
set -euo pipefail

# Downloads the lexd CLI binary from GitHub releases for parity testing.
# Reads version from shared/lex-deps.json.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEPS_FILE="$REPO_DIR/shared/lex-deps.json"
BIN_DIR="$REPO_DIR/bin"

if [[ ! -f "$DEPS_FILE" ]]; then
    echo "Error: $DEPS_FILE not found" >&2
    exit 1
fi

LEXD_VERSION="$(jq -r '.["lexd-cli"]' "$DEPS_FILE")"
LEXD_REPO="$(jq -r '.["lexd-cli-repo"]' "$DEPS_FILE")"

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
        ;;
    Darwin)
        case "$ARCH" in
            x86_64)  TARGET="x86_64-apple-darwin" ;;
            arm64)   TARGET="aarch64-apple-darwin" ;;
            *)       echo "Unsupported macOS arch: $ARCH" >&2; exit 1 ;;
        esac
        ;;
    *)
        echo "Unsupported OS: $OS" >&2
        exit 1
        ;;
esac

ARCHIVE_BASENAME="lexd-${TARGET}"
ARCHIVE_NAME="${ARCHIVE_BASENAME}.tar.gz"
BINARY_NAME="lexd"
OUTPUT="$BIN_DIR/$BINARY_NAME"

# Check if already downloaded
if [[ -f "$OUTPUT" ]]; then
    echo "lexd already exists at $OUTPUT"
    echo "$OUTPUT"
    exit 0
fi

echo "Downloading lexd $LEXD_VERSION for $TARGET..."

DOWNLOAD_URL="https://github.com/${LEXD_REPO}/releases/download/${LEXD_VERSION}/${ARCHIVE_NAME}"

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

# v0.8.0+ archives expand to a directory (lexd-<target>/lexd); older lex
# archives put the binary at the archive root. Handle both layouts.
if [[ -f "$TMP_DIR/$ARCHIVE_BASENAME/$BINARY_NAME" ]]; then
    SRC="$TMP_DIR/$ARCHIVE_BASENAME/$BINARY_NAME"
elif [[ -f "$TMP_DIR/$BINARY_NAME" ]]; then
    SRC="$TMP_DIR/$BINARY_NAME"
else
    echo "Could not find $BINARY_NAME binary in archive" >&2
    rm -rf "$TMP_DIR"
    exit 1
fi

cp "$SRC" "$OUTPUT"
chmod +x "$OUTPUT"
rm -rf "$TMP_DIR"

echo "lexd $LEXD_VERSION installed to $OUTPUT"
echo "$OUTPUT"
