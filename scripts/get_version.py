#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

def get_git_commit_count_since_version_change():
    try:
        # Find the last commit that modified the VERSION file
        result = subprocess.run(['git', 'log', '-1', '--format=%H', '--', 'VERSION'],
                               capture_output=True, text=True, check=True)
        last_version_commit = result.stdout.strip()

        if not last_version_commit:
            # If VERSION has never been committed, count all commits
            result = subprocess.run(['git', 'rev-list', '--count', 'HEAD'],
                                   capture_output=True, text=True, check=True)
            return int(result.stdout.strip())

        # Count commits from last_version_commit to HEAD (exclusive of last_version_commit)
        result = subprocess.run(['git', 'rev-list', '--count', f'{last_version_commit}..HEAD'],
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

    patch = get_git_commit_count_since_version_change()

    # Ensure base_version has exactly major.minor
    parts = base_version.split('.')
    if len(parts) < 2:
        major, minor = parts[0], "0"
    else:
        major, minor = parts[0], parts[1]

    return f"{major}.{minor}.{patch}"

if __name__ == "__main__":
    print(get_version())
