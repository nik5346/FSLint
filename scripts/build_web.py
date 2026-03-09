#!/usr/bin/env python3
import os
import subprocess
import shutil
import sys
from pathlib import Path

def check_prerequisites():
    print("--- Checking Prerequisites ---")
    prereqs = ["cmake", "npm", "emcmake", "emmake"]
    missing = []

    for cmd in prereqs:
        resolved = shutil.which(cmd)
        if not resolved and os.name == 'nt':
            for ext in ['.bat', '.cmd', '.exe']:
                if shutil.which(cmd + ext):
                    resolved = True
                    break

        if not resolved:
            missing.append(cmd)

    if missing:
        print(f"Error: Missing prerequisites: {', '.join(missing)}")
        print("\nTo build the web interface, you must have Emscripten and Node.js installed.")
        print("See https://emscripten.org/docs/getting_started/downloads.html for Emscripten.")
        print("See https://nodejs.org/ for Node.js.")
        if os.name == 'nt':
            print("\nNote for Windows: Ensure you have activated the Emscripten environment in your terminal")
            print("(e.g., by running 'emsdk_env.bat' from your Emscripten installation).")
        sys.exit(1)
    print("All prerequisites found.\n")

def run_command(command, cwd=None):
    cmd_name = command[0]

    # Try to resolve the executable path
    resolved_cmd = shutil.which(cmd_name)
    if resolved_cmd:
        command[0] = resolved_cmd
    elif os.name == 'nt':
        # On Windows, try common extensions if not found
        for ext in ['.bat', '.cmd', '.exe']:
            resolved_cmd = shutil.which(cmd_name + ext)
            if resolved_cmd:
                command[0] = resolved_cmd
                break

    if os.name == 'nt':
        # On Windows, use list2cmdline and shell=True for better compatibility with batch files and PATH
        cmd_str = subprocess.list2cmdline(command)
        print(f"Running: {cmd_str} in {cwd or os.getcwd()}")
        process = subprocess.run(cmd_str, cwd=cwd, shell=True)
    else:
        print(f"Running: {' '.join(command)} in {cwd or os.getcwd()}")
        process = subprocess.run(command, cwd=cwd, shell=False)

    if process.returncode != 0:
        print(f"Command failed with return code {process.returncode}")
        if cmd_name in ["emcmake", "emmake", "npm"]:
            print(f"Error: '{cmd_name}' not found or failed. Please ensure it is installed and in your PATH.")
        sys.exit(process.returncode)

def main():
    # Get the workspace root (parent of the scripts directory)
    workspace_root = Path(__file__).parent.parent.resolve()

    # 0. Check Prerequisites
    check_prerequisites()

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
    run_command(["npm", "install"], cwd=web_dir)
    run_command(["npm", "run", "build"], cwd=web_dir)

    print("\nWeb build complete. To run locally:")
    npm_info = "npm.cmd" if os.name == "nt" else "npm"
    print(f"cd web && {npm_info} run dev")

if __name__ == "__main__":
    main()
