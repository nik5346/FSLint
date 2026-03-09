#!/usr/bin/env python3
import os
import subprocess
import shutil
import sys
from pathlib import Path

def run_command(command, cwd=None):
    print(f"Running: {' '.join(command)} in {cwd or os.getcwd()}")
    process = subprocess.run(command, cwd=cwd, shell=True if os.name == 'nt' else False)
    if process.returncode != 0:
        print(f"Command failed with return code {process.returncode}")
        sys.exit(process.returncode)

def main():
    # Get the workspace root (parent of the scripts directory)
    workspace_root = Path(__file__).parent.parent.resolve()
    build_wasm_dir = workspace_root / "build-wasm"
    web_dir = workspace_root / "web"
    web_public_dir = web_dir / "public"

    # 1. Build WASM
    print("--- Building WASM ---")
    if not build_wasm_dir.exists():
        build_wasm_dir.mkdir(parents=True)

    # emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
    run_command(["emcmake", "cmake", "..", "-DCMAKE_BUILD_TYPE=Release"], cwd=build_wasm_dir)

    # emmake cmake --build . --target FSLint-cli
    run_command(["emmake", "cmake", "--build", ".", "--target", "FSLint-cli"], cwd=build_wasm_dir)

    # 2. Copy WASM artifacts to web public directory
    print("--- Copying artifacts ---")
    if not web_public_dir.exists():
        web_public_dir.mkdir(parents=True)

    wasm_js = build_wasm_dir / "FSLint-cli.js"
    wasm_binary = build_wasm_dir / "FSLint-cli.wasm"

    if wasm_js.exists():
        shutil.copy(wasm_js, web_public_dir / "FSLint-cli-wasm.js")
        print(f"Copied {wasm_js} to {web_public_dir / 'FSLint-cli-wasm.js'}")
    else:
        print(f"Error: {wasm_js} not found!")
        sys.exit(1)

    if wasm_binary.exists():
        shutil.copy(wasm_binary, web_public_dir / "FSLint-cli.wasm")
        print(f"Copied {wasm_binary} to {web_public_dir / 'FSLint-cli.wasm'}")
    else:
        print(f"Error: {wasm_binary} not found!")
        sys.exit(1)

    # 3. Build Web App
    print("--- Building Web App ---")
    # Use 'npm.cmd' on Windows
    npm_cmd = "npm.cmd" if os.name == "nt" else "npm"

    run_command([npm_cmd, "install"], cwd=web_dir)
    run_command([npm_cmd, "run", "build"], cwd=web_dir)

    print("\nWeb build complete. To run locally:")
    print(f"cd web && {npm_cmd} run dev")

if __name__ == "__main__":
    main()
