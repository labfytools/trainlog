#!/bin/sh
# Check only C and header files changed from a caller-selected Git base.

set -eu

base_ref=${1:-main}
base_commit=$(git merge-base HEAD "$base_ref")
temporary_list=$(mktemp)
trap 'rm -f "$temporary_list"' EXIT HUP INT TERM

{
    git diff --name-only --diff-filter=ACMR "$base_commit" -- '*.c' '*.h'
    git ls-files --others --exclude-standard -- '*.c' '*.h'
} | sort -u >"$temporary_list"

if [ ! -s "$temporary_list" ]; then
    echo "CHANGED_C_FORMAT=PASS (no changed C/H files)"
    exit 0
fi

while IFS= read -r source_file; do
    clang-format --dry-run --Werror "$source_file"
done <"$temporary_list"

echo "CHANGED_C_FORMAT=PASS"
