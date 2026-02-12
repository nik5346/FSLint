#!/bin/bash
set -e

# Directory to store reference FMUs
OUT_DIR="tests/reference_fmus"
mkdir -p "$OUT_DIR"

VERSION="0.0.39"
URL="https://github.com/modelica/Reference-FMUs/releases/download/v${VERSION}/Reference-FMUs-${VERSION}.zip"
ZIP_FILE="$OUT_DIR/Reference-FMUs.zip"

if [ ! -f "$ZIP_FILE" ]; then
    echo "Downloading Reference FMUs v${VERSION}..."
    curl -L -o "$ZIP_FILE" "$URL"
else
    echo "Reference FMUs already downloaded."
fi

# Extract a few specific FMUs for testing if not already extracted
# We'll extract them into subdirectories
extract_fmu() {
    local fmu_in_zip=$1
    local target_dir=$2
    if [ ! -d "$target_dir" ]; then
        echo "Extracting $fmu_in_zip..."
        mkdir -p "$target_dir"
        unzip -j "$ZIP_FILE" "$fmu_in_zip" -d "$target_dir"
        # Also unzip the FMU itself because our checkers often work on directory models
        cd "$target_dir"
        local fmu_file=$(basename "$fmu_in_zip")
        unzip "$fmu_file"
        rm "$fmu_file"
        cd - > /dev/null
    fi
}

extract_fmu "2.0/BouncingBall.fmu" "$OUT_DIR/fmi2/BouncingBall"
extract_fmu "3.0/BouncingBall.fmu" "$OUT_DIR/fmi3/BouncingBall"

echo "Done."
