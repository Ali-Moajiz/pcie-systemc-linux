#!/usr/bin/env bash
#
# build-rootfs.sh - Build a minimal Debian/glibc rootfs and package it
# as an initramfs for QEMU.
#
# Usage:
#   ./build-rootfs.sh --arch <x86_64|arm64|arm|riscv64> [options]
#
# Options:
#   --arch <arch>          Target architecture (required)
#   --release <release>    Debian release (default: bookworm)
#   --out-dir <dir>        Output directory (default: <repo-root>/rootfs/<arch>)
#   --packages <list>      Additional comma-separated Debian packages
#   --init <file>          Custom /init script
#   --clean                Remove output directory before building
#   -h, --help             Show this help
#

set -euo pipefail

DEBIAN_RELEASE="${DEBIAN_RELEASE:-bookworm}"
ARCH_ARG=""
OUT_DIR=""
EXTRA_PACKAGES=""
CUSTOM_INIT=""
DO_CLEAN=0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

usage() {
    sed -n '2,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

# ---------------------------------------------------------------------------
# Parse arguments
# ---------------------------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)       ARCH_ARG="$2"; shift 2 ;;
        --release)    DEBIAN_RELEASE="$2"; shift 2 ;;
        --out-dir)    OUT_DIR="$2"; shift 2 ;;
        --packages)   EXTRA_PACKAGES="$2"; shift 2 ;;
        --init)       CUSTOM_INIT="$2"; shift 2 ;;
        --clean)      DO_CLEAN=1; shift ;;
        -h|--help)    usage 0 ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage 1
            ;;
    esac
done

if [[ -z "${ARCH_ARG}" ]]; then
    echo "error: --arch is required" >&2
    usage 1
fi

# ---------------------------------------------------------------------------
# Architecture mapping
#
# Project architecture name -> Debian architecture name
# ---------------------------------------------------------------------------

case "${ARCH_ARG}" in
    x86_64)
        DEBIAN_ARCH="amd64"
        ;;
    arm64|aarch64)
        DEBIAN_ARCH="arm64"
        ;;
    arm|armhf|armv7)
        DEBIAN_ARCH="armhf"
        ;;
    riscv64)
        DEBIAN_ARCH="riscv64"
        ;;
    *)
        echo "error: unsupported architecture '${ARCH_ARG}'" >&2
        echo "supported: x86_64, arm64, arm, riscv64" >&2
        exit 1
        ;;
esac

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

OUT_DIR="${OUT_DIR:-${REPO_ROOT}/rootfs/${ARCH_ARG}}"
ROOTFS_DIR="${OUT_DIR}/rootfs"
INITRAMFS_PATH="${OUT_DIR}/rootfs.cpio.gz"
RECEIPT="${OUT_DIR}/build.env"

# ---------------------------------------------------------------------------
# Check dependencies
# ---------------------------------------------------------------------------

for cmd in debootstrap cpio gzip; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        echo "error: required command '${cmd}' is not installed" >&2
        echo ""
        echo "Install dependencies with:"
        echo "  sudo apt install debootstrap cpio gzip"
        exit 1
    fi
done

# ---------------------------------------------------------------------------
# Native architecture check
#
# debootstrap/chroot package installation is currently intended to run
# natively. Foreign architectures can be added later using qemu-user-static.
# ---------------------------------------------------------------------------

HOST_ARCH="$(uname -m)"

case "${HOST_ARCH}" in
    x86_64)  HOST_DEBIAN_ARCH="amd64" ;;
    aarch64) HOST_DEBIAN_ARCH="arm64" ;;
    armv7l)  HOST_DEBIAN_ARCH="armhf" ;;
    riscv64) HOST_DEBIAN_ARCH="riscv64" ;;
    *)       HOST_DEBIAN_ARCH="${HOST_ARCH}" ;;
esac

if [[ "${HOST_DEBIAN_ARCH}" != "${DEBIAN_ARCH}" ]]; then
    echo "error: target architecture '${DEBIAN_ARCH}' differs from host '${HOST_DEBIAN_ARCH}'" >&2
    echo "       foreign-architecture rootfs creation is not enabled yet." >&2
    echo "       qemu-user-static can be added later for cross-architecture rootfs builds." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Clean/create output
# ---------------------------------------------------------------------------

if [[ "${DO_CLEAN}" -eq 1 && -d "${OUT_DIR}" ]]; then
    echo ">> removing ${OUT_DIR}"
    sudo rm -rf "${OUT_DIR}"
fi

mkdir -p "${OUT_DIR}"

# ---------------------------------------------------------------------------
# Build Debian rootfs
# ---------------------------------------------------------------------------

if [[ ! -d "${ROOTFS_DIR}/bin" ]]; then

    echo ">> creating Debian ${DEBIAN_RELEASE} rootfs"
    echo "   architecture: ${DEBIAN_ARCH}"
    echo "   output:       ${ROOTFS_DIR}"

    sudo debootstrap \
        --arch="${DEBIAN_ARCH}" \
        --variant=minbase \
        "${DEBIAN_RELEASE}" \
        "${ROOTFS_DIR}" \
        https://deb.debian.org/debian
else
    echo ">> existing rootfs found: ${ROOTFS_DIR}"
fi

# ---------------------------------------------------------------------------
# Configure package repositories
# ---------------------------------------------------------------------------

sudo tee "${ROOTFS_DIR}/etc/apt/sources.list" >/dev/null <<EOF
deb https://deb.debian.org/debian ${DEBIAN_RELEASE} main
deb https://deb.debian.org/debian ${DEBIAN_RELEASE}-updates main
deb https://security.debian.org/debian-security ${DEBIAN_RELEASE}-security main
EOF

# DNS for chroot package installation
sudo cp /etc/resolv.conf "${ROOTFS_DIR}/etc/resolv.conf"

# ---------------------------------------------------------------------------
# Mount filesystems required by apt/chroot
# ---------------------------------------------------------------------------

cleanup_mounts() {
    sudo umount "${ROOTFS_DIR}/dev/pts" 2>/dev/null || true
    sudo umount "${ROOTFS_DIR}/dev"     2>/dev/null || true
    sudo umount "${ROOTFS_DIR}/proc"    2>/dev/null || true
    sudo umount "${ROOTFS_DIR}/sys"     2>/dev/null || true
}

trap cleanup_mounts EXIT

sudo mount --bind /dev "${ROOTFS_DIR}/dev"
sudo mount --bind /dev/pts "${ROOTFS_DIR}/dev/pts"
sudo mount -t proc proc "${ROOTFS_DIR}/proc"
sudo mount -t sysfs sysfs "${ROOTFS_DIR}/sys"

# ---------------------------------------------------------------------------
# Install useful development / hardware-validation utilities
# ---------------------------------------------------------------------------

DEFAULT_PACKAGES=(
    kmod
    pciutils
    procps
    iproute2
    iputils-ping
    net-tools
    ethtool
    util-linux
    coreutils
    bash
    file
    strace
)

echo ">> updating Debian package database"

sudo chroot "${ROOTFS_DIR}" \
    /usr/bin/env DEBIAN_FRONTEND=noninteractive \
    apt-get update

echo ">> installing default validation/debugging tools"

sudo chroot "${ROOTFS_DIR}" \
    /usr/bin/env DEBIAN_FRONTEND=noninteractive \
    apt-get install -y --no-install-recommends \
    "${DEFAULT_PACKAGES[@]}"

# ---------------------------------------------------------------------------
# Optional user-requested packages
# ---------------------------------------------------------------------------

if [[ -n "${EXTRA_PACKAGES}" ]]; then

    IFS=',' read -ra PKG_ARRAY <<< "${EXTRA_PACKAGES}"

    echo ">> installing additional packages: ${PKG_ARRAY[*]}"

    sudo chroot "${ROOTFS_DIR}" \
        /usr/bin/env DEBIAN_FRONTEND=noninteractive \
        apt-get install -y --no-install-recommends \
        "${PKG_ARRAY[@]}"
fi

# Keep image reasonably small
sudo chroot "${ROOTFS_DIR}" apt-get clean
sudo rm -rf "${ROOTFS_DIR}/var/lib/apt/lists/"*

# Unmount before packaging
cleanup_mounts
trap - EXIT

# ---------------------------------------------------------------------------
# Install /init
# ---------------------------------------------------------------------------

if [[ -n "${CUSTOM_INIT}" ]]; then

    echo ">> installing custom init: ${CUSTOM_INIT}"

    sudo cp "${CUSTOM_INIT}" "${ROOTFS_DIR}/init"

else

    echo ">> installing default init"

    sudo tee "${ROOTFS_DIR}/init" >/dev/null <<'EOF'
#!/bin/bash

mount -t proc proc /proc
mount -t sysfs sysfs /sys

mount -t devtmpfs devtmpfs /dev 2>/dev/null || true

mkdir -p /dev/pts
mount -t devpts devpts /dev/pts 2>/dev/null || true

mkdir -p /run
mkdir -p /tmp
mkdir -p /mnt/hostshare

echo ""
echo "========================================"
echo " PCIe SystemC Linux Test Environment"
echo "========================================"
echo ""
echo "Debian userspace is ready."
echo ""
echo "To mount the host shared directory:"
echo ""
echo "  mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/hostshare"
echo ""
echo "Then:"
echo ""
echo "  cd /mnt/hostshare"
echo "  insmod pcie_driver.ko"
echo "  ./app"
echo ""

exec /bin/bash
EOF

fi

sudo chmod +x "${ROOTFS_DIR}/init"

# ---------------------------------------------------------------------------
# Prepare directories useful to the project
# ---------------------------------------------------------------------------

sudo mkdir -p "${ROOTFS_DIR}/lib/modules/extra"
sudo mkdir -p "${ROOTFS_DIR}/mnt/hostshare"

# ---------------------------------------------------------------------------
# Package rootfs as initramfs
# ---------------------------------------------------------------------------

echo ">> packaging initramfs"
echo "   ${INITRAMFS_PATH}"

(
    cd "${ROOTFS_DIR}"

    sudo find . -print0 |
        sudo cpio \
            --null \
            --create \
            --format=newc \
            --owner=root:root 2>/dev/null |
        gzip -9 > "${INITRAMFS_PATH}"
)

# ---------------------------------------------------------------------------
# Build receipt
# ---------------------------------------------------------------------------

cat > "${RECEIPT}" <<EOF
# Auto-generated by build-rootfs.sh - do not edit by hand.

ARCH=${ARCH_ARG}
DEBIAN_ARCH=${DEBIAN_ARCH}
DEBIAN_RELEASE=${DEBIAN_RELEASE}
ROOTFS_DIR=${ROOTFS_DIR}
INITRAMFS=${INITRAMFS_PATH}
EOF

echo ""
echo ">> done."
echo ""
echo "   Debian release: ${DEBIAN_RELEASE}"
echo "   architecture:   ${DEBIAN_ARCH}"
echo "   rootfs:         ${ROOTFS_DIR}"
echo "   initramfs:      ${INITRAMFS_PATH}"
echo "   receipt:        ${RECEIPT}"
echo ""