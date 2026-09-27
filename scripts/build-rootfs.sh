#!/usr/bin/env bash
#
# build-rootfs.sh - Fetch Alpine Linux's official minirootfs for a given
# architecture, verify it, and package it as a bootable initramfs for QEMU.
#
# Alpine's minirootfs already gives a small musl+busybox userland with
# apk-tools for adding packages, per architecture, without needing any
# cross-compilation on our side - so this script only fetches and packages,
# it does not build anything from source.
#
# Usage:
#   ./build-rootfs.sh --arch <x86_64|aarch64|armv7|riscv64> [options]
#
# Options:
#   --arch <arch>          Target architecture, in Alpine's naming (required)
#   --alpine-version <ver> Alpine release, e.g. 3.20.3 (default: see ALPINE_VERSION below)
#   --out-dir <dir>        Output directory (default: <repo-root>/rootfs/<arch>)
#   --packages <list>      Comma-separated apk packages to add (native arch only, see note)
#   --init <file>          Custom /init script to install (default: a minimal one, see below)
#   --clean                Remove the output directory first
#   -h, --help              Show this help
#
# Note: --packages runs "apk add" inside the extracted rootfs via chroot,
# which only works when --arch matches the host's architecture. For a
# foreign-arch rootfs, install packages another way (e.g. qemu-user-static
# + binfmt_misc) - this script does not attempt that automatically.
#
set -euo pipefail

ALPINE_VERSION="${ALPINE_VERSION:-3.20.3}"
ARCH_ARG=""
OUT_DIR=""
PACKAGES=""
CUSTOM_INIT=""
DO_CLEAN=0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

usage() {
    sed -n '2,21p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)            ARCH_ARG="$2"; shift 2 ;;
        --alpine-version)  ALPINE_VERSION="$2"; shift 2 ;;
        --out-dir)         OUT_DIR="$2"; shift 2 ;;
        --packages)        PACKAGES="$2"; shift 2 ;;
        --init)            CUSTOM_INIT="$2"; shift 2 ;;
        --clean)           DO_CLEAN=1; shift ;;
        -h|--help)         usage 0 ;;
        *) echo "Unknown argument: $1" >&2; usage 1 ;;
    esac
done

if [[ -z "${ARCH_ARG}" ]]; then
    echo "error: --arch is required" >&2
    usage 1
fi

case "${ARCH_ARG}" in
    x86_64|aarch64|armv7|armhf|riscv64|ppc64le|s390x|x86) ;;
    *)
        echo "error: unsupported --arch '${ARCH_ARG}'" >&2
        echo "supported (Alpine release architectures): x86_64, aarch64, armv7, armhf, riscv64, ppc64le, s390x, x86" >&2
        exit 1
        ;;
esac

OUT_DIR="${OUT_DIR:-${REPO_ROOT}/rootfs/${ARCH_ARG}}"
DL_DIR="${REPO_ROOT}/downloads"
EXTRACT_DIR="${OUT_DIR}/rootfs"
INITRAMFS_PATH="${OUT_DIR}/rootfs.cpio.gz"

if [[ "${DO_CLEAN}" -eq 1 && -d "${OUT_DIR}" ]]; then
    echo ">> removing ${OUT_DIR}"
    rm -rf "${OUT_DIR}"
fi

mkdir -p "${EXTRACT_DIR}" "${DL_DIR}"

# ---------------------------------------------------------------------------
# Fetch + verify. Alpine publishes a matching .sha256 file next to every
# release tarball; fetched fresh each run so bumping ALPINE_VERSION never
# requires touching this script. (Double-check this URL layout on first run
# against dl-cdn.alpinelinux.org - Alpine has changed release paths before.)
# ---------------------------------------------------------------------------
BASE_URL="https://dl-cdn.alpinelinux.org/alpine/v$(echo "${ALPINE_VERSION}" | cut -d. -f1,2)/releases/${ARCH_ARG}"
TARBALL="alpine-minirootfs-${ALPINE_VERSION}-${ARCH_ARG}.tar.gz"
TARBALL_PATH="${DL_DIR}/${TARBALL}"

if [[ ! -f "${TARBALL_PATH}" ]]; then
    echo ">> downloading ${TARBALL}"
    curl -fL --retry 3 -o "${TARBALL_PATH}.part" "${BASE_URL}/${TARBALL}"
    mv "${TARBALL_PATH}.part" "${TARBALL_PATH}"
fi

echo ">> fetching checksum"
EXPECTED_SUM="$(curl -fsL --retry 3 "${BASE_URL}/${TARBALL}.sha256" | awk '{print $1}')"
if [[ -z "${EXPECTED_SUM}" ]]; then
    echo "error: could not fetch checksum for ${TARBALL}" >&2
    echo "       check --arch/--alpine-version are a valid Alpine release combination" >&2
    exit 1
fi

ACTUAL_SUM="$(sha256sum "${TARBALL_PATH}" | awk '{print $1}')"
if [[ "${EXPECTED_SUM}" != "${ACTUAL_SUM}" ]]; then
    echo "error: checksum mismatch for ${TARBALL}" >&2
    echo "       expected: ${EXPECTED_SUM}" >&2
    echo "       actual:   ${ACTUAL_SUM}" >&2
    rm -f "${TARBALL_PATH}"
    exit 1
fi
echo ">> checksum OK"

# ---------------------------------------------------------------------------
# Extract
# ---------------------------------------------------------------------------
echo ">> extracting to ${EXTRACT_DIR}"
rm -rf "${EXTRACT_DIR}"
mkdir -p "${EXTRACT_DIR}"
tar -xzf "${TARBALL_PATH}" -C "${EXTRACT_DIR}"

# ---------------------------------------------------------------------------
# Optional package install (native arch only - see usage note above)
# ---------------------------------------------------------------------------
if [[ -n "${PACKAGES}" ]]; then
    HOST_ARCH="$(uname -m)"
    if [[ "${HOST_ARCH}" != "${ARCH_ARG}" ]]; then
        echo "error: --packages requires --arch to match the host arch (${HOST_ARCH})" >&2
        echo "       for a foreign-arch rootfs, install packages via qemu-user-static + binfmt_misc instead" >&2
        exit 1
    fi
    echo ">> installing packages: ${PACKAGES}"
    IFS=',' read -ra PKG_ARRAY <<< "${PACKAGES}"
    cp /etc/resolv.conf "${EXTRACT_DIR}/etc/resolv.conf" 2>/dev/null || true
    mount --bind /dev "${EXTRACT_DIR}/dev"
    mount -t proc proc "${EXTRACT_DIR}/proc"
    trap 'umount "${EXTRACT_DIR}/dev" "${EXTRACT_DIR}/proc" 2>/dev/null || true' EXIT
    chroot "${EXTRACT_DIR}" /sbin/apk update
    chroot "${EXTRACT_DIR}" /sbin/apk add "${PKG_ARRAY[@]}"
    umount "${EXTRACT_DIR}/dev" "${EXTRACT_DIR}/proc"
    trap - EXIT
fi

# ---------------------------------------------------------------------------
# Install /init. Alpine's minirootfs has no init system configured out of
# the box (just busybox + apk-tools), so we always need one for QEMU to
# hand off to. A minimal default is provided; pass --init to supply your own
# (e.g. one that insmods your driver and launches a test app automatically).
# ---------------------------------------------------------------------------
if [[ -n "${CUSTOM_INIT}" ]]; then
    echo ">> installing custom init from ${CUSTOM_INIT}"
    cp "${CUSTOM_INIT}" "${EXTRACT_DIR}/init"
else
    echo ">> installing default init"
    cat > "${EXTRACT_DIR}/init" <<'EOF'
#!/bin/sh
# Minimal init for the pcie-systemc-linux rootfs.
# Replace with --init at build-rootfs.sh time for automated driver
# loading / test-app execution.

mount -t proc     proc     /proc
mount -t sysfs    sysfs    /sys
mount -t devtmpfs devtmpfs /dev 2>/dev/null || mdev -s

echo ""
echo "pcie-systemc-linux rootfs ready."
echo "Load your driver with: insmod /lib/modules/extra/<your_driver>.ko"
echo ""

exec /bin/sh
EOF
fi
chmod +x "${EXTRACT_DIR}/init"

# Directory for the caller to drop a built .ko into before packaging.
mkdir -p "${EXTRACT_DIR}/lib/modules/extra"

# ---------------------------------------------------------------------------
# Package as an initramfs for QEMU's -initrd
# ---------------------------------------------------------------------------
echo ">> packaging initramfs -> ${INITRAMFS_PATH}"
( cd "${EXTRACT_DIR}" && find . -print0 | cpio --null -ov --format=newc 2>/dev/null | gzip -9 > "${INITRAMFS_PATH}" )

# ---------------------------------------------------------------------------
# Write a receipt, matching build-kernel.sh's pattern, for downstream
# run-scripts to consume.
# ---------------------------------------------------------------------------
RECEIPT="${OUT_DIR}/build.env"
cat > "${RECEIPT}" <<EOF
# Auto-generated by build-rootfs.sh - do not edit by hand.
ARCH=${ARCH_ARG}
ALPINE_VERSION=${ALPINE_VERSION}
ROOTFS_DIR=${EXTRACT_DIR}
INITRAMFS=${INITRAMFS_PATH}
EOF

echo ""
echo ">> done."
echo "   rootfs dir:  ${EXTRACT_DIR}"
echo "   initramfs:   ${INITRAMFS_PATH}"
echo "   receipt:     ${RECEIPT}"
