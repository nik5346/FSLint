#!/usr/bin/env python3
"""
Build FSLint as WASM locally.
Automatically finds or installs Emscripten.

Usage:
    python scripts/build_wasm_local.py
    python scripts/build_wasm_local.py --clean
    python scripts/build_wasm_local.py --skip-web
"""

import argparse
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile
import tempfile
from pathlib import Path

# ── paths ────────────────────────────────────────────────────────────────────

# Script lives in <root>/scripts/, so root is one level up
ROOT        = Path(__file__).resolve().parent.parent
BUILD_DIR   = ROOT / "build-wasm"
WEB_DIR     = ROOT / "web"
WEB_PUBLIC  = WEB_DIR / "dist"

# Default place to install emsdk when it can't be found anywhere
EMSDK_INSTALL_DIR = Path(os.environ.get("LOCALAPPDATA", Path.home())) / "emsdk"

# Filled in by find_or_install_emsdk()
EMSDK_ROOT: Path = None

# Filled in by find_ninja()
NINJA_PATH: Path = None


# ── subprocess helper ────────────────────────────────────────────────────────

def em_env() -> dict:
    """
    Build an environment dict with the EMSDK vars that emcmake.bat/emmake.bat need.
    emsdk activate only sets these in the calling shell session; we must inject
    them manually into every subprocess that calls an emscripten .bat file.
    """
    env = os.environ.copy()
    if EMSDK_ROOT:
        python_exe = EMSDK_ROOT / "python" / "3.13.3_64bit" / "python.exe"
        if not python_exe.exists():
            # Fallback: find any python exe under emsdk/python/
            hits = list((EMSDK_ROOT / "python").glob("*/python.exe")) if (EMSDK_ROOT / "python").exists() else []
            python_exe = hits[0] if hits else None
        node_exe = EMSDK_ROOT / "node"
        node_hits = list(node_exe.glob("*/bin/node.exe")) if node_exe.exists() else []

        env["EMSDK"] = str(EMSDK_ROOT)
        # Add Ninja to PATH so emcmake can find it even in a plain shell
        # without VS environment loaded.
        if NINJA_PATH:
            env["PATH"] = str(NINJA_PATH) + os.pathsep + env.get("PATH", "")
        if python_exe:
            env["EMSDK_PYTHON"] = str(python_exe)
        if node_hits:
            env["EMSDK_NODE"] = str(node_hits[0])
    return env



def run(*args, cwd: Path = None, use_em_env: bool = False):
    """
    Run a command given as individual string arguments.
    Always uses a flat string + shell=True so Windows .bat files and paths
    with spaces both work without any quoting gymnastics.
    """
    # Build a shell-safe string: quote every token that contains a space
    cmd = " ".join(f'"{a}"' if (" " in str(a) and not str(a).startswith('"')) else str(a)
                   for a in args)
    print(f"\n>>> {cmd}" + (f"  [cwd: {cwd}]" if cwd else ""))
    env = em_env() if use_em_env else None
    r = subprocess.run(cmd, shell=True, cwd=str(cwd) if cwd else None, env=env)
    if r.returncode != 0:
        print(f"[ERROR] command exited with code {r.returncode}")
        sys.exit(r.returncode)


# ── ninja detection ─────────────────────────────────────────────────────────

def find_ninja() -> Path:
    """
    Try to find the Ninja executable from Visual Studio installs.
    Checks multiple VS versions and editions, then PATH.
    Returns the directory containing ninja.exe, or None if not found.
    """
    # First, try to find ninja in PATH
    try:
        result = subprocess.run("where ninja", shell=True, capture_output=True, text=True)
        if result.returncode == 0:
            ninja_path = Path(result.stdout.strip().split("\n")[0]).parent
            print(f"[OK] Found Ninja in PATH: {ninja_path}")
            return ninja_path
    except Exception:
        pass

    # Check common Visual Studio installation locations
    # VSPath is typically: C:\Program Files\Microsoft Visual Studio\YEAR\EDITION
    program_files = os.environ.get("ProgramFiles")
    vs_base = Path(program_files) / "Microsoft Visual Studio"
    if vs_base.exists():
        # Try years in reverse order (newest first)
        for year_dir in sorted(vs_base.iterdir(), reverse=True):
            if not year_dir.is_dir():
                continue
            # Try each edition (Community, Professional, Enterprise)
            for edition_dir in year_dir.iterdir():
                if not edition_dir.is_dir():
                    continue
                ninja_candidate = (
                    edition_dir / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja"
                )
                if ninja_candidate.exists() and (ninja_candidate / "ninja.exe").exists():
                    print(f"[OK] Found Ninja in VS install: {ninja_candidate}")
                    return ninja_candidate
    
    print("[WARNING] Ninja not found in PATH or Visual Studio installations")
    return None





def emsdk_bat() -> Path:
    return EMSDK_ROOT / "emsdk.bat"

def emcmake() -> Path:
    return EMSDK_ROOT / "upstream" / "emscripten" / "emcmake.bat"

def emmake() -> Path:
    return EMSDK_ROOT / "upstream" / "emscripten" / "emmake.bat"

def tools_present() -> bool:
    return emcmake().exists() and emmake().exists()


def find_or_install_emsdk():
    global EMSDK_ROOT
    EMSDK_ROOT = EMSDK_INSTALL_DIR

    if tools_present():
        print(f"[OK] Found Emscripten at {emcmake()}")
        return

    if (EMSDK_INSTALL_DIR / "emsdk.bat").exists():
        print(f"[INFO] emsdk found at {EMSDK_INSTALL_DIR} but tools not installed — installing now...")
    else:
        print(f"[INFO] emsdk not found. Downloading to {EMSDK_INSTALL_DIR} ...")
        download_emsdk(EMSDK_INSTALL_DIR)

    run(emsdk_bat(), "install", "latest", cwd=EMSDK_ROOT)
    run(emsdk_bat(), "activate", "latest", cwd=EMSDK_ROOT)
    if not tools_present():
        print("[ERROR] Installation finished but emcmake.bat still not found.")
        sys.exit(1)
    print(f"[OK] Emscripten installed at {emcmake()}")


def download_emsdk(target: Path):
    url = "https://github.com/emscripten-core/emsdk/archive/refs/heads/main.zip"
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        zip_path = Path(tmp) / "emsdk.zip"
        print(f"  Downloading {url} ...")

        def progress(count, block, total):
            if total > 0:
                print(f"\r  {min(count*block*100//total, 100)}%", end="", flush=True)

        urllib.request.urlretrieve(url, zip_path, progress)
        print()
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(tmp)
        extracted = next(Path(tmp).glob("emsdk-*"))
        shutil.move(str(extracted), str(target))
    print(f"  Extracted to {target}")


# ── build steps ──────────────────────────────────────────────────────────────

def clean():
    print("\n--- clean ---")
    if BUILD_DIR.exists():
        print(f"  removing {BUILD_DIR}")

        def on_err(func, path, _exc):
            os.chmod(path, 0o777)
            func(path)

        shutil.rmtree(BUILD_DIR,
                      onexc=on_err if sys.version_info >= (3, 12) else None,
                      onerror=None if sys.version_info >= (3, 12) else on_err)

    for name in ("FSLint-cli-wasm.js", "FSLint-cli-wasm.wasm"):
        f = WEB_PUBLIC / name
        if f.exists():
            f.unlink()
            print(f"  removed {f}")


def build_wasm():
    print("\n--- configure (emcmake cmake) ---")
    # Only configure if build dir doesn't exist; otherwise use incremental builds
    if not BUILD_DIR.exists():
        BUILD_DIR.mkdir(parents=True, exist_ok=True)
        # zlib v1.3.1 has a bug where it creates both shared+static targets pointing
        # to the same libz.a under Emscripten, causing a ninja "multiple rules" error.
        # Fix: clone a patched zlib ourselves and point FetchContent at it so
        # CMakeLists.txt is never touched.
        zlib_dir = EMSDK_INSTALL_DIR.parent / "zlib-wasm-fix"
        if not zlib_dir.exists():
            print("\n--- Cloning patched zlib (emscripten-ports fork) ---")
            # emscripten-ports/zlib is the patched fork used by emscripten itself;
            # it guards the shared target with BUILD_SHARED_LIBS so ninja gets only
            # one rule for libz.a when cross-compiling.
            run("git", "clone", "https://github.com/emscripten-ports/zlib.git", str(zlib_dir))
        run(emcmake(), "cmake", ROOT,
            "-DCMAKE_BUILD_TYPE=Release",
            "-G", "Ninja",
            "-DBUILD_SHARED_LIBS=OFF",
            f"-DFETCHCONTENT_SOURCE_DIR_ZLIB={zlib_dir}",
            cwd=BUILD_DIR, use_em_env=True)
    else:
        print(f"  {BUILD_DIR} exists, skipping configure (use --clean to reconfigure)")

    print("\n--- build (emmake cmake --build) ---")
    run(emmake(), "cmake", "--build", ".", "--target", "FSLint-cli", cwd=BUILD_DIR, use_em_env=True)


def copy_artifacts():
    print("\n--- copy artifacts ---")
    WEB_PUBLIC.mkdir(parents=True, exist_ok=True)

    # Dictionary of original names to target names
    artifacts = {
        "FSLint-cli.js":   "FSLint-cli-wasm.js",
        "FSLint-cli.wasm": "FSLint-cli-wasm.wasm",
    }

    for src_name, dst_name in artifacts.items():
        src = BUILD_DIR / src_name
        if not src.exists():
            print(f"[ERROR] expected output not found: {src}")
            sys.exit(1)

        if src.exists():
            dst = WEB_PUBLIC / dst_name
            shutil.copy(src, dst)
            print(f"  {src.name} -> {dst}")

    # Copy WASM and JS files to public folder
    web_public_root = WEB_DIR / "public"
    web_public_root.mkdir(parents=True, exist_ok=True)
    
    for src_name, dst_name in artifacts.items():
        src = BUILD_DIR / src_name
        if src.exists():
            dst = web_public_root / dst_name
            shutil.copy(src, dst)
            print(f"  {src.name} -> {dst}")
    
    # Copy index.html to public folder
    index_src = WEB_DIR / "index.html"
    if index_src.exists():
        index_dst = web_public_root / "index.html"
        shutil.copy(index_src, index_dst)
        print(f"  {index_src.name} -> {index_dst}")
    else:
        print(f"[WARNING] index.html not found: {index_src}")
    
    # Copy favicon to public folder
    favicon_src = ROOT / "images" / "icon.svg"
    favicon_dst = web_public_root / "icon.svg"
    if favicon_src.exists():
        shutil.copy(favicon_src, favicon_dst)
        print(f"  {favicon_src.name} -> {favicon_dst}")
    else:
        print(f"[WARNING] favicon not found: {favicon_src}")

    # Copy banner to public folder
    banner_src = ROOT / "images" / "banner.svg"
    banner_dst = web_public_root / "banner.svg"
    if banner_src.exists():
        shutil.copy(banner_src, banner_dst)
        print(f"  {banner_src.name} -> {banner_dst}")
    else:
        print(f"[WARNING] banner not found: {banner_src}")

    # Copy rules to public folder
    rules_src = ROOT / "RULES.md"
    rules_dst = web_public_root / "rules.md"
    if rules_src.exists():
        shutil.copy(rules_src, rules_dst)
        print(f"  {rules_src.name} -> {rules_dst}")
    else:
        print(f"[WARNING] rules not found: {rules_src}")

    # Copy LICENSE to public folder
    license_src = ROOT / "LICENSE"
    license_dst = web_public_root / "LICENSE"
    if license_src.exists():
        shutil.copy(license_src, license_dst)
        print(f"  {license_src.name} -> {license_dst}")
    else:
        print(f"[WARNING] LICENSE not found: {license_src}")


def build_web():
    print("\n--- build web app ---")
    npm = "npm.cmd" if os.name == "nt" else "npm"
    run(npm, "install", cwd=WEB_DIR)
    run(npm, "run", "build", cwd=WEB_DIR)


# ── entry point ──────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--clean",    action="store_true", help="Wipe build-wasm before building")
    ap.add_argument("--skip-web", action="store_true", help="Skip the npm web build")
    args = ap.parse_args()

    global NINJA_PATH
    NINJA_PATH = find_ninja()

    find_or_install_emsdk()

    if args.clean:
        clean()

    build_wasm()
    copy_artifacts()

    if not args.skip_web:
        build_web()
        npm = "npm.cmd" if os.name == "nt" else "npm"
        print(f"\n[DONE]  cd web && {npm} run dev")
    else:
        print("\n[DONE]  WASM build complete.")



if __name__ == "__main__":
    main()