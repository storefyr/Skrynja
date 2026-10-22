#!/bin/sh
# Skrynja update script - run on the Sailfish OS device
# Usage: sh update.sh [version] [arch]
# Example: sh update.sh 0.9.1
#          sh update.sh 0.9.1 aarch64   (override auto-detected arch)

set -e

VERSION="${1:-0.9.1}"

# Auto-detect the device architecture unless overridden as the 2nd argument
if [ -n "$2" ]; then
    ARCH="$2"
else
    case "$(uname -m)" in
        armv7l|armv7hl|armv7*) ARCH="armv7hl" ;;
        aarch64)               ARCH="aarch64" ;;
        i?86)                  ARCH="i486" ;;
        *)
            echo "Unknown architecture: $(uname -m)" >&2
            exit 1
            ;;
    esac
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
devel-su pkcon install-local "${TMP_DIR}/${RPM_NAME}"

echo "--- Step 3: Clean up ---"
rm -f "${TMP_DIR}/${RPM_NAME}"

echo ""
echo "=== Done! ==="
echo "Skrynja ${VERSION} is installed. Launch it from the app grid."
