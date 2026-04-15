#!/bin/bash
#
# create_submission.sh
#
# Creates a submission tarball for the Tsunami Lab at FSU Jena.
# Team: Brückner, Köllmann, Vogt
#
# Usage:
#   ./create_submission.sh <due_date YYYY-MM-DD> <project_path>
#
# Example:
#   ./create_submission.sh 2025-10-20 ~/tsunami-lab
#
# Packages the given directory into
# submission_YY_MM_DD_brueckner_koellmann_vogt.tar.xz,
# excluding hidden files and directories (.git, .DS_Store, etc.).
# The tarball is created in the current working directory.
#

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <due_date YYYY-MM-DD> <project_path>"
    echo ""
    echo "Example:"
    echo "  $0 2025-10-20 ~/tsunami-lab"
    exit 1
fi

DUE_DATE="$1"
PROJECT_PATH="${2/#\~/$HOME}"

if [[ ! "$DUE_DATE" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]]; then
    echo "Error: due_date must be in the format YYYY-MM-DD (e.g. 2025-10-20)."
    exit 1
fi

if [[ ! -d "$PROJECT_PATH" ]]; then
    echo "Error: '$PROJECT_PATH' is not a directory."
    exit 1
fi

if date --version &>/dev/null 2>&1; then
    DATE_FORMATTED="$(date -d "$DUE_DATE" +%y_%m_%d)"
else
    DATE_FORMATTED="$(date -j -f "%Y-%m-%d" "$DUE_DATE" +%y_%m_%d)"
fi
DIR_NAME="submission_${DATE_FORMATTED}_brueckner_koellmann_vogt"
TARBALL="${DIR_NAME}.tar.xz"

echo "=== Tsunami Lab Submission ==="
echo "  Team    : Brückner, Köllmann, Vogt"
echo "  Date    : ${DUE_DATE}"
echo "  Source  : ${PROJECT_PATH}"
echo "  Tarball : ${TARBALL}"
echo ""

# Create a clean copy without hidden files in a temp directory
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

echo "Copying project files (excluding hidden files)..."
rsync -a --exclude='.*' "${PROJECT_PATH}/" "${TMPDIR}/${DIR_NAME}/"

echo "Creating tarball..."
tar -cvJf "$TARBALL" -C "$TMPDIR" "$DIR_NAME"

echo ""
echo "=== Done! ==="
echo "Upload '${TARBALL}' to Moodle."
echo "Remember: Every team member must upload the file!"