#!/bin/bash
set -euo pipefail

GREEN='\033[0;32m'
NC='\033[0m'

OLD_NAME="Bitcoin"
NEW_NAME="ZiaCoin"
OLD_BIN="bitcoin"
NEW_BIN="ziacoin"
OLD_TICKER="BTC"
NEW_TICKER="ZIA"

echo -e "🔁 ${GREEN}1. Replacing identifiers inside source files and renaming files (locale-safe, UTF-8 only)...${NC}"

export LC_CTYPE=C

# Step 1: Rename source/header files with bitcoin prefix to ziacoin prefix
find src -type f \( -name "bitcoin*.cpp" -o -name "bitcoin*.h" \) | while read -r filepath; do
  dir=$(dirname "$filepath")
  base=$(basename "$filepath")
  newbase=$(echo "$base" | sed "s/^bitcoin/${NEW_BIN}/")
  mv "$filepath" "$dir/$newbase"
  echo "Renamed $base → $newbase"
done

# Step 2: Replace internal text references in source and config files
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.txt" -o -name "*.md" -o -name "*.py" -o -name "*.json" \) | while read -r file; do
  sed -i '' "s/${OLD_NAME}/${NEW_NAME}/g" "$file" || true
  sed -i '' "s/${OLD_BIN}/${NEW_BIN}/g" "$file" || true
  sed -i '' "s/${OLD_TICKER}/${NEW_TICKER}/g" "$file" || true
  sed -i '' "s/ziacoin-config.h/ziacoin-config.h/g" "$file" || true
done

echo -e "🔄 Renaming build config template file..."
if [[ -f cmake/ziacoin-build-config.h.in ]]; then
  mv cmake/ziacoin-build-config.h.in cmake/ziacoin-build-config.h.in
  echo -e "✅ Renamed cmake/ziacoin-build-config.h.in → cmake/ziacoin-build-config.h.in"
else
  echo "⚠️ cmake/ziacoin-build-config.h.in not found!"
fi

echo -e "🎨 Looking for zia.svg icon..."
if [[ -f ../zia.svg ]]; then
  cp ../zia.svg src/qt/ziacoin.svg
  echo "✅ Icon added."
else
  echo "⚠️ zia.svg icon not found in parent directory."
fi

echo -e "🧹 Cleaning old build files..."
rm -rf build

echo -e "🧬 Running genesis.py script..."
python3 genesis.py

echo -e "⚙️ Configuring build with CMake..."
cmake -B build -DBUILD_GUI=ON

echo -e "🚧 Building project..."
cmake --build build

echo -e "${GREEN}Setup complete!${NC}"