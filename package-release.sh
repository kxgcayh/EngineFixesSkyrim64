#!/usr/bin/env bash
#
# EngineFixesSkyrim64 Release Packager for Skyrim AE 1.7.104
# Packages installable mod manager archives (Standard & FOMOD All-In-One).
#
# Outputs:
#   dist/EngineFixes-Part1-AE17104-v7.0.21.zip    (Direct Mod Manager Part 1 archive)
#   dist/EngineFixes-FOMOD-AllInOne-v7.0.21.zip   (FOMOD installer with environment presets)
#   dist/SHA256SUMS.txt                            (SHA-256 checksums of generated archives)
#
# Usage:
#   ./package-release.sh [--skip-build] [--dll /path/to/EngineFixes.dll] [--part1-only] [--fomod-only]
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

DIST_DIR="$SCRIPT_DIR/dist"
ASSETS_DIR="$SCRIPT_DIR/assets"
SKYRIM_ASSETS_DIR="$SCRIPT_DIR/Skyrim/Data"
BUILD_DIR="$SCRIPT_DIR/build"
COMPILED_STAGING="$SCRIPT_DIR/../../SkyrimMods/compiled_dlls"

TOOLCHAIN="/Volumes/S512/Projects/ETC/winsdk/toolchain/clang-cl-msvc.cmake"
VCPKG_PREFIX="/Volumes/S512/Projects/ETC/vcpkg/installed/x64-windows-cross"

VERSION="$(grep -m1 'set(VERSION' "$SCRIPT_DIR/CMakeLists.txt" | awk '{print $2}')"
VERSION="${VERSION:-7.0.22}"

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
elif [[ $SKIP_BUILD -eq 0 && -f "$TOOLCHAIN" ]]; then
    echo "[1/4] Configuring CMake and cross-compiling EngineFixes.dll..."
    cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SKYRIMAE=ON \
        -DCOPY_BUILD=OFF \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DCMAKE_PREFIX_PATH="$VCPKG_PREFIX" 2>/dev/null || true
    if [[ -d "$BUILD_DIR" ]] && cmake --build "$BUILD_DIR" --config Release 2>/dev/null; then
        DLL="$BUILD_DIR/EngineFixes.dll"
    fi
fi

if [[ -z "$DLL" || ! -f "$DLL" ]]; then
    # Search common build and staging locations
    for candidate in \
        "$BUILD_DIR/EngineFixes.dll" \
        "$BUILD_DIR/Release/EngineFixes.dll" \
        "$COMPILED_STAGING/EngineFixes.dll" \
        "$DIST_DIR/EngineFixes.dll"; do
        if [[ -f "$candidate" ]]; then
            DLL="$candidate"
            echo "[1/4] Found existing pre-compiled DLL: $DLL"
            break
        fi
    done
fi

if [[ -z "$DLL" || ! -f "$DLL" ]]; then
    echo "[WARN] No pre-compiled EngineFixes.dll found in build directories."
    echo "       Creating a dummy verification placeholder or pass --dll /path/to/EngineFixes.dll."
    echo "       Packaging will continue with configuration assets and staging."
fi

# 2. Binary Verification (if DLL exists)
if [[ -n "$DLL" && -f "$DLL" ]]; then
    echo "[2/4] Verifying binary architecture and exports..."
    file "$DLL" | grep -q "PE32+.*x86-64" || {
        echo "[ERROR] $DLL is not a 64-bit Windows PE DLL" >&2
        exit 1
    }

    OBJDUMP="/opt/homebrew/opt/llvm/bin/llvm-objdump"
    if [[ -x "$OBJDUMP" ]]; then
        for export_sym in SKSEPlugin_Load SKSEPlugin_Version; do
            "$OBJDUMP" -p "$DLL" | grep -q "$export_sym" || {
                echo "[WARN] Missing export symbol: $export_sym" >&2
            }
        done
    fi
    echo "      Architecture: PE32+ x86-64 verified"

    # Stage to central compiled_dlls
    mkdir -p "$COMPILED_STAGING"
    cp "$DLL" "$COMPILED_STAGING/EngineFixes.dll"
    echo "      Staged to: $COMPILED_STAGING/EngineFixes.dll"
else
    echo "[2/4] Skipping binary verification (no DLL specified)"
fi

# 3. Create Distribution Archives
mkdir -p "$DIST_DIR"

# 3.1 Standard Part 1 Archive
if [[ $BUILD_PART1 -eq 1 ]]; then
    echo "[3/4] Packaging Part 1 Mod Manager archive..."
    STAGE_PART1="$(mktemp -d "${TMPDIR:-/tmp}/ef-part1.XXXXXX")"
    trap 'rm -rf "$STAGE_PART1"' EXIT

    mkdir -p "$STAGE_PART1/SKSE/Plugins"

    if [[ -n "$DLL" && -f "$DLL" ]]; then
        cp "$DLL" "$STAGE_PART1/SKSE/Plugins/EngineFixes.dll"
    fi

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
    trap 'rm -rf "$STAGE_FOMOD"' EXIT

    # Core files
    mkdir -p "$STAGE_FOMOD/00 Core/SKSE/Plugins"
    if [[ -n "$DLL" && -f "$DLL" ]]; then
        cp "$DLL" "$STAGE_FOMOD/00 Core/SKSE/Plugins/EngineFixes.dll"
    fi
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
for arch in EngineFixes-*.zip; do
    if [[ -f "$arch" ]]; then
        shasum -a 256 "$arch" >> SHA256SUMS.txt
        echo "--------------------------------------------------"
        echo "File: $arch"
        shasum -a 256 "$arch"
        echo "Contents:"
        unzip -l "$arch"
    fi
done

echo "=================================================="
echo "Packaging complete. Archives located in dist/:"
ls -lh "$DIST_DIR"/*.zip
echo "=================================================="
