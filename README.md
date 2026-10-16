# Engine Fixes for Skyrim Special Edition / Anniversary Edition

An SKSE64 plugin designed to fix engine bugs, patch memory allocation bottlenecks, and stabilize Skyrim Special Edition and Anniversary Edition on both native Windows and macOS (via CrossOver, D3DMetal, and Apple Game Porting Toolkit).

---

## 1. Compatibility

- **Supported Skyrim Versions**:
  - Anniversary Edition: `1.7.104`, `1.7.99`, `1.6.1170`, `1.6.1130`
  - Special Edition: `1.5.97`
- **SKSE Version**: SKSE64 2.2.6 through 2.3.1+ (AddressLibraryV5 compliant)
- **Platforms**:
  - Native Windows 10 / 11 (x86_64)
  - macOS 13+ Apple Silicon (M1/M2/M3/M4) via CrossOver 23+, Wine 8+, and D3DMetal / GPTK 1/2/3/4

---

## 2. Key Enhancements & Fixes in this Fork

- **macOS / CrossOver / D3DMetal Compatibility Layer**:
  - Dynamic Wine runtime detection via `ntdll.dll` (`wine_get_version` / `wine_get_host_version`).
  - Adaptive memory allocator: Automatically defaults to `CRTAllocator` (`malloc` / `_aligned_malloc`) under Wine/CrossOver to leverage Darwin libc's scalable unified memory allocator and avoid Intel TBB (`tbbmalloc.dll`) spinlock contention on Apple Silicon.
  - Delay-loads `tbbmalloc.dll` so the plugin starts cleanly even if `tbbmalloc.dll` was not manually copied into the root game directory.
  - Modal dialog suppression: Replaces blocking Win32 `MessageBoxW` calls with safe non-blocking log output under Wine, preventing display server deadlocks and fullscreen freezes during `SafeExit` and ref-handle limit warnings.
- **x64 ABI Register Preservation**:
  - Corrected volatile general-purpose register (`RAX`, `RCX`, `RDX`, `R8`-`R11`) and SIMD register (`XMM0`-`XMM5`) preservation in the waterflow animation trampoline hook, preventing engine main loop state corruption.
- **Null Pointer Dereference Guards**:
  - Safe function pointer invocation for `_getmaxstdio` in `MaxStdIO`.
  - Null owner guards for dynamic sound categories in `SaveAddedSoundCategories`.
  - Cell attachment verification (`parentCell && parentCell->IsAttached()`) and stipple fade check in `TreeLodReferenceCaching`.
  - Defensive INI pointer resolution in `SaveScreenshots`.
- **ESL Light Plugin Tree LOD Support**:
  - Extended distant tree reference caching to iterate `compiledFileCollection.smallFiles` using the ESL FormID masking scheme (`0xFE000000 | (slot << 12) | localId`), resolving LOD caching for light masters.
- **Zero-Dependency Co-Save Extension Check**:
  - Removed heavy `boost::regex` dependency in favor of standard `<filesystem>`, simplifying the build pipeline and reducing binary footprint.
- **Integrated Offline Unit Test Suite**:
  - Standalone test target (`BUILD_TESTS=ON`) validating cosave extension filtering, ESL FormID calculations, and memory allocator selection logic.

---

## 3. Build Requirements

### 3.1 Native Windows Build
- **Operating System**: Windows 10 or 11 (x86_64)
- **Compiler**: Microsoft Visual Studio 2022 (MSVC v143, Version 17.4 or newer) or Clang-CL
- **Build System**: CMake 3.23 or newer, Ninja or MSBuild
- **Package Manager**: vcpkg (configured in manifest mode)
- **Submodules**: Git with recursive submodules initialized (`extern/CommonLibSSE`)

### 3.2 macOS / Unix Cross-Compilation
- **Operating System**: macOS (Apple Silicon or Intel) / Linux
- **Compiler**: LLVM Clang-CL 16+ (`clang-cl`, `lld-link`)
- **Sysroot**: MSVC CRT and Windows SDK headers/libraries (unpacked via `xwin`)
- **Build System**: CMake 3.23+, Ninja
- **Cross-Triplet**: vcpkg with `x64-windows` or `x64-windows-cross`

---

## 4. How to Build

### 4.1 Clone Repository and Submodules

```bash
git clone --recurse-submodules https://github.com/aers/EngineFixesSkyrim64.git
cd EngineFixesSkyrim64

# If cloned without --recurse-submodules:
git submodule update --init --recursive
```

### 4.2 Building on Native Windows (MSVC)

#### Using CMake Presets / Command Line:

```bash
# Configure for Skyrim Anniversary Edition (AE 1.6.1130 - 1.7.104)
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SKYRIMAE=ON \
  -DCOPY_BUILD=OFF \
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"

# Build the DLL
cmake --build build --config Release
```

The resulting `EngineFixes.dll` and `EngineFixes.pdb` will be located in `build/Release/`.

#### For Special Edition (SSE 1.5.97):
Omit `-DBUILD_SKYRIMAE=ON` during configuration:
```bash
cmake -B build-sse -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SKYRIMAE=OFF \
  -DCOPY_BUILD=OFF \
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"

cmake --build build-sse --config Release
```

---

### 4.3 Cross-Compiling on macOS via Clang-CL & MSVC Sysroot

Set up your environment variables pointing to LLVM and the Windows SDK sysroot:

```bash
export LLVM_PATH="/opt/homebrew/opt/llvm"
export WINSDK_PATH="/path/to/winsdk"
export VCPKG_ROOT="/path/to/vcpkg"

cmake -B build-cross -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SKYRIMAE=ON \
  -DCOPY_BUILD=OFF \
  -DCMAKE_C_COMPILER="${LLVM_PATH}/bin/clang-cl" \
  -DCMAKE_CXX_COMPILER="${LLVM_PATH}/bin/clang-cl" \
  -DCMAKE_LINKER="${LLVM_PATH}/bin/lld-link" \
  -DCMAKE_TOOLCHAIN_FILE="${WINSDK_PATH}/toolchain/clang-cl-msvc.cmake" \
  -DVCPKG_TARGET_TRIPLET="x64-windows-cross"

cmake --build build-cross
```

---

### 4.4 Building and Running Unit Tests

The test suite does not require Windows SDK headers or the game executable and can be built and run on any platform:

#### Via CMake:
```bash
cmake -B build-tests -S . -DBUILD_TESTS=ON
cmake --build build-tests --target EngineFixes_Tests
ctest --test-dir build-tests --output-on-failure
```

#### Directly via Clang++ / G++:
```bash
clang++ -std=c++20 tests/test_main.cpp -Isrc -o enginefixes_tests
./enginefixes_tests
```

Expected output:
```
========================================
EngineFixes Unit Tests
========================================
[PASS] test_has_skse_extension
[PASS] test_esl_formid_calculation
[PASS] test_hex_formatting
[PASS] test_max_stdio_fallback_logic
[PASS] test_wine_memory_allocator_selection
[PASS] test_wine_dialog_suppression
----------------------------------------
Results: 6 passed, 0 failed.
========================================
```

---

### 4.5 Packaging Mod Manager Archives

The repository provides automated packager scripts to bundle installable zip archives for Mod Organizer 2 and Vortex:

#### Bash Packager (macOS / Linux / Git Bash):
```bash
# Package all archives (Standard Part 1 and FOMOD All-In-One):
./package-release.sh --skip-build

# Or specify a custom compiled DLL path:
./package-release.sh --dll /path/to/EngineFixes.dll

# Package only the FOMOD interactive installer:
./package-release.sh --fomod-only
```

#### Python Packager (Cross-Platform):
```bash
python3 scripts/package_release.py [--dll /path/to/EngineFixes.dll] [--fomod-only] [--part1-only]
```

Outputs generated in `dist/`:
- `EngineFixes-Part1-AE17104-v7.0.23.zip`: Direct mod manager installable archive (`SKSE/Plugins/`).
- `EngineFixes-FOMOD-AllInOne-v7.0.23.zip`: Interactive FOMOD installer with presets for macOS CrossOver, Native Windows, and Clean/Fast.
- `SHA256SUMS.txt`: SHA-256 integrity checksums for generated archives.

---

## 5. Configuration & TOML Options

Configuration files reside in `Data/skse/plugins/`:
- `EngineFixes.toml` (Default settings)
- `EngineFixesCustom.toml` (User overrides; preserved across updates)

### Important Settings:

```toml
[General]
bVerboseLogging = false        # Enable trace-level logging to EngineFixes.log
bCleanSKSECoSaves = false      # Automatically delete orphaned .skse co-saves
bWineCompatibility = true     # Auto-detect Wine/CrossOver, use Darwin CRT allocator, suppress modal freezes

[Patches]
bMaxStdIO = true               # Raise maximum open file handles to 8192
bTreeLodReferenceCaching = true# Cache distant tree references (supports ESM/ESP/ESL)
bWaterflowAnimation = true     # Fixes rapid water animations at high framerates
bSafeExit = true               # Terminate process immediately upon quitting to desktop

[MemoryManager]
bOverrideMemoryManager = true  # Replaces game allocator with scalable memory manager
bOverrideScrapHeap = true      # Overrides ScrapHeap allocations
bOverrideScaleformAllocator = true # Overrides Scaleform GFx memory heap
```

---

## 6. Project Structure

```
EngineFixesSkyrim64/
├── CMakeLists.txt             # Primary build definition
├── vcpkg.json                 # Package dependencies (safetyhook, tbb)
├── src/
│   ├── main.cpp               # SKSE plugin entry points (PreLoad, Query, Load)
│   ├── settings.h             # Configuration parser and schema
│   ├── clean_cosaves.h/.cpp   # Orphaned co-save cleanup
│   ├── fixes/                 # Bug fixes (shaders, memory access, animations)
│   ├── memory/                # Memory manager overrides & CRT/TBB allocators
│   ├── patches/               # Engine patches (MaxStdIO, Waterflow, Tree LOD)
│   ├── util/
│   │   ├── environment.h      # Wine/CrossOver and Darwin runtime detection
│   │   └── dialog.h           # Non-blocking dialog wrapper for Wine
│   └── warnings/              # Ref-handle limit warnings
└── tests/
    └── test_main.cpp          # Standalone offline unit test suite
```

---

## 7. License & Attribution

- **Original Author**: aers
- **Contributors**: Ryan (CommonLibSSE), Charmed Baryon (CommonLibSSE-NG), EngineFixes contributors
- **License**: See individual source files and upstream repository for licensing details.