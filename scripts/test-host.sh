#!/usr/bin/env bash
#
# test-host.sh - build and run the portable host test suites
#
# Author:  Sandesh Ghimire <sandesh@soccentric.com>
# Usage:   scripts/test-host.sh [--coverage]
#
# The tests cover crc32 and the HWLINK framing layer.  They need only a host C
# compiler, so the same command runs in GitHub Actions and on a developer
# machine without Docker or an ARM toolchain.
#

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT"

COVERAGE=0
if [[ "${1:-}" == "--coverage" ]]; then
    COVERAGE=1
elif [[ $# -gt 0 ]]; then
    echo "usage: scripts/test-host.sh [--coverage]" >&2
    exit 2
fi

CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c11 -Wall -Wextra -Werror -pedantic -O2 -g}"
OUT="build/host-tests"
mkdir -p "$OUT"

SOURCES=(
    tests/host/test_main.c
    tests/host/test_crc32.c
    tests/host/test_hwlink.c
    src/services/crc32.c
    src/protocol/hwlink.c
)

if [[ "$COVERAGE" == "0" ]]; then
    # shellcheck disable=SC2086
    "$CC" $CFLAGS -Isrc "${SOURCES[@]}" -o "$OUT/host-tests"
    "$OUT/host-tests"
    exit 0
fi

COV="$OUT/coverage"
mkdir -p "$COV"
objects=()
for src in "${SOURCES[@]}"; do
    obj="$COV/$(basename "${src%.c}").o"
    # shellcheck disable=SC2086
    "$CC" $CFLAGS --coverage -Isrc -c "$src" -o "$obj"
    objects+=("$obj")
done
# shellcheck disable=SC2086
"$CC" $CFLAGS --coverage "${objects[@]}" -o "$COV/host-tests"
"$COV/host-tests"
gcov -b -c -o "$COV" \
    src/services/crc32.c \
    src/protocol/hwlink.c >/dev/null
mv ./*.gcov "$COV"/
for report in crc32.c.gcov hwlink.c.gcov; do
    if [[ -f "$COV/$report" ]]; then
        printf '\n== %s ==\n' "$report"
        awk -F: '/^ *[0-9#-]+:/ { lines++; if ($1 !~ /#####/) hit++ } END { printf "lines: %d, hit: %d\n", lines, hit }' "$COV/$report"
    fi
done
