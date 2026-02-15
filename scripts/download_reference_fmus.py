#!/usr/bin/env python3
import urllib.request
import zipfile
import shutil
from pathlib import Path

VERSION = "0.0.39"
URL = f"https://github.com/modelica/Reference-FMUs/releases/download/v{VERSION}/Reference-FMUs-{VERSION}.zip"
DEST = Path("tests/reference_fmus")
ZIP_PATH = DEST / "Reference-FMUs.zip"

print(f"Downloading Reference FMUs v{VERSION}...")
DEST.mkdir(parents=True, exist_ok=True)

with urllib.request.urlopen(URL) as response:
    with open(ZIP_PATH, "wb") as f:
        shutil.copyfileobj(response, f)

print("Extracting Reference-FMUs.zip...")
with zipfile.ZipFile(ZIP_PATH, "r") as zip_ref:
    zip_ref.extractall(DEST)

ZIP_PATH.unlink()
print(f"Reference FMUs downloaded and extracted to {DEST}")