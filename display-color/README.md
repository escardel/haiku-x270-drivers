# Display Color

Version 1.1.1 fixes RGB matrix latching and keeps guarded preview hardware
access in the independent rollback process.

A native Haiku application with Contrast (50–200%), Gamma (0.50–3.00),
individual Red/Green/Blue saturation (0–200%), a grayscale reference, and a
Defaults button. Contrast defaults to 100%, gamma to 1.00, and all saturation
channels to 100%. Higher gamma brightens midtones. Adjustments affect the
whole display. Saturation is not RGB brightness gain: each output channel
blends between luminance gray at 0%, its original value at 100%, and stronger
color at 200%. Changing one slider leaves the other matrix rows unchanged.
Colored reference patches make RGB changes visible; the grayscale ramp remains
useful for contrast and gamma.

The **Blue light filter** tab has an enable checkbox and an intensity slider
(0–100%). It reduces blue and, more gently, green after the contrast/gamma
curve. At 0% there is no filter. Filtering is initially disabled so existing
settings retain their appearance. This is a display comfort feature, not a
medical claim or a calibrated color-temperature control.

Enable **Turn on and off automatically** and enter local 24-hour `HH:MM`
on/off times (default 20:00–07:00) to use the schedule. Overnight intervals
work; start is inclusive and end is exclusive. Invalid/equal scheduled times
are rejected without saving. The background timer reevaluates the local clock
every two seconds, including after wake or clock changes. The app must remain
running; enable login startup for unattended operation. The filter's enable
checkbox is the master switch, even when scheduling is selected.

Defaults resets contrast, gamma and RGB saturation to neutral and disables the
filter/schedule. It does not change the login startup preference. Existing
contrast/gamma settings are retained when upgrading; new controls default to
neutral/off. All controls and the schedule save in the same settings file.

Settings save immediately in `~/config/settings/Display Color`. Closing the
window keeps the application in the background. The app does not appear in
Deskbar's running-app list or the application switcher. Reopen its controls
through Deskbar's Applications menu or its Desktop shortcut.
Quit stops the background process; the
hardware curve remains until the display driver resets it.

“Start in background at login” can be enabled in the app. The background
process checks the hardware every two seconds and reapplies the saved curve
when a screen mode change or wake resets it. It skips disabled display pipes.
The startup shortcut is `~/config/settings/boot/launch/Display Color`.
It points to the native `DisplayColorStartup` helper, which launches the app
with `--background` through Haiku's application roster without opening a terminal.

The native package installs the application and its startup helper together
under `apps/DisplayColor`, with a Deskbar Applications menu entry. Both
executables resolve their companion relative to their own installed location.
No Desktop shortcut is installed. See `packaging/README.md` for installation,
manual-install migration, removal and compatibility details.

Supported hardware: Intel Skylake and Kaby Lake using Haiku's intel_extreme
driver. This version was built for the Intel HD Graphics 620 in this machine.
Other hardware is rejected before writing registers. Enabled display pipes
receive the same curve. This uses 256-entry hardware lookup tables, so strong
adjustments can produce banding or clipping.

Build: `make`. Curve checks: `make check`.
Package: `bash packaging/build-package.sh` (outputs in `dist/`).
`./DisplayColor --verify-hardware` applies the neutral curve and verifies
register readback and that a second application makes no changes. Run this
only when intentionally restoring neutral output.

The hardware backend uses the installed Haiku Intel driver headers and its
accelerant lock. Contrast/gamma and blue-light intensity use the 256-entry
lookup table. RGB saturation uses the pipe color-space conversion matrix,
disabled when all three saturation sliders are neutral. Register definitions
and coefficient encodings were checked against Linux v6.12 i915's
`intel_color_regs.h`, `skl_universal_plane_regs.h`, and `intel_color.c`.
No kernel or display driver files are modified.

RGB matrix programming also bypasses the unused plane-gamma stage. Omitting
that bypass caused a black display in an earlier experimental build; the
corrected identity/RGB/filter previews were confirmed by the user. CSC is
refused while panel self-refresh is enabled. See
`packaging/SAFETY-HISTORY.md` for the incident and its resolution.

The optional `SafeDisplayTest` companion runs attended 12-second previews with
an independent rollback process. Normal app sliders still apply immediately,
as before; they are not modal preview controls. See `SAFE-TESTING.md`.
Its app-sliders preview opens the full UI with temporary adjustments, settings
saves disabled and login startup locked off. Stronger 50% RGB and full-grayscale
presets are also available for checking visible matrix output.
