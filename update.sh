#!/bin/sh
# Skrynja update script - run on the Sailfish OS device
# Usage: sh update.sh [version]
# Example: sh update.sh 0.9.1
# Default: latest release from GitHub

set -e

# Target device: Jolla Phone (2026) - aarch64 only
ARCH="aarch64"

if [ -n "$1" ]; then
    VERSION="$1"
else
    VERSION=$(curl -sf "https://api.github.com/repos/storefyr/Skrynja/releases/latest" \
        | grep -o '"tag_name": *"[^"]*"' | head -1 | cut -d'"' -f4 | sed 's/^v//')
    if [ -z "$VERSION" ]; then
        echo "ERROR: Could not fetch latest version from GitHub." >&2
        echo "Usage: sh update.sh <version>" >&2
        exit 1
    fi
fi

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
devel-su pkcon install-local --allow-reinstall "${TMP_DIR}/${RPM_NAME}"

echo "--- Step 3: Clean up ---"
rm -f "${TMP_DIR}/${RPM_NAME}"

echo ""
echo "=== Done! ==="
echo "Skrynja ${VERSION} is installed. Launch it from the app grid."
