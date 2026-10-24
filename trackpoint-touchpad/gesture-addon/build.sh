#!/bin/bash
# SPDX-License-Identifier: MIT
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -s) == Haiku && $(uname -m) == x86_64 ]] || {
    echo 'Build on Haiku x86_64 with the development headers installed.'; exit 1;
}
flags=(-std=c++17 -DHAIKU_TARGET_PLATFORM_HAIKU -O2 -Wall
    -I/boot/system/develop/headers/private/input
    -I/boot/system/develop/headers/private/shared
    -I/boot/system/develop/headers/private)
g++ "${flags[@]}" -shared -fPIC MouseInputDevice.cpp movement_maker.cpp -lbe -o mouse
g++ "${flags[@]}" gesture_test.cpp movement_maker.cpp -lbe -o gesture_test
./gesture_test
echo 'Built mouse input add-on. Use the component installer to deploy it.'
