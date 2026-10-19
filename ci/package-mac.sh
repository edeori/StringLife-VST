#!/usr/bin/env bash
set -euo pipefail
bash ./package.sh licensed
cmake --build build/Release/licensed --config Release --target MothFingerprintTests -j 3
ctest --test-dir build/Release/licensed -C Release -R machine-fingerprint --output-on-failure
version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' build/Release/licensed/CMakeCache.txt)"
mkdir -p dist
cp "build/Release/licensed/StringLife-${version}.pkg" dist/installer.pkg
pkgutil --check-signature dist/installer.pkg
xcrun stapler validate dist/installer.pkg
