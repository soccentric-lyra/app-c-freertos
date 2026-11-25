#!/usr/bin/env bash
#
# cppcheck.sh - static analysis with cppcheck
#
# Author:  Sandesh Ghimire <sandesh@soccentric.com>
# Usage:   scripts/cppcheck.sh
#
# Hand-written application sources only: generated flatcc headers and the
# vendored runtime are not ours to analyse.  The check is deliberately strict
# enough to catch broken code but does not turn stylistic notes into build
# failures.
#

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT"

SOURCES=()
while IFS= read -r -d '' file; do
    SOURCES+=("$file")
done < <(find src -type f -name '*.c' \
    -not -path 'src/generated/*' -print0 | sort -z)

if [[ ${#SOURCES[@]} -eq 0 ]]; then
    echo "cppcheck: no sources found" >&2
    exit 1
fi

cppcheck \
    --enable=warning,performance,portability \
    --inline-suppr \
    --error-exitcode=1 \
    --std=c11 \
    --quiet \
    -Isrc \
    -DAPP_VERSION_STR='"cppcheck"' \
    -DAPP_FIRMWARE_NAME='"cppcheck"' \
    -DAPP_GIT_COMMIT='"unknown"' \
    -DAPP_BUILD_DATE='"unknown"' \
    -DAPP_BUILD_TIMESTAMP=0 \
    -DAPP_FLATCC_HEAP_SIZE=12288 \
    --suppress=missingInclude \
    "${SOURCES[@]}"

echo "cppcheck: ${#SOURCES[@]} files clean"
