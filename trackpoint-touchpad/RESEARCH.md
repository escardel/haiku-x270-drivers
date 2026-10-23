# Offline research and next hardware check

Historical research notes. The subsequent driver build, reboot, and physical drag validation passed; see [README.md](README.md) and [validation/RESULTS.md](validation/RESULTS.md) for the current state.

Recorded 2026-10-22 while the X270 is unreachable. These are leads for testing, not claims about this unit's current input behavior.

## Existing Haiku implementation

Haiku already has PS/2 TrackPoint, Synaptics, ALPS, and Elantech probes in [`ps2_dev.cpp`](https://github.com/haiku/haiku/blob/660ea78079ca9ee36623a0562208476bce89b11e/src/add-ons/kernel/bus_managers/ps2/ps2_dev.cpp). The TrackPoint probe runs first and uses the standard mouse hooks when detected. Synaptics has a dedicated pass-through device path, `input/mouse/ps2/synaptics_passthrough`, for a pointing stick behind the touchpad. The upstream source baseline examined here is Haiku commit `660ea78079ca9ee36623a0562208476bce89b11e`.

An [X270 Haiku report](https://discuss.haiku-os.org/t/thinkpad-x270-touchpad/19672) describes intermittent TrackPoint and touchpad detection and identifies both devices as PS/2. The discussion points to active multiplexing and packet routing as investigation areas. Another X270 owner reports a working touchpad but no detected TrackPoint. These reports show that detection varies; they do not identify this machine's hardware or prove a common cause.

The [keyboard publication ordering fix](https://review.haiku-os.org/c/haiku/+/5388) referenced in that discussion was merged in 2022. It should be checked in the X270 checkout before proposing it again.

## On reconnect

1. Save `git status --short`, `git branch --show-current`, and `git diff` from `/boot/home/haiku` branch `x270-ps2-debug` before changing files.
2. Record `/dev/input` entries and boot log lines mentioning `ps2`, `synaptics`, `trackpoint`, `elantech`, `alps`, and controller multiplexing.
3. Compare observed device names and behavior with the exact probe order and pass-through handling in the target checkout. Do not assume every X270 uses the same touchpad vendor.
4. Export only relevant PS/2 changes as a reviewable patch. Keep the HDA and camera experiments separate.
5. Build the PS/2 module, test cold and warm boots, and record TrackPoint movement, all buttons, touchpad motion, tapping, and dragging independently.

The X270 working-tree diff was subsequently exported to [patches/x270-ps2-working.patch](patches/x270-ps2-working.patch). Its functional changes and diagnostic logging still need separate review and hardware validation.
