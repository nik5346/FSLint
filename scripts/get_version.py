#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

def get_git_commit_count():
    try:
        # Get total number of commits in the current branch
        result = subprocess.run(['git', 'rev-list', '--count', 'HEAD'],
                               capture_output=True, text=True, check=True)
        return int(result.stdout.strip())
    except (subprocess.CalledProcessError, FileNotFoundError, ValueError):
        return 0

def get_version():
    root = Path(__file__).resolve().parent.parent
    version_file = root / "VERSION"

    if not version_file.exists():
        base_version = "0.0"
    else:
        with open(version_file, "r") as f:
            base_version = f.read().strip()

    patch = get_git_commit_count()

    # Ensure base_version has exactly major.minor
    parts = base_version.split('.')
    if len(parts) < 2:
        major, minor = parts[0], "0"
    else:
        major, minor = parts[0], parts[1]

    return f"{major}.{minor}.{patch}"

if __name__ == "__main__":
    print(get_version())
