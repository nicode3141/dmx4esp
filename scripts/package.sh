#!/usr/bin/env bash

set -euo pipefail

APP_NAME="dmx4esp"
PACKAGE_DIR="package/apps/$APP_NAME"

# review all files
for file in src/dmx4esp.c src/dmx4esp.h src/CMakeLists.txt; do
    if [ ! -f "$file" ]; then
        echo "Error: Missing file '$file'" >&2
        exit 1
    fi
done

rm -rf package
mkdir -p "$PACKAGE_DIR"

cp src/dmx4esp.c "$PACKAGE_DIR/"
cp src/dmx4esp.h "$PACKAGE_DIR/"
cp src/CMakeLists.txt "$PACKAGE_DIR/"

(
    cd package
    zip -r "../${APP_NAME}.zip" apps
)