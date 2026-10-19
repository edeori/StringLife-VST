#!/usr/bin/env bash
set -euo pipefail
bash ./package.sh licensed
version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' build/Release/licensed/CMakeCache.txt)"
mkdir -p dist
cp "build/Release/licensed/StringLife-${version}.pkg" dist/installer.pkg
pkgutil --check-signature dist/installer.pkg
xcrun stapler validate dist/installer.pkg
