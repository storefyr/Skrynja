#!/bin/sh
# Skrynja update script - run on the Sailfish OS device
# Usage: sh update.sh [version]
# Example: sh update.sh 0.5.8

set -e

VERSION="${1:-0.8.5}"
ARCH="aarch64"
RPM_NAME="harbour-skrynja-${VERSION}-1.${ARCH}.rpm"
DOWNLOAD_URL="https://github.com/storefyr/Skrynja/releases/download/v${VERSION}/${RPM_NAME}"
TMP_DIR="/tmp"

echo "=== Skrynja Update Script ==="
echo "Version: ${VERSION}"
echo "Architecture: ${ARCH}"
echo ""

echo "--- Step 1: Download new RPM ---"
cd "${TMP_DIR}"
curl -LO "${DOWNLOAD_URL}"
echo "Downloaded: ${RPM_NAME}"

echo "--- Step 2: Install new version ---"
devel-su pkcon install-local "${TMP_DIR}/${RPM_NAME}"

echo "--- Step 3: Clean up ---"
rm -f "${TMP_DIR}/${RPM_NAME}"

echo ""
echo "=== Done! ==="
echo "Skrynja ${VERSION} is installed. Launch it from the app grid."
