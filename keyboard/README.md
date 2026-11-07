# Function-key shortcuts for this Haiku machine

Build this directory with `make`. This is an input-server filter, not a kernel driver.

The release ZIP includes `payload/fn_shortcuts`, source, tests, and checksums.
Run `bash install.sh --verify`, then `bash install.sh --install`.
Use `bash install.sh --uninstall` to remove it and restore a previous filter.
The installer supports Haiku x86_64 hrev59866+92; rebuild for other revisions.
Exit F7 fullscreen before replacing/removing the filter.

The Git checkout stores the prebuilt binary in
`../artifacts/keyboard-haiku-x86_64-hrev59866+92/fn_shortcuts`.
To use the installer from the checkout, copy it into `payload/fn_shortcuts`
and generate `SHA256SUMS`, or use the complete release ZIP.
Installed add-on: `/boot/home/config/non-packaged/add-ons/input_server/filters/fn_shortcuts`.
Haiku loads this input-server filter automatically, including at subsequent boots.
No boot script or kernel driver modification is needed.

F1–F12 perform the shortcuts below. Hold either physical Alt key before pressing a
function key to bypass the shortcuts and pass the original event, including Alt
and any other modifiers, through to applications. Other keys pass through.
The routing choice is kept until the function key is released, even if Alt is
released first. The physical Alt modifiers are read from the keymap at filter load.

| Key | Action |
| --- | --- |
| F1 | Toggle mute |
| F2 | Reduce system volume by 3 dB |
| F3 | Increase system volume by 3 dB |
| F4 | Disabled |
| F5 | Reduce brightness by 5 percentage points |
| F6 | Increase brightness by 5 percentage points |
| F7 | Toggle active window's borderless fullscreen; restore its frame and border on next press |
| F8 | Disabled |
| F9 | Open `/boot/system/preferences` in Tracker |
| F10 | Disabled |
| F11 | Open Terminal |
| F12 | Launch `/boot/system/apps/Firefox/Firefox` |

Volume, mute, and brightness display a temporary native popup with a level bar.
Each update replaces the previous popup and disappears after 1.5 seconds.
Volume shows the mixer level as a percentage of its supported dB range and the dB value;
mute is read from the mixer after the change. Brightness shows a percentage.

Volume and brightness repeat while held. Other actions fire once until release.
F7 retains the application's internal menus/toolbars. The desktop and Deskbar are excluded.
An application can enforce size limits or reject window scripting. Fullscreen restoration
state lives in the input server and does not survive restarting it. Exit fullscreen
before removing or replacing the filter.

## Build and install

Run `make` in this directory, then `make install`. For an already loaded filter,
run `make uninstall` first, allow Haiku to unload it, then `make install`.
Do not overwrite a loaded binary in place.

## Remove

Run `make uninstall` in this directory. Haiku detects the removal and unloads the filter.
The function keys then resume their normal behavior. Source files remain here.

## Verification

`make check` runs tests for all function keys, mapped and unmapped events,
left/right Alt bypass, modifier ordering during a held key, repeat behavior,
and pass-through of unrelated input.
`make check-fullscreen` briefly opens a test window and verifies fullscreen entry
and restoration of the original frame and border.
The filter's successful live load was verified with `listimage` on input_server.
Physical keyboard presses have not been verified automatically.

`~/config/var/log/fn-shortcuts.log` records filter initialization and action exit statuses.
Volume popup percentages describe the supported mixer dB range, not perceived loudness.
Brightness requires driver support. Native popups require notifications to be enabled.
