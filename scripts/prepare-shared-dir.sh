#!/usr/bin/env bash

set -e

# Project root: pcie-systemc-linux/
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Paths
SHARED_DIR="${REPO_ROOT}/shared_dir"

DRIVER="${REPO_ROOT}/device-driver/pcie_driver.ko"
APP="${REPO_ROOT}/device-driver/user-space-application/app"

# Create shared_dir if it doesn't exist
mkdir -p "${SHARED_DIR}"

# Verify kernel module exists
if [[ ! -f "${DRIVER}" ]]; then
    echo "error: kernel module not found: ${DRIVER}"
    echo "       build the kernel module first."
    exit 1
fi

# Verify userspace application exists
if [[ ! -f "${APP}" ]]; then
    echo "error: application not found: ${APP}"
    echo "       build the userspace application first."
    exit 1
fi

# Copy artifacts
cp "${DRIVER}" "${SHARED_DIR}/"
cp "${APP}" "${SHARED_DIR}/"

echo ">> Shared directory prepared successfully:"
echo "   ${SHARED_DIR}"
echo ""
echo ">> Contents:"
ls -lh "${SHARED_DIR}"