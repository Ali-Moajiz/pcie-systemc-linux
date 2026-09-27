#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

ARCH_ARG="${1:-x86_64}"
RECEIPT="${REPO_ROOT}/kernel/${ARCH_ARG}/build.env"

if [[ ! -f "${RECEIPT}" ]]; then
    echo "error: kernel build receipt not found: ${RECEIPT}" >&2
    echo "       build the kernel first." >&2
    exit 1
fi

echo ">> sourcing kernel build receipt: ${RECEIPT}"
source "${RECEIPT}"

CC="${CROSS_COMPILE}gcc"

if ! command -v "${CC}" >/dev/null 2>&1; then
    echo "error: compiler '${CC}' not found in PATH" >&2
    exit 1
fi

echo ">> building userspace application"
echo "   ARCH=${ARCH}"
echo "   CC=${CC}"

make -C "${SCRIPT_DIR}" clean
make -C "${SCRIPT_DIR}" CC="${CC}"