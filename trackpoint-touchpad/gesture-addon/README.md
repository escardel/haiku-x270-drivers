# X270 touchpad gesture add-on

This is the native Haiku `input_server` mouse device add-on, with local gesture fixes tested on the ThinkPad X270 running `hrev59866+92` x86_64. It accompanies the existing PS/2 module; the kernel module is unchanged in this update.

- One-finger tap: left-click. Two-finger tap: right-click. Three-finger tap: middle-click.
- A light tap can start at the existing minimum contact-pressure threshold.
- Multi-finger gestures suppress pointer movement until all fingers lift. Changing finger counts resets accumulated movement so primary-coordinate changes do not become pointer jumps.
- Multi-finger synthetic clicks schedule their button release, including when no further hardware packet arrives.
- Gesture state is initialized, and physical clicks or excessive motion do not produce an extra tap.

Tap sensitivity zero disables tapping; positive values enable the same light-contact detection. Existing scroll direction, speed, edge-motion and physical-button preferences are retained.

Run `bash build.sh` on the matching Haiku revision with GCC and development headers installed. It builds `mouse` and runs 22 gesture checks. The source copies retain the Haiku MIT notices. Their baseline is the mouse device directory at Haiku commit `5e32b4dc9d36d6bc82f7ec312cf498b3b2654b1a`; `../patches/0003-touchpad-gestures.patch` records the changes against the X270 checkout. These are local downstream changes, not an upstream Haiku submission.

Install through the TrackPoint/touchpad ZIP. The add-on goes to `/boot/home/config/non-packaged/add-ons/input_server/devices/mouse`; packaged Haiku files are retained. Uninstall restores a pre-existing override, or removes this override if none existed. The input server monitors that directory and reloads the selected add-on. The combined installer also includes the kernel module, so reboot after installing the complete package.

See [physical and automated validation](../validation/TOUCHPAD-GESTURES.md). Temporary per-event diagnostics and the native check application are not included in the installed payload.
