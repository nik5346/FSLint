#!/usr/bin/env python3
import subprocess
import sys
import os
from pathlib import Path

def run_command(command, cwd=None, name=""):
    print(f"--- Running {name}: {' '.join(command)} ---")
    try:
        subprocess.run(command, cwd=cwd, check=True)
        print(f"SUCCESS: {name}\n")
        return True
    except subprocess.CalledProcessError as e:
        print(f"ERROR: {name} failed with exit code {e.returncode}\n")
        return False
    except FileNotFoundError:
        print(f"ERROR: {command[0]} not found.\n")
        return False

def main():
    root_dir = Path(__file__).parent.parent.absolute()
    web_dir = root_dir / "web"
    success = True

    # 1. C++ Verification
    print("=== C++ Verification ===")
    if not run_command(["cmake", "--build", "build", "--target", "clang-format"], root_dir, "C++ Format"):
        success = False

    # We allow clang-tidy to fail if it's not installed or crashes, but try to run it
    run_command(["cmake", "--build", "build", "--target", "clang-tidy"], root_dir, "C++ Tidy (Optional)")

    if not run_command(["cmake", "--build", "build", "--target", "doxygen-check"], root_dir, "C++ Doxygen"):
        success = False

    if not run_command(["ctest", "--output-on-failure", "-C", "Release"], root_dir / "build", "C++ Tests"):
        success = False

    # 2. Web Verification
    print("=== Web Verification ===")
    if web_dir.exists():
        if not run_command(["npm", "run", "format"], web_dir, "Web Format"):
            success = False
        if not run_command(["npm", "run", "lint"], web_dir, "Web Lint"):
            success = False
        if not run_command(["npm", "run", "check-types"], web_dir, "Web Typecheck"):
            success = False
        if not run_command(["npm", "run", "build"], web_dir, "Web Build"):
            success = False
    else:
        print("Skipping Web verification (web/ directory not found)")

    if success:
        print("ALL MANDATORY STEPS PASSED")
        sys.exit(0)
    else:
        print("SOME MANDATORY STEPS FAILED")
        sys.exit(1)

if __name__ == "__main__":
    main()
