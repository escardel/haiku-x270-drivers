#!/bin/bash
# X270 only. Firmware bootstrap is bounded; server readiness gets one retry.
set -e
base=$(cd "$(dirname "$0")" && pwd)
if ! mkdir /tmp/x270-bluetooth-bootstrap.lock 2>/dev/null; then
    echo 'A Bluetooth bootstrap is already running.'
    exit 1
fi
trap 'rmdir /tmp/x270-bluetooth-bootstrap.lock' EXIT
# Force device enumeration before the raw bootstrap; no HCI receive is started.
ls /dev/bluetooth/h2/h2generic >/dev/null
usb_path=$(listusb | awk '$1 == "8087:0032" { print $2; exit }')
if [ -z "$usb_path" ]; then echo 'AX210 Bluetooth device not found.'; exit 1; fi
"$base/intel_firmware_loader" "$usb_path" "$base/ibt-0041-0041.sfi" "$base/ibt-0041-0041.ddc"
for attempt in 1 2; do
    stdbuf -oL -eL "$base/bluetooth_server-X270" &
    server_pid=$!
    sleep 2
    if "$base/bt_probe" --adapter-only; then
        echo "Bluetooth adapter ready (attempt $attempt)."
        exit 0
    fi
    echo "Bluetooth server readiness failed (attempt $attempt); stopping it."
    hey -o "$server_pid" quit || true
    for wait_count in 1 2 3; do
        kill -0 "$server_pid" 2>/dev/null || break
        sleep 1
    done
    kill -0 "$server_pid" 2>/dev/null && kill -TERM "$server_pid" || true
    wait "$server_pid" 2>/dev/null || true
done
echo 'Bluetooth adapter failed readiness checks; boot will continue.'
exit 1
