#!/bin/bash
set -e

VERSION="0.0.39"
URL="https://github.com/modelica/Reference-FMUs/releases/download/v${VERSION}/Reference-FMUs-${VERSION}.zip"
DEST="tests/reference_fmus"

echo "Downloading Reference FMUs v${VERSION}..."
mkdir -p "$DEST"
curl -L "$URL" -o "$DEST/Reference-FMUs.zip"

echo "Extracting Reference-FMUs.zip..."
unzip -q -o "$DEST/Reference-FMUs.zip" -d "$DEST"
rm "$DEST/Reference-FMUs.zip"

echo "Extracting BouncingBall 2.0..."
mkdir -p "$DEST/BouncingBall_20"
unzip -q -o "$DEST/2.0/BouncingBall.fmu" -d "$DEST/BouncingBall_20"

echo "Extracting BouncingBall 3.0..."
mkdir -p "$DEST/BouncingBall_30"
unzip -q -o "$DEST/3.0/BouncingBall.fmu" -d "$DEST/BouncingBall_30"

echo "Reference FMUs downloaded and extracted to $DEST"
