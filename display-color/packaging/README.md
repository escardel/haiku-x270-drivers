# Display Color — bonus X270 display tuning app

Native Haiku controls for whole-display contrast (50–200%), gamma
(0.50–3.00), independent red/green/blue saturation (0–200%), and scheduled
blue-light filtering with intensity (0–100%). Includes a grayscale reference
and Defaults. Neutral is 100% contrast, 1.00 gamma and 100% on each saturation
slider; the filter is initially off. Existing contrast/gamma settings survive
upgrades. Higher gamma brightens midtones.

## Compatibility and safety

- Haiku **x86_64 only**, Intel Skylake/Kaby Lake graphics with `intel_extreme`.
  Developed and previously hardware-tested on the X270's Intel HD Graphics 620
  with Haiku hrev59866+92. Other graphics hardware is rejected.
- **No Haiku version or revision gate.** Package dependencies have no revision
  pins. This is not a promise of compatibility with every Haiku version:
  private Intel driver structures, register access and library ABIs may change.
  Other revisions and 32-bit Haiku have not been tested.
- This app writes display hardware lookup tables through the existing driver;
  it does not replace kernel drivers or system libraries. Strong adjustments
  can cause banding or clipping. Save work before trying an untested revision.
- Included historical hardware validation is distinct from package validation.
  Physical login and sleep/wake were not tested.
- The corrected RGB backend bypasses the unused plane-gamma stage; omitting
  that setting caused a black screen in an earlier experimental build. The
  user confirmed the corrected identity, mild RGB and blue-filter previews.
  CSC is refused while panel self-refresh is active. See `SAFETY-HISTORY.md`
  in the package documentation or `source/packaging/SAFETY-HISTORY.md` in the ZIP.

## Install and use

Version 1.1.1 corrects RGB matrix latching. The user confirmed visible RGB
adjustment in the complete app. The optional app preview sends adjustments to
its guard, so stopping the preview cannot abandon a frontend-held driver lock.

Extract the ZIP with Expander. Check the package before installation:

```sh
sha256sum -c SHA256SUMS
pkgman install ./displaycolor-1.1.1-1-x86_64.hpkg
```

Alternatively open the `.hpkg` in Haiku's package installer. Launch **Display
Color** from Deskbar's Applications menu. Installation alone does not launch
the app, modify display settings, enable startup or require a reboot.

Closing the controls leaves the app running in the background, without a
Deskbar running-app entry. Reopen it through Applications. Use **Quit** to stop
the background process. Quit does not reset the programmed hardware curve;
choose **Defaults** before quitting to restore neutral output.

The **Display** tab retains contrast, gamma and the grayscale reference, and
adds independent RGB saturation sliders. At 100% a channel is unchanged;
at 0% that channel moves toward luminance gray; above 100% it becomes more
saturated. These are saturation controls, not RGB brightness gains. Strong
settings can clip or shift hues.

The **Blue light filter** tab has a master enable checkbox, an intensity
slider, an automatic schedule checkbox, and on/off times in local 24-hour
`HH:MM` format. Higher intensity reduces more blue and a smaller amount of
green; 0% has no filtering. Enable the master checkbox for manual filtering,
or also select automatic scheduling for the specified interval. Defaults are
20:00–07:00. Overnight intervals work; start is inclusive and end exclusive.
Invalid times and equal scheduled on/off times are rejected without saving.
The app checks the clock every two seconds and after wake via the same timer.
Keep it running and enable login startup to use the schedule unattended.
Defaults disables the filter and schedule, but preserves the login preference.

Normal sliders apply immediately. For optional attended tests, quit Display
Color, disable its login startup, and open the companion `SafeDisplayTest`
beside the installed application. It uses a separate rollback process and a
12-second confirmation dialog. First let its no-change rehearsal expire.
See `SAFE-TESTING.md` in the package documentation or source tree. It cannot
recover a frozen kernel; leave startup disabled during testing.

To restore adjustments at login, enable **Start in background at login** in
the app. This uses the `boot/launch` folder: your UserBootscript must open that
folder's entries (the standard Haiku script does). Startup is opt-in. The app
checks every two seconds for display resets and reapplies the saved curve.
Enabled pipes receive the same curve; inactive pipes are skipped.

Settings: `~/config/settings/Display Color`. Startup link:
`~/config/settings/boot/launch/Display Color`. No personal settings are bundled.

If upgrading the earlier manually installed app, quit it first. Its custom
Applications shortcut may hide the packaged menu entry. Move that old shortcut
and `~/config/non-packaged/apps/Display Color` aside before using this package.
Disable then reenable the app's startup checkbox to replace the old startup
link with the packaged helper. Keep the settings file to retain adjustments.
To preserve an old launch path, it can instead be a symlink to the packaged
application. A per-user install uses `pkgman install --home ./PACKAGE.hpkg` and
places the app in `~/config/apps/DisplayColor/Display Color`; a system install
places it in `/boot/system/apps/DisplayColor/Display Color`.

## Uninstall / recovery

First choose **Defaults**, disable **Start in background at login**, then
**Quit**. Remove the package:

```sh
pkgman uninstall displaycolor
```

For a per-user installation, use `pkgman uninstall --home displaycolor` instead.

If the app cannot run, remove only its startup link
`~/config/settings/boot/launch/Display Color` and uninstall the package. A
display-driver reset or reboot normally clears its hardware curve. The saved
settings file is deliberately retained; remove it separately only if desired.
Old manual installations and their shortcuts are not removed by this package.

## Rebuild

The ZIP includes the application and packaging source, not personal settings
or development backups. On x86_64 Haiku with the C++ compiler, `make`, `zip`,
`sha256sum`, `package`, and matching private Intel driver headers installed:

```sh
cd source
bash packaging/build-package.sh
```

Artifacts are created in `dist/`: the native `.hpkg`, this source-inclusive
ZIP and `DisplayColor-SHA256SUMS`. The curve self-tests do not touch hardware.
Do not use `--verify-hardware` unless intentionally resetting output to neutral.
No explicit open-source license has been supplied; see the included notice.
