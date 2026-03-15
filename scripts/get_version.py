#!/usr/bin/env python3
import subprocess
import json
import sys
import argparse
import re
from pathlib import Path

def get_git_version():
    try:
        # Try to get the version from the nearest git tag
        # Use --abbrev=0 to get just the tag name, avoiding commit hashes
        result = subprocess.run(['git', 'describe', '--tags', '--abbrev=0', '--always'],
                               capture_output=True, text=True, check=True)
        tag = result.stdout.strip().lstrip('v')

        # Validate that the tag is a valid CMake version (Major.Minor.Patch)
        if re.match(r'^\d+(\.\d+){0,3}$', tag):
            return tag
        return None
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None

def get_file_version():
    root = Path(__file__).resolve().parent.parent
    version_file = root / "VERSION"
    if version_file.exists():
        with open(version_file, "r") as f:
            return f.read().strip()
    return "0.1.0"

def get_version():
    # Prioritize Git tag if it is a valid numeric version
    version = get_git_version()
    if not version:
        # Fallback to the VERSION file
        version = get_file_version()

    # Final validation: Ensure it's a valid CMake version string (numeric)
    # If even the file is invalid, fallback to a safe default
    if not re.match(r'^\d+(\.\d+){0,3}$', version):
        return "0.1.0"

    return version

def sync_version():
    version = get_version()
    print(f"Synchronizing version: {version}")

    root = Path(__file__).resolve().parent.parent
    web_dir = root / "web"

    # Update web/package.json
    package_json_path = web_dir / "package.json"
    if package_json_path.exists():
        with open(package_json_path, "r") as f:
            data = json.load(f)
        data["version"] = version
        with open(package_json_path, "w") as f:
            json.dump(data, f, indent=2)
            f.write("\n")
        print(f"  Updated {package_json_path}")

    # Update web/package-lock.json if it exists
    package_lock_path = web_dir / "package-lock.json"
    if package_lock_path.exists():
        with open(package_lock_path, "r") as f:
            data = json.load(f)
        if "version" in data:
            data["version"] = version
        if "packages" in data and "" in data["packages"]:
            data["packages"][""]["version"] = version
        with open(package_lock_path, "w") as f:
            json.dump(data, f, indent=2)
            f.write("\n")
        print(f"  Updated {package_lock_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Get or synchronize project version")
    parser.add_argument("--sync", action="store_true", help="Synchronize version across files")
    args = parser.parse_args()

    if args.sync:
        sync_version()
    else:
        print(get_version())
