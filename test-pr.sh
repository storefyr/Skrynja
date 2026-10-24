#!/bin/sh
# Skrynja PR test script - run on the Sailfish OS device (Jolla Phone 2026, aarch64)
# Downloads the latest aarch64 RPM artifact from a PR's CI build and installs it.
#
# Usage:
#   sh test-pr.sh <pr-number>              # e.g. sh test-pr.sh 11
#   sh test-pr.sh <pr-number> <run-id>     # pin a specific CI run
#
# Requirements: curl, jq, devel-su
# The GitHub CLI (gh) is NOT needed on the device - the artifact download URL is
# resolved via the public GitHub API using curl.

set -e

PR="${1:-}"
RUN_ID="${2:-}"
REPO="storefyr/Skrynja"
ARCH="aarch64"
ARTIFACT_PREFIX="rpm-${ARCH}-pr"

if [ -z "$PR" ]; then
    echo "Usage: sh test-pr.sh <pr-number> [run-id]"
    echo "Example: sh test-pr.sh 11"
    exit 1
fi

echo "=== Skrynja PR Test ==="
echo "PR: #${PR}"
echo "Repo: ${REPO}"
echo "Arch: ${ARCH}"
echo ""

# --- Step 1: Find the latest successful PR CI run ---
if [ -z "$RUN_ID" ]; then
    echo "--- Step 1: Finding latest CI run for PR #${PR} ---"
    RUN_ID=$(curl -sf "https://api.github.com/repos/${REPO}/actions/runs?event=pull_request&per_page=30" \
        | jq -r --arg pr "${PR}" \
            '[.workflow_runs[] | select(.head_branch != null and .pull_requests[0].number == ($pr|tonumber) and .conclusion == "success")] | first | .id // empty')
    if [ -z "$RUN_ID" ] || [ "$RUN_ID" = "null" ]; then
        echo "ERROR: No successful CI run found for PR #${PR}." >&2
        echo "Has the PR build finished? Check the Actions tab." >&2
        exit 1
    fi
    echo "Found run: ${RUN_ID}"
else
    echo "--- Step 1: Using pinned run ${RUN_ID} ---"
fi

# --- Step 2: Find the RPM artifact in the run ---
echo "--- Step 2: Finding RPM artifact ---"
ARTIFACT_INFO=$(curl -sf "https://api.github.com/repos/${REPO}/actions/runs/${RUN_ID}/artifacts?per_page=100" \
    | jq -r --arg prefix "${ARTIFACT_PREFIX}${PR}" \
        '[.artifacts[] | select(.name | startswith($prefix))] | first | "\(.id) \(.name)"')
ARTIFACT_ID=$(echo "$ARTIFACT_INFO" | awk '{print $1}')
ARTIFACT_NAME=$(echo "$ARTIFACT_INFO" | awk '{print $2}')

if [ -z "$ARTIFACT_ID" ] || [ "$ARTIFACT_ID" = " " ]; then
    echo "ERROR: No artifact matching '${ARTIFACT_PREFIX}${PR}*' found in run ${RUN_ID}." >&2
    exit 1
fi
echo "Found artifact: ${ARTIFACT_NAME} (id: ${ARTIFACT_ID})"

# --- Step 3: Download the artifact ---
echo "--- Step 3: Downloading RPM ---"
TMP_DIR="/tmp/skrynja-pr-test"
rm -rf "${TMP_DIR}"
mkdir -p "${TMP_DIR}"
cd "${TMP_DIR}"

curl -sfL -o artifact.zip "https://api.github.com/repos/${REPO}/actions/artifacts/${ARTIFACT_ID}/zip"
echo "Downloaded artifact.zip"

# --- Step 4: Extract the RPM ---
echo "--- Step 4: Extracting RPM ---"
if command -v unzip >/dev/null 2>&1; then
    unzip -o artifact.zip
elif command -v busybox >/dev/null 2>&1; then
    busybox unzip -o artifact.zip
else
    echo "ERROR: No unzip available. Install it or extract manually." >&2
    exit 1
fi

RPM_FILE=$(ls harbour-skrynja-*.rpm 2>/dev/null | head -1)
if [ -z "$RPM_FILE" ]; then
    echo "ERROR: No RPM found inside the artifact." >&2
    ls -la
    exit 1
fi
echo "RPM: ${RPM_FILE} ($(du -h "${RPM_FILE}" | cut -f1))"

# --- Step 5: Install ---
echo "--- Step 5: Installing ---"
devel-su pkcon install-local --allow-reinstall "${TMP_DIR}/${RPM_FILE}"

# --- Step 6: Clean up ---
echo "--- Step 6: Cleaning up ---"
rm -rf "${TMP_DIR}"

echo ""
echo "=== Done! ==="
echo "Skrynja PR #${PR} (${ARCH}) is installed. Launch it from the app grid."
