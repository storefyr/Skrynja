#!/bin/sh
# Skrynja PR test script - run on the Sailfish OS device (Jolla Phone 2026, aarch64)
# Downloads the aarch64 RPM for a PR from the pr-<N>-rpm branch and installs it.
#
# Usage:
#   sh test-pr.sh <pr-number>     # e.g. sh test-pr.sh 11
#
# Requirements: curl, devel-su (both preinstalled on Sailfish OS)
# No jq, no python, no gh - everything uses curl only.

set -e

PR="${1:-}"
REPO="storefyr/Skrynja"
ARCH="aarch64"
BRANCH="pr-${PR}-rpm"

if [ -z "$PR" ]; then
    echo "Usage: sh test-pr.sh <pr-number>"
    echo "Example: sh test-pr.sh 11"
    exit 1
fi

echo "=== Skrynja PR Test ==="
echo "PR: #${PR}"
echo "Branch: ${BRANCH}"
echo ""

# --- Step 1: Find the RPM filename on the branch ---
echo "--- Step 1: Looking up RPM on branch ${BRANCH} ---"
# List the branch contents via the GitHub API (no auth needed for public repos)
CONTENTS_JSON=$(curl -sf "https://api.github.com/repos/${REPO}/contents/?ref=${BRANCH}")
RPM_NAME=$(echo "$CONTENTS_JSON" \
    | tr ',' '\n' \
    | grep '"name":"harbour-skrynja-' \
    | grep '\.rpm"' \
    | head -1 \
    | cut -d'"' -f4)

if [ -z "$RPM_NAME" ]; then
    echo "ERROR: No RPM found on branch '${BRANCH}'." >&2
    echo "Has the PR build finished? The RPM appears after CI completes." >&2
    exit 1
fi
echo "Found: ${RPM_NAME}"

# --- Step 2: Download the RPM ---
echo "--- Step 2: Downloading RPM ---"
TMP_DIR="/tmp/skrynja-pr-test"
rm -rf "${TMP_DIR}"
mkdir -p "${TMP_DIR}"
cd "${TMP_DIR}"

DOWNLOAD_URL="https://raw.githubusercontent.com/${REPO}/${BRANCH}/${RPM_NAME}"
curl -sfL -o "${RPM_NAME}" "${DOWNLOAD_URL}"
echo "Downloaded: ${RPM_NAME} ($(du -h "${RPM_NAME}" | cut -f1))"

# --- Step 3: Install ---
echo "--- Step 3: Installing ---"
devel-su pkcon install-local --allow-reinstall "${TMP_DIR}/${RPM_NAME}"

# --- Step 4: Clean up ---
echo "--- Step 4: Cleaning up ---"
rm -rf "${TMP_DIR}"

echo ""
echo "=== Done! ==="
echo "Skrynja PR #${PR} (${ARCH}) is installed. Launch it from the app grid."
