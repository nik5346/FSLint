#!/usr/bin/env python3
import os
import sys
import subprocess

def check_file(filepath):
    if not os.path.isfile(filepath):
        return True

    # Skip empty files
    if os.path.getsize(filepath) == 0:
        return True

    # Try to decode as UTF-8
    try:
        with open(filepath, 'rb') as f:
            content = f.read()
        content.decode('utf-8')
    except UnicodeDecodeError:
        print(f"Error: {filepath} is not valid UTF-8.")
        return False

    # Check for binary signature using 'file' command
    try:
        # Use -b to be brief, -i for mime
        result = subprocess.run(['file', '-b', '-i', filepath], capture_output=True, text=True, check=True)
        if 'charset=binary' in result.stdout:
            print(f"Error: {filepath} appears to be a binary file.")
            return False
    except (subprocess.CalledProcessError, FileNotFoundError):
        # Fallback if 'file' is not available
        if b'\x00' in content:
            print(f"Error: {filepath} contains null bytes, suggesting it is a binary file.")
            return False

    return True

def main():
    try:
        # Use git ls-files to get tracked files
        files = subprocess.check_output(['git', 'ls-files'], text=True).splitlines()
    except (subprocess.CalledProcessError, FileNotFoundError):
        # Fallback to finding all files if not a git repo or git not found
        files = []
        for root, _, filenames in os.walk('.'):
            if '.git' in root:
                continue
            for filename in filenames:
                files.append(os.path.relpath(os.path.join(root, filename), '.'))

    failed_files = []
    for file in files:
        if not check_file(file):
            failed_files.append(file)

    if failed_files:
        print(f"\nEncoding check failed for {len(failed_files)} file(s).")
        sys.exit(1)
    else:
        print("All files passed encoding and binary check.")

if __name__ == "__main__":
    main()
