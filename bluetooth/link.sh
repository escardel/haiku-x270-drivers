#!/bin/bash
set -e
cd /boot/home/haiku
objects=generated/objects/haiku/x86_64/release
out=/boot/home/x270-camera-mic/bluetooth
c++ -shared -Wl,-soname,libbluetooth.so -o "$out/libbluetooth-X270.so" "$objects"/kits/bluetooth/*.o -L"$objects/kits/locale" -lbe -llocalestub -lstdc++
c++ -o "$out/bluetooth_server-X270" "$objects"/servers/bluetooth/*.o "$out/libbluetooth-X270.so" -L"$objects/kits/locale" -lbe -lnetwork -llocalestub -lstdc++
