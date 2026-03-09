#!/usr/bin/env python3
import os
import subprocess
import shutil
import sys
from pathlib import Path

def find_tool(cmd):
    """Finds a tool in PATH or common Emscripten locations."""
    resolved = shutil.which(cmd)
    if resolved:
        return resolved

    if os.name == 'nt':
        for ext in ['.bat', '.cmd', '.exe']:
            resolved = shutil.which(cmd + ext)
            if resolved:
                return resolved

    # Fallback for Emscripten tools
    if cmd in ["emcmake", "emmake"]:
        for env_var in ["EMSCRIPTEN", "EMSDK"]:
            root = os.environ.get(env_var)
            if not root:
                continue

            root_path = Path(root)
            # Potential subdirectories where Emscripten tools might reside
            potential_dirs = [
                root_path,
                root_path / "upstream" / "emscripten",
                root_path / "bin",
            ]

            for d in potential_dirs:
                if not d.exists():
                    continue

                cmd_path = d / cmd
                if os.name == 'nt':
                    for ext in ['.bat', '.cmd', '.exe']:
                        p = d / (cmd + ext)
                        if p.exists():
                            return str(p)
                elif cmd_path.exists():
                    return str(cmd_path)

    return None

def activate_emsdk():
    """Attempts to activate the Emscripten environment."""
    print("--- Activating Emscripten Environment ---")
    emsdk_roots = []
    if os.environ.get("EMSDK"):
        emsdk_roots.append(Path(os.environ.get("EMSDK")))
    if os.environ.get("EMSCRIPTEN"):
        emsdk_roots.append(Path(os.environ.get("EMSCRIPTEN")))

    if not emsdk_roots:
        print("Neither EMSDK nor EMSCRIPTEN environment variables are set.")
        return False

    # Expand search paths to include parent and grandparent directories
    search_paths = set()
    for root in emsdk_roots:
        search_paths.add(root)
        search_paths.add(root.parent)
        search_paths.add(root.parent.parent)

    env_script = None
    for p in search_paths:
        if not p.exists():
            continue
        if os.name == 'nt':
            script = p / "emsdk_env.bat"
        else:
            script = p / "emsdk_env.sh"

        if script.exists():
            env_script = script
            break

    if not env_script:
        return False

    print(f"Attempting to activate Emscripten environment via {env_script}...")
    try:
        if os.name == 'nt':
            # Run the batch file and then 'set' to get all environment variables
            # We use 'call' and don't use 'check=True' because emsdk_env.bat might
            # return non-zero exit codes even if it successfully sets variables.
            command = f'call "{env_script}" && set'
            result = subprocess.run(command, capture_output=True, text=True, shell=True, cwd=env_script.parent)

            # If it failed but still produced output, we try to parse it anyway
            output = result.stdout
            if result.returncode != 0:
                print(f"Warning: Activation script returned non-zero exit code {result.returncode}")
                if result.stderr:
                    print(f"Stderr: {result.stderr.strip()}")

            count = 0
            for line in output.splitlines():
                if '=' in line:
                    # Basic validation that it looks like an environment variable
                    if line.startswith(('PATH=', 'EMSCRIPTEN', 'EMSDK', 'BINARYEN')):
                        key, value = line.split('=', 1)
                        os.environ[key] = value
                        count += 1

            if count > 0:
                print(f"Emscripten environment activated ({count} variables updated).")
                return True
        else:
            # Run the shell script and then 'env' to get all environment variables
            command = f'source "{env_script}" && env'
            result = subprocess.run(['/bin/bash', '-c', command], capture_output=True, text=True, cwd=env_script.parent)

            for line in result.stdout.splitlines():
                if '=' in line:
                    key, value = line.split('=', 1)
                    os.environ[key] = value

            print("Emscripten environment activated successfully.")
            return True

        return False
    except Exception as e:
        print(f"Failed to activate Emscripten environment: {e}")
        return False

def check_prerequisites():
    print("--- Checking Prerequisites ---")
    prereqs = ["cmake", "npm", "emcmake", "emmake"]
    missing = []

    for cmd in prereqs:
        if not find_tool(cmd):
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
    resolved_cmd = find_tool(cmd_name)

    if resolved_cmd:
        command[0] = resolved_cmd

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

    # 0. Activate Emscripten environment
    activate_emsdk()

    # 1. Check Prerequisites
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
