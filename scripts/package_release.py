#!/usr/bin/env python3
"""
EngineFixesSkyrim64 Cross-Platform Release Packager
Packages installable Mod Manager archives (Part 1 and FOMOD All-In-One).
Compatible with Windows, macOS, and Linux using pure standard library.
"""

import argparse
import hashlib
import os
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

VERSION = "7.0.23"
GAME_VERSION = "AE17104"
MOD_ID = 17230

def calculate_sha256(file_path: Path) -> str:
    h = hashlib.sha256()
    with open(file_path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def make_meta_ini(notes: str) -> str:
    return f"""[General]
gameName=SkyrimSE
modid={MOD_ID}
version={VERSION}
newestVersion={VERSION}
category=39
notes={notes}
author=aers
installedFiles\\size=0
"""

def create_archive_from_dir(src_dir: Path, output_zip: Path) -> None:
    output_zip.parent.mkdir(parents=True, exist_ok=True)
    if output_zip.exists():
        output_zip.unlink()
    with zipfile.ZipFile(output_zip, "w", zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(src_dir):
            for file in sorted(files):
                abs_path = Path(root) / file
                rel_path = abs_path.relative_to(src_dir)
                zf.write(abs_path, rel_path)

def package_part1(root: Path, dist: Path, dll_path: Path | None) -> Path:
    print("[+] Packaging Part 1 Mod Manager archive...")
    with tempfile.TemporaryDirectory() as tmp:
        stage = Path(tmp)
        plugin_dir = stage / "SKSE" / "Plugins"
        plugin_dir.mkdir(parents=True, exist_ok=True)

        if dll_path and dll_path.is_file():
            shutil.copy2(dll_path, plugin_dir / "EngineFixes.dll")

        skyrim_assets = root / "Skyrim" / "Data" / "SKSE" / "Plugins"
        shutil.copy2(skyrim_assets / "EngineFixes.toml", plugin_dir / "EngineFixes.toml")
        shutil.copy2(skyrim_assets / "EngineFixes_SNCT.ini", plugin_dir / "EngineFixes_SNCT.ini")
        shutil.copy2(skyrim_assets / "EngineFixes_preload.txt", plugin_dir / "EngineFixes_preload.txt")

        (stage / "meta.ini").write_text(make_meta_ini("Engine Fixes for Skyrim AE 1.7.104 with macOS CrossOver and D3DMetal compatibility"))

        out_zip = dist / f"EngineFixes-Part1-{GAME_VERSION}-v{VERSION}.zip"
        create_archive_from_dir(stage, out_zip)
        print(f"    Created: {out_zip}")
        return out_zip

def package_fomod(root: Path, dist: Path, dll_path: Path | None) -> Path:
    print("[+] Packaging FOMOD All-In-One archive...")
    with tempfile.TemporaryDirectory() as tmp:
        stage = Path(tmp)

        # 00 Core
        core_dir = stage / "00 Core" / "SKSE" / "Plugins"
        core_dir.mkdir(parents=True, exist_ok=True)
        if dll_path and dll_path.is_file():
            shutil.copy2(dll_path, core_dir / "EngineFixes.dll")

        skyrim_assets = root / "Skyrim" / "Data" / "SKSE" / "Plugins"
        shutil.copy2(skyrim_assets / "EngineFixes_SNCT.ini", core_dir / "EngineFixes_SNCT.ini")
        shutil.copy2(skyrim_assets / "EngineFixes_preload.txt", core_dir / "EngineFixes_preload.txt")

        # Presets
        presets_dir = root / "assets" / "presets"
        shutil.copytree(presets_dir / "mac_crossover", stage / "01 Preset - macOS CrossOver")
        shutil.copytree(presets_dir / "windows_standard", stage / "02 Preset - Native Windows")
        shutil.copytree(presets_dir / "clean_fast", stage / "03 Preset - Clean & Fast")

        # FOMOD XML metadata
        fomod_dir = stage / "fomod"
        fomod_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / "assets" / "fomod" / "info.xml", fomod_dir / "info.xml")
        shutil.copy2(root / "assets" / "fomod" / "ModuleConfig.xml", fomod_dir / "ModuleConfig.xml")

        (stage / "meta.ini").write_text(make_meta_ini("Engine Fixes All-In-One FOMOD with CrossOver / D3DMetal preset"))

        out_zip = dist / f"EngineFixes-FOMOD-AllInOne-v{VERSION}.zip"
        create_archive_from_dir(stage, out_zip)
        print(f"    Created: {out_zip}")
        return out_zip

def main():
    parser = argparse.ArgumentParser(description="Package EngineFixes release archives.")
    parser.add_argument("--dll", type=Path, default=None, help="Path to pre-compiled EngineFixes.dll")
    parser.add_argument("--part1-only", action="store_true", help="Build only Part 1 archive")
    parser.add_argument("--fomod-only", action="store_true", help="Build only FOMOD archive")
    parser.add_argument("--dist", type=Path, default=None, help="Output directory (default: ./dist)")
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    dist = args.dist if args.dist else root / "dist"
    dist.mkdir(parents=True, exist_ok=True)

    dll_path = args.dll
    if not dll_path or not dll_path.is_file():
        # Check standard build and staging locations
        candidates = [
            root / "build" / "EngineFixes.dll",
            root / "build" / "Release" / "EngineFixes.dll",
            root.parent.parent / "SkyrimMods" / "compiled_dlls" / "EngineFixes.dll",
            dist / "EngineFixes.dll",
        ]
        for c in candidates:
            if c.is_file():
                dll_path = c
                print(f"[+] Found pre-compiled DLL: {dll_path}")
                break

    archives = []
    if not args.fomod_only:
        archives.append(package_part1(root, dist, dll_path))
    if not args.part1_only:
        archives.append(package_fomod(root, dist, dll_path))

    # Checksums
    checksum_file = dist / "SHA256SUMS.txt"
    with open(checksum_file, "w") as f:
        for arch in archives:
            sha = calculate_sha256(arch)
            f.write(f"{sha}  {arch.name}\n")
            print(f"[+] {arch.name}: {sha}")

    print("==================================================")
    print("Packaging complete. Checksums saved to SHA256SUMS.txt")
    print("==================================================")

if __name__ == "__main__":
    main()
