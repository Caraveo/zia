#!/bin/bash

# Exclude .git directory from grep
grep -rl --exclude-dir=".git" 'ziacoin-build-config.h.in' . | while read -r file; do
  echo "Patching $file"
  sed -i '' 's/ziacoin-build-config.h.in/ziacoin-build-config.h.in/g' "$file"
done

grep -rl --exclude-dir=".git" 'ziacoin-config.h' . | while read -r file; do
  echo "Patching $file"
  sed -i '' 's/ziacoin-config.h/ziacoin-config.h/g' "$file"
done

echo "Done patching all references."