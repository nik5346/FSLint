#!/usr/bin/env python3
import subprocess
import re
import argparse
from pathlib import Path

# The fallback version if no git tags are found
DEFAULT_VERSION = "0.1.0"

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

def get_version():
    # Prioritize Git tag if it is a valid numeric version
    version = get_git_version()
    if not version:
        version = DEFAULT_VERSION

    # Final validation: Ensure it's a valid CMake version string (numeric)
    if not re.match(r'^\d+(\.\d+){0,3}$', version):
        return DEFAULT_VERSION

    return version

if __name__ == "__main__":
    print(get_version())
