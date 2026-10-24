#!/bin/sh
# Skrynja PR test script - run on the Sailfish OS device (Jolla Phone 2026, aarch64)
# Downloads the aarch64 RPM for a PR and installs it.
#
# Usage:
#   sh test-pr.sh <pr-number>     # e.g. sh test-pr.sh 11
#
# Requirements: curl, devel-su (both preinstalled on Sailfish OS)
# No API calls, no jq, no python - just a direct download from raw.githubusercontent.com

set -e

PR="${1:-}"
REPO="storefyr/Skrynja"
BRANCH="pr-${PR}-rpm"
RPM_NAME="skrynja-pr${PR}.rpm"
DOWNLOAD_URL="https://raw.githubusercontent.com/${REPO}/${BRANCH}/${RPM_NAME}"

if [ -z "$PR" ]; then
    echo "Usage: sh test-pr.sh <pr-number>"
    echo "Example: sh test-pr.sh 11"
    exit 1
fi

echo "=== Skrynja PR Test ==="
echo "PR: #${PR}"
echo "URL: ${DOWNLOAD_URL}"
echo ""

# --- Step 1: Download the RPM ---
echo "--- Step 1: Downloading RPM ---"
TMP_DIR="/tmp/skrynja-pr-test"
rm -rf "${TMP_DIR}"
mkdir -p "${TMP_DIR}"
cd "${TMP_DIR}"

if ! curl -sfL -o "${RPM_NAME}" "${DOWNLOAD_URL}"; then
    echo "ERROR: Could not download RPM." >&2
    echo "Has the PR build finished? The RPM appears after CI completes." >&2
    echo "Tried: ${DOWNLOAD_URL}" >&2
    rm -rf "${TMP_DIR}"
    exit 1
fi
echo "Downloaded: ${RPM_NAME} ($(du -h "${RPM_NAME}" | cut -f1))"

# --- Step 2: Install ---
echo "--- Step 2: Installing ---"
devel-su pkcon install-local --allow-reinstall "${TMP_DIR}/${RPM_NAME}"

# --- Step 3: Clean up ---
echo "--- Step 3: Cleaning up ---"
rm -rf "${TMP_DIR}"

echo ""
echo "=== Done! ==="
echo "Skrynja PR #${PR} (aarch64) is installed. Launch it from the app grid."
