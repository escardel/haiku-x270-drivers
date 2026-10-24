# ThinkPad X270 TrackPoint and touchpad on Haiku

The custom PS/2 bus manager now preserves physical TrackPoint button state during movement. It was built and reboot-tested on Haiku `hrev59866+92` x86_64. The user confirmed Tracker icon dragging, normal clicks, and right-clicks work correctly.

## Cause and fix

The physical TrackPoint buttons are wired to the Synaptics touchpad. The old driver emitted button presses from the touchpad device, while TrackPoint movement packets reported no pressed buttons. A controlled comparison showed 32 TrackPoint movement messages losing the left-button state during one drag; the USB mouse kept that state on all 72 movement messages in its comparison drag.

The fix uses the device's stick-button capability to route those buttons through the TrackPoint guest mouse queue. A stationary press or release generates a zero-motion packet; every later guest movement retains the held state. This also keeps click counting in the standard PS/2 mouse path and avoids duplicating the buttons through the touchpad gesture path. Firmware 8.2 reports without a valid extended-button flag and secondary-finger reports do not clear the state.

The source comparison and event measurements are in [validation/RESULTS.md](validation/RESULTS.md). The temporary pointer tracer was removed after testing.

## Touchpad gestures, 2026-10-23

The combined installer now also includes a native input_server mouse add-on. It fixes light tap-to-click, implements reliable three-finger tap middle-click and suppresses pointer jumps when fingers lift unevenly after two-finger scrolling. The user confirmed movement, taps, middle-click and scrolling work. The existing working PS/2 module is unchanged.

Source and a standalone native build/test script are in [gesture-addon/](gesture-addon/). The add-on preserves existing scroll preferences; enable tapping in Input preferences (tap sensitivity zero disables it). Its changes are recorded in `patches/0003-touchpad-gestures.patch`. See [gesture validation](validation/TOUCHPAD-GESTURES.md) for the 22 checks and physical confirmation, and the [installer guide](../installer/README.md) for upgrades and uninstall.

## Source patches and build

Apply `patches/x270-ps2-working.patch` first to the Haiku source base documented in the root README, and commit that baseline before applying `patches/0002-route-trackpoint-buttons.patch` with `git am`.

On the target, the original PS/2 work is commit `e9dacb5c0f`; the drag fix is `b7ed43bcd2` on branch `x270-ps2-drag`. The source checkout is `/boot/home/haiku`.

```sh
LIBRARY_PATH=/boot/system/lib:/boot/system/develop/lib jam -j2 ps2
```

This library path is required by the build's host metadata tools on this machine. The normal compile, link, MIME, version, and permission steps all succeeded. The deployed 77,251-byte module is included in the root `artifacts/` directory.

## Recovery

The installed module is `/boot/home/config/non-packaged/add-ons/kernel/bus_managers/ps2`. The previous custom module is preserved at `/boot/home/x270-camera-mic/backups/ps2-before-drag-fix`; restoring it and rebooting rolls back this drag fix. Removing the override returns to packaged Haiku behavior.

[RESEARCH.md](RESEARCH.md) preserves the earlier investigation notes. Other pointing-device models and hardware configurations have not been tested.
