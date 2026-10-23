#!/bin/bash
# X270 only: bootstrap signed volatile AX210 firmware before starting the server.
set -e
base=/boot/home/x270-camera-mic/bluetooth
if ! mkdir /tmp/x270-bluetooth-bootstrap.lock 2>/dev/null; then
    echo 'A Bluetooth bootstrap is already running.'
    exit 1
fi
trap 'rmdir /tmp/x270-bluetooth-bootstrap.lock' EXIT
usb_path=$(listusb | awk '$1 == "8087:0032" { print $2; exit }')
if [ -z "$usb_path" ]; then
    echo 'AX210 Bluetooth device not found.'
    exit 1
fi
"$base/intel_firmware_loader" "$usb_path" "$base/ibt-0041-0041.sfi" "$base/ibt-0041-0041.ddc"
rmdir /tmp/x270-bluetooth-bootstrap.lock
trap - EXIT
exec "$base/bluetooth_server-X270"
