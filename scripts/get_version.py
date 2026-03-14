#!/usr/bin/env python3
from pathlib import Path

def get_version():
    root = Path(__file__).resolve().parent.parent
    version_file = root / "VERSION"

    if not version_file.exists():
        return "0.1.0"

    with open(version_file, "r") as f:
        return f.read().strip()

if __name__ == "__main__":
    print(get_version())
