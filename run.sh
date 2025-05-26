#!/bin/bash
set -euo pipefail

GREEN='\033[0;32m'
NC='\033[0m'

# Delete zia_genesis_output.txt if it exists and --regen-genesis is passed
if [[ "$*" == *"--regen-genesis"* ]]; then
    if [ -f "zia_genesis_output.txt" ]; then
        rm zia_genesis_output.txt
        echo "Deleted existing zia_genesis_output.txt"
    fi
    echo -e "🧬 Regenerating genesis block (DEV MODE)..."
    python3 genesis.py
else
    echo -e "🧬 Using existing genesis block parameters."
fi

echo -e "🧹 Cleaning old build files..."
rm -rf build

echo -e "⚙️ Configuring build with CMake..."
cmake -B build \
    -DBUILD_GUI=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON

echo -e "🚧 Building project..."
cmake --build build -j$(sysctl -n hw.ncpu)

echo -e "📋 Copying word.csv to build/bin..."
cp word.csv build/bin/

echo -e "${GREEN}Setup complete!${NC}"
echo -e "To run the CLI: ${GREEN}./build/bin/ziacoin-cli${NC}"
echo -e "To run the GUI: ${GREEN}./build/bin/ziacoin-qt${NC}"

