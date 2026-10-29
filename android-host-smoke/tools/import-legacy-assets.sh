#!/usr/bin/env bash
# Import the legacy WebView assets from the old APK.
# Usage: bash tools/import-legacy-assets.sh [path-to-apk]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APK="${1:-${LEGACY_APK:-D:/zg/apk114-bridge/output/apk114-minimal-r129b2-signed.apk}}"

test -f "$APK" || { echo "legacy APK not found: $APK" >&2; exit 2; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
unzip -q "$APK" -d "$TMP"
test -d "$TMP/assets" || { echo "assets/ missing in APK" >&2; exit 2; }
rm -rf "$ROOT/app/src/main/assets"
cp -R "$TMP/assets" "$ROOT/app/src/main/assets"
test -f "$ROOT/app/src/main/assets/index.html" || {
  echo "assets/index.html missing after import" >&2
  exit 2
}
echo "imported $(find "$ROOT/app/src/main/assets" -type f | wc -l) files from $APK"
