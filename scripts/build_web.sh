#!/bin/bash
set -e

# Build WASM
echo "Building WASM..."
mkdir -p build-wasm
cd build-wasm
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake cmake --build . --target FSLint-cli
cd ..

# Copy WASM artifacts to web public directory
echo "Copying artifacts..."
mkdir -p web/public
cp build-wasm/FSLint-cli.js web/public/FSLint-cli-wasm.js
cp build-wasm/FSLint-cli.wasm web/public/

# Build Web App
echo "Building Web App..."
cd web
npm install
npm run build
cd ..

echo "Web build complete. To run locally:"
echo "cd web && npm run dev"
