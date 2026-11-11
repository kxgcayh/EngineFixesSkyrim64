#!/usr/bin/env bash
#
# EngineFixesSkyrim64 Release Packager for Skyrim AE 1.7.104
# Packages installable mod manager archives (Standard & FOMOD All-In-One).
#
# Outputs (<version> is set(VERSION ...) in CMakeLists.txt):
#   dist/EngineFixes-Part1-AE17104-v<version>.zip    (Direct Mod Manager Part 1 archive)
#   dist/EngineFixes-FOMOD-AllInOne-v<version>.zip   (FOMOD installer with environment presets)
#   dist/SHA256SUMS.txt                               (SHA-256 checksums of generated archives)
#
# Usage:
#   ./package-release.sh [--skip-build] [--dll /path/to/EngineFixes.dll] [--part1-only] [--fomod-only]
#
# Building (no --dll / --skip-build) cross-compiles with clang-cl, configured like README section 4.3:
#   WINSDK_PATH        Windows SDK sysroot; the toolchain file is $WINSDK_PATH/toolchain/clang-cl-msvc.cmake
#   VCPKG_ROOT         vcpkg root; packages from $VCPKG_ROOT/installed/x64-windows-cross
#   LLVM_PATH          optional, LLVM install whose bin/llvm-objdump checks the exports (else PATH)
#   EF_TOOLCHAIN_FILE  optional, overrides the toolchain file
#   EF_VCPKG_PREFIX    optional, overrides the vcpkg installed prefix
#   EF_STAGE_DIR       optional, folder that also receives a copy of the packaged DLL
#                      (and is searched with --skip-build). Unset: nothing is written outside this repo.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

DIST_DIR="$SCRIPT_DIR/dist"
ASSETS_DIR="$SCRIPT_DIR/assets"
SKYRIM_ASSETS_DIR="$SCRIPT_DIR/Skyrim/Data"
BUILD_DIR="$SCRIPT_DIR/build"
STAGE_DIR="${EF_STAGE_DIR:-}"

TOOLCHAIN="${EF_TOOLCHAIN_FILE:-${WINSDK_PATH:+$WINSDK_PATH/toolchain/clang-cl-msvc.cmake}}"
VCPKG_PREFIX="${EF_VCPKG_PREFIX:-${VCPKG_ROOT:+$VCPKG_ROOT/installed/x64-windows-cross}}"

VERSION="$(grep -m1 'set(VERSION' "$SCRIPT_DIR/CMakeLists.txt" | awk '{print $2}')"
if [[ -z "$VERSION" ]]; then
    echo "[ERROR] No set(VERSION ...) found in CMakeLists.txt" >&2
    exit 1
fi

STAGE_PART1=""
STAGE_FOMOD=""
cleanup() {
    [[ -n "$STAGE_PART1" ]] && rm -rf "$STAGE_PART1"
    [[ -n "$STAGE_FOMOD" ]] && rm -rf "$STAGE_FOMOD"
    return 0
}
trap cleanup EXIT

SKIP_BUILD=0
CUSTOM_DLL=""
BUILD_PART1=1
BUILD_FOMOD=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --dll)
            CUSTOM_DLL="$2"
            SKIP_BUILD=1
            shift 2
            ;;
        --part1-only)
            BUILD_FOMOD=0
            shift
            ;;
        --fomod-only)
            BUILD_PART1=0
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [--skip-build] [--dll <path>] [--part1-only] [--fomod-only]"
            exit 0
            ;;
        *)
            echo "[ERROR] Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

echo "=================================================="
echo "EngineFixes AE 1.7.104 Mod Manager Release Packager"
echo "=================================================="

# 1. Compilation / Resolution of DLL
DLL=""
if [[ -n "$CUSTOM_DLL" ]]; then
    DLL="$CUSTOM_DLL"
    echo "[1/4] Using specified DLL: $DLL"
elif [[ $SKIP_BUILD -eq 0 ]]; then
    if [[ -z "$TOOLCHAIN" || ! -f "$TOOLCHAIN" || -z "$VCPKG_PREFIX" ]]; then
        echo "[ERROR] Cannot build: set WINSDK_PATH and VCPKG_ROOT as in README section 4.3" >&2
        echo "        (or EF_TOOLCHAIN_FILE / EF_VCPKG_PREFIX), or package an existing DLL with" >&2
        echo "        --dll /path/to/EngineFixes.dll or --skip-build." >&2
        echo "        Toolchain file: ${TOOLCHAIN:-<unset>}" >&2
        exit 1
    fi
    echo "[1/4] Configuring CMake and cross-compiling EngineFixes.dll..."
    cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SKYRIMAE=ON \
        -DCOPY_BUILD=OFF \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DCMAKE_PREFIX_PATH="$VCPKG_PREFIX"
    cmake --build "$BUILD_DIR" --config Release
    DLL="$BUILD_DIR/EngineFixes.dll"
else
    # --skip-build: package a DLL built earlier
    candidates=("$BUILD_DIR/EngineFixes.dll" "$BUILD_DIR/Release/EngineFixes.dll")
    [[ -n "$STAGE_DIR" ]] && candidates+=("$STAGE_DIR/EngineFixes.dll")
    candidates+=("$DIST_DIR/EngineFixes.dll")
    for candidate in "${candidates[@]}"; do
        if [[ -f "$candidate" ]]; then
            DLL="$candidate"
            echo "[1/4] Found existing pre-compiled DLL: $DLL"
            break
        fi
    done
fi

if [[ -z "$DLL" || ! -f "$DLL" ]]; then
    echo "[ERROR] No EngineFixes.dll to package${DLL:+ ($DLL does not exist)}." >&2
    echo "        Build it, or pass --dll /path/to/EngineFixes.dll. No archives were written." >&2
    exit 1
fi

# 2. Binary Verification
echo "[2/4] Verifying binary architecture and exports..."
file "$DLL" | grep -q "PE32+.*x86-64" || {
    echo "[ERROR] $DLL is not a 64-bit Windows PE DLL" >&2
    exit 1
}

OBJDUMP=""
if [[ -n "${LLVM_PATH:-}" && -x "$LLVM_PATH/bin/llvm-objdump" ]]; then
    OBJDUMP="$LLVM_PATH/bin/llvm-objdump"
elif command -v llvm-objdump >/dev/null 2>&1; then
    OBJDUMP="$(command -v llvm-objdump)"
fi
if [[ -n "$OBJDUMP" ]]; then
    for export_sym in SKSEPlugin_Load SKSEPlugin_Version; do
        "$OBJDUMP" -p "$DLL" | grep -q "$export_sym" || {
            echo "[WARN] Missing export symbol: $export_sym" >&2
        }
    done
else
    echo "      llvm-objdump not found (set LLVM_PATH or add it to PATH): export check skipped"
fi
echo "      Architecture: PE32+ x86-64 verified"

if [[ -n "$STAGE_DIR" ]]; then
    mkdir -p "$STAGE_DIR"
    if [[ "$(cd "$(dirname "$DLL")" && pwd)/$(basename "$DLL")" != "$(cd "$STAGE_DIR" && pwd)/EngineFixes.dll" ]]; then
        cp "$DLL" "$STAGE_DIR/EngineFixes.dll"
    fi
    echo "      Staged to: $STAGE_DIR/EngineFixes.dll"
fi

# 3. Create Distribution Archives
mkdir -p "$DIST_DIR"

# 3.1 Standard Part 1 Archive
if [[ $BUILD_PART1 -eq 1 ]]; then
    echo "[3/4] Packaging Part 1 Mod Manager archive..."
    STAGE_PART1="$(mktemp -d "${TMPDIR:-/tmp}/ef-part1.XXXXXX")"

    mkdir -p "$STAGE_PART1/SKSE/Plugins"
    cp "$DLL" "$STAGE_PART1/SKSE/Plugins/EngineFixes.dll"

    cp "$SKYRIM_ASSETS_DIR/SKSE/Plugins/EngineFixes.toml" "$STAGE_PART1/SKSE/Plugins/EngineFixes.toml"
    cp "$SKYRIM_ASSETS_DIR/SKSE/Plugins/EngineFixes_SNCT.ini" "$STAGE_PART1/SKSE/Plugins/EngineFixes_SNCT.ini"
    cp "$SKYRIM_ASSETS_DIR/SKSE/Plugins/EngineFixes_preload.txt" "$STAGE_PART1/SKSE/Plugins/EngineFixes_preload.txt"

    cat << EOF > "$STAGE_PART1/meta.ini"
[General]
gameName=SkyrimSE
modid=17230
version=$VERSION
newestVersion=$VERSION
category=39
notes=Engine Fixes for Skyrim AE 1.7.104 with macOS CrossOver and D3DMetal compatibility
author=aers
installedFiles\\size=0
EOF

    ARCHIVE_PART1="$DIST_DIR/EngineFixes-Part1-AE17104-v$VERSION.zip"
    rm -f "$ARCHIVE_PART1"
    (cd "$STAGE_PART1" && zip -qr "$ARCHIVE_PART1" .)
    echo "      Created: $ARCHIVE_PART1"
fi

# 3.2 FOMOD All-In-One Archive
if [[ $BUILD_FOMOD -eq 1 ]]; then
    echo "[3/4] Packaging FOMOD All-In-One archive..."
    STAGE_FOMOD="$(mktemp -d "${TMPDIR:-/tmp}/ef-fomod.XXXXXX")"

    # Core files
    mkdir -p "$STAGE_FOMOD/00 Core/SKSE/Plugins"
    cp "$DLL" "$STAGE_FOMOD/00 Core/SKSE/Plugins/EngineFixes.dll"
    cp "$SKYRIM_ASSETS_DIR/SKSE/Plugins/EngineFixes_SNCT.ini" "$STAGE_FOMOD/00 Core/SKSE/Plugins/EngineFixes_SNCT.ini"
    cp "$SKYRIM_ASSETS_DIR/SKSE/Plugins/EngineFixes_preload.txt" "$STAGE_FOMOD/00 Core/SKSE/Plugins/EngineFixes_preload.txt"

    # Presets
    cp -R "$ASSETS_DIR/presets/mac_crossover" "$STAGE_FOMOD/01 Preset - macOS CrossOver"
    cp -R "$ASSETS_DIR/presets/windows_standard" "$STAGE_FOMOD/02 Preset - Native Windows"
    cp -R "$ASSETS_DIR/presets/clean_fast" "$STAGE_FOMOD/03 Preset - Clean & Fast"

    # FOMOD XML metadata
    mkdir -p "$STAGE_FOMOD/fomod"
    cp "$ASSETS_DIR/fomod/info.xml" "$STAGE_FOMOD/fomod/info.xml"
    cp "$ASSETS_DIR/fomod/ModuleConfig.xml" "$STAGE_FOMOD/fomod/ModuleConfig.xml"

    cat << EOF > "$STAGE_FOMOD/meta.ini"
[General]
gameName=SkyrimSE
modid=17230
version=$VERSION
newestVersion=$VERSION
category=39
notes=Engine Fixes All-In-One FOMOD with CrossOver / D3DMetal preset
author=aers
installedFiles\\size=0
EOF

    ARCHIVE_FOMOD="$DIST_DIR/EngineFixes-FOMOD-AllInOne-v$VERSION.zip"
    rm -f "$ARCHIVE_FOMOD"
    (cd "$STAGE_FOMOD" && zip -qr "$ARCHIVE_FOMOD" .)
    echo "      Created: $ARCHIVE_FOMOD"
fi

# 4. Checksums & Verification
echo "[4/4] Generating checksums and verifying archives..."
cd "$DIST_DIR"
rm -f SHA256SUMS.txt
# Only this run's archives: older versions left in dist/ must not enter the checksum file.
BUILT=()
[[ $BUILD_PART1 -eq 1 ]] && BUILT+=("$(basename "$ARCHIVE_PART1")")
[[ $BUILD_FOMOD -eq 1 ]] && BUILT+=("$(basename "$ARCHIVE_FOMOD")")
for arch in "${BUILT[@]}"; do
    shasum -a 256 "$arch" >> SHA256SUMS.txt
    echo "--------------------------------------------------"
    echo "File: $arch"
    shasum -a 256 "$arch"
    echo "Contents:"
    unzip -l "$arch"
done

echo "=================================================="
echo "Packaging complete. Archives in dist/:"
for arch in "${BUILT[@]}"; do ls -lh "$arch"; done
echo "=================================================="
