#!/usr/bin/env bash
#
# kmodule_build.sh - Build the out-of-tree kernel module in this directory
# (custom_qemu_device_driver.c, chardev.h, Makefile) against a kernel
# produced by ../kernel's build-kernel.sh.
#
# By default this sources the build.env receipt that build-kernel.sh wrote
# under the sibling kernel/ directory, so the module is automatically built
# with the same ARCH/CROSS_COMPILE/KDIR as the kernel it has to load into -
# the two build steps can never drift apart.
#
# Usage:
#   ./kmodule_build.sh [options]
#
# Options:
#   --driver-dir <dir>     Directory containing the module's Makefile (default: this directory)
#   --receipt <file>       Explicit build.env to source
#   --arch <arch>          Override target architecture (x86_64|arm64|arm|riscv64)
#   --kdir <dir>           Override kernel build directory
#   --cross-compile <pfx>  Override cross-compiler prefix
#   --jobs <n>             Parallel build jobs (default: nproc)
#   --clean                Run "make clean" first
#   -h, --help             Show this help
#
# If none of --receipt/--arch/--kdir are given, the script looks for exactly
# one <repo-root>/kernel/*/build.env and uses it automatically.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# The driver source, chardev.h and Makefile live right next to this script.
DRIVER_DIR="${SCRIPT_DIR}"
RECEIPT=""
ARCH=""
KDIR=""
CROSS_COMPILE=""
JOBS="$(nproc 2>/dev/null || echo 4)"
DO_CLEAN=0
ARCH_EXPLICIT=""
CROSS_EXPLICIT=""
KDIR_EXPLICIT=""

usage() {
    sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --driver-dir)     DRIVER_DIR="$2"; shift 2 ;;
        --receipt)        RECEIPT="$2"; shift 2 ;;
        --arch)           ARCH_EXPLICIT="$2"; shift 2 ;;
        --kdir)           KDIR_EXPLICIT="$2"; shift 2 ;;
        --cross-compile)  CROSS_EXPLICIT="$2"; shift 2 ;;
        --jobs)           JOBS="$2"; shift 2 ;;
        --clean)          DO_CLEAN=1; shift ;;
        -h|--help)        usage 0 ;;
        *) echo "Unknown argument: $1" >&2; usage 1 ;;
    esac
done

# Friendly arch name -> "kernel ARCH  default cross-compiler prefix"
map_arch() {
    case "$1" in
        x86_64)          echo "x86_64 " ;;
        arm64|aarch64)   echo "arm64 aarch64-linux-gnu-" ;;
        arm|armhf|armv7) echo "arm arm-linux-gnueabihf-" ;;
        riscv64)         echo "riscv riscv64-linux-gnu-" ;;
        *)               echo "" ;;
    esac
}

# ---------------------------------------------------------------------------
# Resolve ARCH / CROSS_COMPILE / KDIR: explicit flags always win; otherwise
# auto-locate and source the receipt build-kernel.sh wrote under
# <repo-root>/kernel/<arch>/build.env.
# ---------------------------------------------------------------------------
if [[ -z "${RECEIPT}" && -z "${KDIR_EXPLICIT}" ]]; then
    if [[ -n "${ARCH_EXPLICIT}" && -f "${REPO_ROOT}/kernel/${ARCH_EXPLICIT}/build.env" ]]; then
        RECEIPT="${REPO_ROOT}/kernel/${ARCH_EXPLICIT}/build.env"
    else
        mapfile -t CANDIDATES < <(find "${REPO_ROOT}/kernel" -maxdepth 2 -name build.env 2>/dev/null || true)
        if [[ "${#CANDIDATES[@]}" -eq 1 ]]; then
            RECEIPT="${CANDIDATES[0]}"
        elif [[ "${#CANDIDATES[@]}" -gt 1 ]]; then
            echo "error: multiple kernel builds found under ${REPO_ROOT}/kernel/, disambiguate with --receipt or --arch:" >&2
            printf '  %s\n' "${CANDIDATES[@]}" >&2
            exit 1
        fi
    fi
fi

if [[ -n "${RECEIPT}" ]]; then
    if [[ ! -f "${RECEIPT}" ]]; then
        echo "error: receipt not found: ${RECEIPT}" >&2
        exit 1
    fi
    echo ">> sourcing kernel build receipt: ${RECEIPT}"
    # shellcheck source=/dev/null
    source "${RECEIPT}"
    ARCH="${KARCH:-}"
fi

# Explicit flags override whatever the receipt provided.
if [[ -n "${ARCH_EXPLICIT}" ]]; then
    read -r MAPPED_ARCH MAPPED_CROSS <<< "$(map_arch "${ARCH_EXPLICIT}")"
    if [[ -z "${MAPPED_ARCH}" ]]; then
        echo "error: unsupported --arch '${ARCH_EXPLICIT}'" >&2
        exit 1
    fi
    ARCH="${MAPPED_ARCH}"
    CROSS_COMPILE="${MAPPED_CROSS}"
fi
[[ -n "${CROSS_EXPLICIT}" ]] && CROSS_COMPILE="${CROSS_EXPLICIT}"
[[ -n "${KDIR_EXPLICIT}" ]]   && KDIR="${KDIR_EXPLICIT}"

if [[ -z "${ARCH}" || -z "${KDIR:-}" ]]; then
    echo "error: could not determine ARCH/KDIR." >&2
    echo "       run ../kernel's build-kernel.sh first, or pass --arch and --kdir explicitly." >&2
    exit 1
fi

if [[ ! -d "${KDIR}" ]]; then
    echo "error: KDIR does not exist: ${KDIR}" >&2
    exit 1
fi

if [[ ! -f "${DRIVER_DIR}/Makefile" ]]; then
    echo "error: no Makefile found in driver directory: ${DRIVER_DIR}" >&2
    exit 1
fi

if [[ -n "${CROSS_COMPILE}" ]] && ! command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1; then
    echo "error: cross-compiler '${CROSS_COMPILE}gcc' not found in PATH" >&2
    exit 1
fi

echo ">> building module in ${DRIVER_DIR}"
echo "   ARCH=${ARCH}  CROSS_COMPILE=${CROSS_COMPILE:-<none>}  KDIR=${KDIR}"

MAKE_ARGS=(ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" -C "${KDIR}" M="${DRIVER_DIR}" -j"${JOBS}")

[[ "${DO_CLEAN}" -eq 1 ]] && make "${MAKE_ARGS[@]}" clean

make "${MAKE_ARGS[@]}" modules

echo ""
echo ">> done. built module(s):"
find "${DRIVER_DIR}" -maxdepth 1 -name '*.ko' -print
