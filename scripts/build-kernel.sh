#!/usr/bin/env bash
#
# build-kernel.sh - Fetch, configure and build a pinned Linux kernel for
# the pcie-systemc-linux co-simulation framework.
#
# The kernel is fetched as a versioned, checksum-verified tarball from
# kernel.org (not a git clone) to keep this fast and reproducible.
#
# On success this writes a "build.env" receipt into the output directory
# that build-module.sh sources automatically, so out-of-tree driver builds
# always match the kernel they're built against without re-specifying
# --arch by hand.
#
# Usage:
#   ./build-kernel.sh --arch <x86_64|arm64|arm|riscv64> [options]
#
# Options:
#   --arch <arch>          Target architecture (required)
#   --version <ver>        Kernel version, e.g. 6.6.30 (default: see KERNEL_VERSION below)
#   --out-dir <dir>        Output directory (default: <repo-root>/kernel/<arch>)
#   --config-fragment <f>  Extra Kconfig fragment to merge on top of defconfig
#   --jobs <n>              Parallel build jobs (default: nproc)
#   --menuconfig            Open menuconfig after applying defconfig/fragment
#   --clean                 Remove the output directory first
#   -h, --help              Show this help
#
set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
KERNEL_VERSION="${KERNEL_VERSION:-6.6.30}"
JOBS="$(nproc 2>/dev/null || echo 4)"
ARCH_ARG=""
OUT_DIR=""
CONFIG_FRAGMENT=""
DO_MENUCONFIG=0
DO_CLEAN=0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

usage() {
    sed -n '2,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)            ARCH_ARG="$2"; shift 2 ;;
        --version)         KERNEL_VERSION="$2"; shift 2 ;;
        --out-dir)         OUT_DIR="$2"; shift 2 ;;
        --config-fragment) CONFIG_FRAGMENT="$2"; shift 2 ;;
        --jobs)            JOBS="$2"; shift 2 ;;
        --menuconfig)      DO_MENUCONFIG=1; shift ;;
        --clean)           DO_CLEAN=1; shift ;;
        -h|--help)         usage 0 ;;
        *) echo "Unknown argument: $1" >&2; usage 1 ;;
    esac
done

if [[ -z "${ARCH_ARG}" ]]; then
    echo "error: --arch is required" >&2
    usage 1
fi

# ---------------------------------------------------------------------------
# Architecture -> kernel ARCH / cross-compiler / image path mapping.
# Extend this case statement (and nothing else) to add a new architecture.
# ---------------------------------------------------------------------------
case "${ARCH_ARG}" in
    x86_64)
        KARCH="x86_64"
        CROSS_COMPILE=""
        IMAGE_RELPATH="arch/x86/boot/bzImage"
        ;;
    arm64|aarch64)
        KARCH="arm64"
        CROSS_COMPILE="aarch64-linux-gnu-"
        IMAGE_RELPATH="arch/arm64/boot/Image"
        ;;
    arm|armhf|armv7)
        KARCH="arm"
        CROSS_COMPILE="arm-linux-gnueabihf-"
        IMAGE_RELPATH="arch/arm/boot/zImage"
        ;;
    riscv64)
        KARCH="riscv"
        CROSS_COMPILE="riscv64-linux-gnu-"
        IMAGE_RELPATH="arch/riscv/boot/Image"
        ;;
    *)
        echo "error: unsupported --arch '${ARCH_ARG}'" >&2
        echo "supported: x86_64, arm64, arm, riscv64" >&2
        exit 1
        ;;
esac

# Fail fast if the cross-compiler isn't installed, rather than 20 minutes
# into a build.
if [[ -n "${CROSS_COMPILE}" ]] && ! command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1; then
    echo "error: cross-compiler '${CROSS_COMPILE}gcc' not found in PATH" >&2
    case "${ARCH_ARG}" in
        arm64|aarch64)   echo "  try: apt install gcc-aarch64-linux-gnu" >&2 ;;
        arm|armhf|armv7) echo "  try: apt install gcc-arm-linux-gnueabihf" >&2 ;;
        riscv64)         echo "  try: apt install gcc-riscv64-linux-gnu" >&2 ;;
    esac
    exit 1
fi

OUT_DIR="${OUT_DIR:-${REPO_ROOT}/kernel/${ARCH_ARG}}"
SRC_DIR="${OUT_DIR}/linux-${KERNEL_VERSION}"
DL_DIR="${REPO_ROOT}/downloads"
KDIR="${SRC_DIR}"

if [[ "${DO_CLEAN}" -eq 1 && -d "${OUT_DIR}" ]]; then
    echo ">> removing ${OUT_DIR}"
    rm -rf "${OUT_DIR}"
fi

mkdir -p "${OUT_DIR}" "${DL_DIR}"

# ---------------------------------------------------------------------------
# Fetch + verify. kernel.org publishes sha256sums.asc alongside every
# tarball; we fetch it fresh each run instead of hardcoding a checksum, so
# bumping KERNEL_VERSION never requires touching this script.
# ---------------------------------------------------------------------------
KMAJOR="$(echo "${KERNEL_VERSION}" | cut -d. -f1)"
KBASE_URL="https://cdn.kernel.org/pub/linux/kernel/v${KMAJOR}.x"
TARBALL="linux-${KERNEL_VERSION}.tar.xz"
TARBALL_PATH="${DL_DIR}/${TARBALL}"
SUMS_PATH="${DL_DIR}/sha256sums-v${KMAJOR}.x.asc"

if [[ ! -f "${TARBALL_PATH}" ]]; then
    echo ">> downloading ${TARBALL}"
    curl -fL --retry 3 -o "${TARBALL_PATH}.part" "${KBASE_URL}/${TARBALL}"
    mv "${TARBALL_PATH}.part" "${TARBALL_PATH}"
fi

echo ">> fetching checksum list"
curl -fL --retry 3 -o "${SUMS_PATH}" "${KBASE_URL}/sha256sums.asc"

EXPECTED_SUM="$(grep " ${TARBALL}\$" "${SUMS_PATH}" | awk '{print $1}' || true)"
if [[ -z "${EXPECTED_SUM}" ]]; then
    echo "error: could not find a checksum for ${TARBALL} in sha256sums.asc" >&2
    echo "       kernel.org may not host this version anymore; check --version" >&2
    exit 1
fi

ACTUAL_SUM="$(sha256sum "${TARBALL_PATH}" | awk '{print $1}')"
if [[ "${EXPECTED_SUM}" != "${ACTUAL_SUM}" ]]; then
    echo "error: checksum mismatch for ${TARBALL}" >&2
    echo "       expected: ${EXPECTED_SUM}" >&2
    echo "       actual:   ${ACTUAL_SUM}" >&2
    rm -f "${TARBALL_PATH}"
    echo "       corrupt download removed - re-run to retry" >&2
    exit 1
fi
echo ">> checksum OK"

# ---------------------------------------------------------------------------
# Extract (skip if already extracted)
# ---------------------------------------------------------------------------
if [[ ! -d "${SRC_DIR}" ]]; then
    echo ">> extracting to ${SRC_DIR}"
    mkdir -p "${SRC_DIR}"
    tar -xJf "${TARBALL_PATH}" -C "${SRC_DIR}" --strip-components=1
fi

# ---------------------------------------------------------------------------
# Optional local patches: patches/<arch>/*.patch, applied in sorted order.
# Empty by default - this is the seam for future kernel-side tweaks.
# ---------------------------------------------------------------------------
PATCH_DIR="${REPO_ROOT}/patches/${ARCH_ARG}"
if [[ -d "${PATCH_DIR}" ]]; then
    shopt -s nullglob
    for p in "${PATCH_DIR}"/*.patch; do
        echo ">> applying patch $(basename "${p}")"
        patch -d "${SRC_DIR}" -p1 --forward -N < "${p}" || {
            echo "error: failed to apply $(basename "${p}")" >&2
            exit 1
        }
    done
    shopt -u nullglob
fi

# ---------------------------------------------------------------------------
# Configure
# ---------------------------------------------------------------------------
export ARCH="${KARCH}"
export CROSS_COMPILE

MAKE=(make -C "${SRC_DIR}" -j"${JOBS}")

echo ">> generating defconfig for ARCH=${KARCH}"
"${MAKE[@]}" defconfig

if [[ -n "${CONFIG_FRAGMENT}" ]]; then
    echo ">> merging config fragment ${CONFIG_FRAGMENT}"
    "${SRC_DIR}/scripts/kconfig/merge_config.sh" \
        -O "${SRC_DIR}" "${SRC_DIR}/.config" "${CONFIG_FRAGMENT}"
fi

# Repo-provided default fragment (e.g. config-fragments/x86_64.config for
# CONFIG_PCI, virtio drivers, etc.), applied after any user-supplied one.
DEFAULT_FRAGMENT="${REPO_ROOT}/config-fragments/${ARCH_ARG}.config"
if [[ -f "${DEFAULT_FRAGMENT}" ]]; then
    echo ">> merging default fragment ${DEFAULT_FRAGMENT}"
    "${SRC_DIR}/scripts/kconfig/merge_config.sh" \
        -O "${SRC_DIR}" "${SRC_DIR}/.config" "${DEFAULT_FRAGMENT}"
fi

"${MAKE[@]}" olddefconfig

if [[ "${DO_MENUCONFIG}" -eq 1 ]]; then
    "${MAKE[@]}" menuconfig
fi

# ---------------------------------------------------------------------------
# Build. The default "all" target already produces the right boot image for
# each of these architectures (bzImage/Image/zImage), so no special image
# target needs to be named here. modules_prepare sets up the generated
# headers/Module.symvers your out-of-tree driver needs, without requiring a
# full in-tree "make modules" pass.
# ---------------------------------------------------------------------------
echo ">> building kernel"
"${MAKE[@]}"

echo ">> preparing module build infrastructure"
"${MAKE[@]}" modules_prepare

IMAGE_PATH="${SRC_DIR}/${IMAGE_RELPATH}"
if [[ ! -f "${IMAGE_PATH}" ]]; then
    echo "error: expected kernel image not found at ${IMAGE_PATH}" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Write the build receipt that build-module.sh (and run scripts) source
# ---------------------------------------------------------------------------
RECEIPT="${OUT_DIR}/build.env"
cat > "${RECEIPT}" <<EOF
# Auto-generated by build-kernel.sh - do not edit by hand.
ARCH=${ARCH_ARG}
KARCH=${KARCH}
CROSS_COMPILE=${CROSS_COMPILE}
KERNEL_VERSION=${KERNEL_VERSION}
KDIR=${KDIR}
KERNEL_IMAGE=${IMAGE_PATH}
EOF

echo ""
echo ">> done."
echo "   kernel image: ${IMAGE_PATH}"
echo "   KDIR:         ${KDIR}"
echo "   receipt:      ${RECEIPT}"
echo ""
echo "Build your driver against this kernel with:"
echo "   ./kernel_driver/kmodule_build.sh   (reads ${RECEIPT} automatically)"
