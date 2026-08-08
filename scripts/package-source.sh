#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${1:-$ROOT/out/packages}"
VERSION="$(sed -n 's/.*version_string = "\([^"]*\)".*/\1/p' "$ROOT/include/epochengine/particle/version.hpp")"
[[ -n "$VERSION" ]] || { echo "Could not read version" >&2; exit 1; }
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
ARCHIVE="$OUTPUT_DIR/EpochParticleEngine-v${VERSION}-source.zip"
STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT
DEST="$STAGING/EpochParticleEngine-v${VERSION}"
mkdir -p "$DEST"

rsync -a \
    --exclude '.git/' \
    --exclude '.vs/' \
    --exclude 'build/' \
    --exclude 'build-*/' \
    --exclude 'out/' \
    "$ROOT/" "$DEST/"

rm -f "$ARCHIVE"
(
    cd "$STAGING"
    zip -qr "$ARCHIVE" "EpochParticleEngine-v${VERSION}"
)
(
    cd "$OUTPUT_DIR"
    sha256sum "$(basename "$ARCHIVE")" > "$(basename "$ARCHIVE").sha256"
)
echo "$ARCHIVE"
echo "$ARCHIVE.sha256"
