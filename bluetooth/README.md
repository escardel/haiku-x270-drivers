# Bluetooth on the X270

Hardware: Intel AX210 USB `8087:0032`. Tested Haiku: R1 beta 6, `hrev59866+92`, x86_64.

## What changed

The packaged generic USB driver already enumerates this adapter. Its controller starts in bootloader mode, where standard HCI commands fail. `intel_firmware_loader.cpp` uses USB Kit to load Intel's signed `ibt-0041-0041.sfi`, boot the operational image, apply DDC configuration, and perform an HCI reset. It accepts only the hardware revision actually tested (CNVI/CNVR `00400410`, Bluetooth `00173700`, ECDSA bootloader). The firmware is volatile; this does not flash the adapter.

Haiku's Bluetooth Kit then hangs while asking this controller for unsupported stored link keys. The patch in `patches/` gates that optional request on supported-commands octet 6, bit 5. It also supplies the required parameters for controllers supporting the request. Both patched library and server are required for the capability reply. With an old server, the new library skips the optional request.

No Bluetooth kernel driver changes were needed. The existing generic USB transport and Bluetooth server perform inquiry once firmware and kit initialization are fixed.

## Build

On the target:

```sh
c++ -std=c++17 intel_firmware_loader.cpp -o intel_firmware_loader -ldevice -lbe
c++ -I/boot/system/develop/headers/private/bluetooth bt_probe.cpp -o bt_probe -lbluetooth -lbe
```

Apply the Haiku patch to the documented source baseline, then build `libbluetooth.so bluetooth_server`. The normal jam build compiled all Bluetooth objects but failed on unrelated ICU/zlib/zstd dependency headers. `link.sh` links those objects using installed system libraries and the generated locale stub. The Haiku source commit is `99ccfe1a45`, based on the existing X270 source checkpoint `b7ed43bcd2`.

## Startup and recovery

The target-specific `start-bluetooth.sh` locates the AX210 USB path, runs the bootstrap, then launches the patched server. It uses a lock directory and the loader has a 120-second watchdog. The loader refuses to run while bluetooth_server is registered. Stop the server before manual raw USB tests. Do not use concurrent USB Kit endpoint readers: Haiku usb_raw holds one device lock across each transfer.

Installed library: `/boot/system/non-packaged/lib/libbluetooth.so`. The server, loader, firmware, and startup helper reside in `/boot/home/x270-camera-mic/bluetooth`. The background UserBootscript entry redirects output to that directory's `boot.log`. The prior UserBootscript is saved at `/boot/home/x270-camera-mic/backups/UserBootscript-before-bluetooth`; the packaged library remains available, with a copy at `backups/libbluetooth-stock`.

To disable the experiment, remove only the marked X270 Bluetooth command from UserBootscript, quit the patched Bluetooth server, remove the non-packaged libbluetooth override, and reboot. Leave the separate Wi-Fi and other existing boot commands intact. If an interrupted bootstrap leaves `/tmp/x270-bluetooth-bootstrap.lock`, ensure no loader is running before removing that empty directory.

## Evidence and limits

The operational firmware reports image type 3; Read Local Version, DDC writes, and HCI reset succeed. A bounded Bluetooth Kit inquiry found three nearby devices and completed successfully. The probe reports counts without publishing nearby device identifiers. Pairing, HID, BLE, file transfer, and Bluetooth audio have not been validated. Discovery alone does not establish profile support.

The first loader revision waited for a separate final secure-send vendor event that did not arrive through the chosen USB Kit path. The current loader checks each fragment's command completion, boots the image, and requires the operational boot event plus a successful standard HCI version query. The controller enforces signed firmware authentication.

## Sources and licensing

- [Haiku source](https://github.com/haiku/haiku), particularly `src/kits/bluetooth`, `src/servers/bluetooth`, and `src/add-ons/kernel/drivers/bluetooth/h2/h2generic`.
- [Linux Intel firmware protocol](https://github.com/torvalds/linux/blob/v6.12/drivers/bluetooth/btintel.c) and [USB transport](https://github.com/torvalds/linux/blob/v6.12/drivers/bluetooth/btusb.c).
- [Supported-command check](https://github.com/torvalds/linux/blob/v6.12/net/bluetooth/hci_sync.c) for Read Stored Link Key.
- [Intel firmware source](https://gitlab.com/kernel-firmware/linux-firmware), snapshot `412eb7861d40b9dc3dfa01d4059d4b5c414b0307`. Unmodified firmware and its redistribution license are included with the revision-specific artifacts. The bootstrap source is GPL-2.0-or-later; Haiku patches retain upstream licensing.

Firmware SHA-256:

```text
2e5f11ecc302e10dba51e9e07babef1b6d5c2a2359c695f1a23836552271d906  ibt-0041-0041.sfi
fe272982577efdc289cfe3e8bedafca607c0b7d6c9df1aab1880e22ef930d077  ibt-0041-0041.ddc
```

## Packaged startup validation

The final bounded helper enumerates the generic driver before raw bootstrap, checks adapter readiness, and permits at most one server restart. After installing it and rebooting, readiness passed on attempt 1 and a full inquiry completed successfully with three devices. A separate hard reset to bootloader followed by the final loader also passed the complete upload/boot/DDC/reset path. Warm reboot may preserve operational firmware; a cold power-cycle has not been physically tested.

The separate Bluetooth installer places its helper, firmware, server and probes under `/boot/home/config/non-packaged/data/x270-drivers/bluetooth`; the earlier development path is retained only for source/checkpoints. See [installer instructions](../installer/README.md).
