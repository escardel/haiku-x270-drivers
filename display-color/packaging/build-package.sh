#!/bin/bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")/.." && pwd)
if [ -f "$source_dir/packaging/UNSAFE-RGB-SATURATION.md" ]; then
    echo 'Build blocked: a reported blackout is under investigation. See packaging/UNSAFE-RGB-SATURATION.md.' >&2
    exit 1
fi
output_dir="$source_dir/dist"
mkdir -p "$output_dir"
staging=$(mktemp -d "$output_dir/staging.XXXXXX")
trap 'rm -rf "$staging"' EXIT
make -C "$source_dir" all check check-safety
mkdir -p "$staging/apps/DisplayColor" \
    "$staging/data/deskbar/menu/Applications" \
    "$staging/documentation/packages/displaycolor" \
    "$staging/data/licenses"
cp -p "$source_dir/DisplayColor" "$staging/apps/DisplayColor/Display Color"
cp -p "$source_dir/DisplayColorStartup" "$staging/apps/DisplayColor/DisplayColorStartup"
cp -p "$source_dir/SafeDisplayTest" "$staging/apps/DisplayColor/SafeDisplayTest"
cp "$source_dir/packaging/DisplayColor.PackageInfo" "$staging/.PackageInfo"
cp "$source_dir/packaging/DisplayColor-License-Notice" "$staging/data/licenses/"
cp "$source_dir/README.md" "$source_dir/VALIDATION.md" \
    "$source_dir/SAFE-TESTING.md" "$source_dir/packaging/SAFETY-HISTORY.md" \
    "$staging/documentation/packages/displaycolor/"
cp "$source_dir/packaging/README.md" \
    "$staging/documentation/packages/displaycolor/INSTALL.md"
ln -s '../../../../apps/DisplayColor/Display Color' \
    "$staging/data/deskbar/menu/Applications/Display Color"
package create -C "$staging" "$output_dir/displaycolor-1.1.1-1-x86_64.hpkg"
bundle_name=displaycolor-1.1.1-any-haiku-revision-x86_64
bundle_dir="$staging/$bundle_name"
mkdir -p "$bundle_dir/source/packaging" "$bundle_dir/source/tests"
cp "$output_dir/displaycolor-1.1.1-1-x86_64.hpkg" "$bundle_dir/"
cp "$source_dir/packaging/README.md" "$bundle_dir/README.md"
cp "$source_dir/DisplayColor.cpp" "$source_dir/Startup.cpp" \
    "$source_dir/SafeDisplayTest.cpp" "$source_dir/SafeDisplayTest.rdef" \
    "$source_dir/DisplayColor.rdef" "$source_dir/Startup.rdef" \
    "$source_dir/Makefile" "$source_dir/README.md" \
    "$source_dir/SAFE-TESTING.md" \
    "$source_dir/VALIDATION.md" "$bundle_dir/source/"
cp "$source_dir/packaging/DisplayColor.PackageInfo" \
    "$source_dir/packaging/DisplayColor-License-Notice" \
    "$source_dir/packaging/SAFETY-HISTORY.md" \
    "$source_dir/packaging/build-package.sh" "$source_dir/packaging/README.md" \
    "$bundle_dir/source/packaging/"
cp "$source_dir/tests/HardwareTests.cpp" "$source_dir/tests/PreviewTests.cpp" \
    "$bundle_dir/source/tests/"
(cd "$bundle_dir" && sha256sum displaycolor-1.1.1-1-x86_64.hpkg > SHA256SUMS)
rm -f "$output_dir/$bundle_name.zip"
(cd "$staging" && zip -qr "$output_dir/$bundle_name.zip" "$bundle_name")
(cd "$output_dir" && sha256sum displaycolor-1.1.1-1-x86_64.hpkg \
    "$bundle_name.zip" > DisplayColor-SHA256SUMS)
