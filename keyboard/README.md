# Function-key shortcuts for this Haiku machine

Build this directory with `make`. This is an input-server filter, not a kernel driver.

## Native installation

Open `x270_keyboard_shortcuts-2026.11.07.1-1-x86_64.hpkg` to install with
Haiku's graphical package installer. The filter loads automatically; no boot
script or kernel driver modification is needed. The package requires Haiku
x86_64 hrev59866+92. Rebuild for other revisions.

The native source ZIP includes the `.hpkg`, build source, tests and checksums.
Check `Keyboard-SHA256SUMS` for the release downloads or `SHA256SUMS` inside
the extracted native ZIP. Build with `python3 packaging/build-package.py`.

The package installs `add-ons/input_server/filters/fn_shortcuts`, exposed at
`/boot/system/add-ons/input_server/filters/fn_shortcuts` for a system install
or `~/config/add-ons/input_server/filters/fn_shortcuts` for a home install.
A normal system install activates without rebooting. A first-time home install
may require rebooting because its add-on directory did not exist when
input_server started.

**Upgrading a manual/ZIP installation:** exit F7 fullscreen first. If installed
using the legacy ZIP script, run that ZIP's `bash install.sh --uninstall`.
Move any remaining manual filter at
`~/config/non-packaged/add-ons/input_server/filters/fn_shortcuts` out of that
directory before installing the package. A non-packaged copy can override the
packaged copy. The native package does not delete or modify manual files.

**Remove the native package:** exit F7 fullscreen, then remove
`x270_keyboard_shortcuts` through HaikuDepot, or run
`pkgman uninstall x270_keyboard_shortcuts` (add `--home` for a home install).
The add-on unloads and normal function-key behavior returns. If a manual
copy is still present it remains active. No personal settings are bundled.

The original ZIP/script installer remains available as a separate legacy
installation method; its backup/restore state belongs to that script.

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
| F12 | Launch Firefox |

**Alt+,** snaps the active window to the left half of the screen;
**Alt+.** snaps it to the right half; **Alt+/** maximizes it with its
title bar and border intact. Either physical Alt key works.
The window border/title tab fits inside its half. Snapping out of F7 fullscreen
restores the border first. Application size limits still apply.
Comma, period and slash without Alt pass through normally.

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

For a manual installation, run `make` in this directory, then `make install`. For an already loaded filter,
run `make uninstall` first, allow Haiku to unload it, then `make install`.
Do not overwrite a loaded binary in place.

## Remove

For a manual installation, run `make uninstall` in this directory. Haiku detects the removal and unloads the filter.
The function keys then resume their normal behavior. Source files remain here.

## Verification

`make check` runs tests for all function keys, mapped and unmapped events,
left/right Alt bypass, modifier ordering during a held key, repeat behavior,
and pass-through of unrelated input.
`make check-fullscreen` briefly opens a test window and verifies fullscreen entry
and restoration of the original frame and border, left/right snapping, and
snapping out of fullscreen, and maximize.
The filter's successful live load was verified with `listimage` on input_server.
Physical keyboard presses have not been verified automatically.

`~/config/var/log/fn-shortcuts.log` records filter initialization and action exit statuses.
Volume popup percentages describe the supported mixer dB range, not perceived loudness.
Brightness requires driver support. Native popups require notifications to be enabled.
