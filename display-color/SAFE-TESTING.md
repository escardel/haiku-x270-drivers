# Attended display testing

An earlier experimental RGB matrix build produced a black display despite
successful register readback. The corrected backend includes plane-gamma bypass
and was visually confirmed with identity, mild RGB and blue-filter previews.
See `packaging/SAFETY-HISTORY.md` for the incident history and limitations.
After installation and startup, a later brief blackout was reported; its cause
remains unexplained. The user has now confirmed visible RGB adjustment in the
complete app. Version 1.1.1 also corrects
CSC register sequencing: coefficient/offset reads wait for frame latching,
posted writes flush through pipe control, and CSC is armed after staging and
checking the matrix.

## Recovery conditions

- Display Color's login shortcut has been moved out of `boot/launch` to
  `backup-before-rgb-saturation/startup-disabled-during-testing`. Do not
  reenable login startup during testing.
- Tests never save display settings, change firmware, or install drivers. They
  only change volatile display lookup tables and color-matrix registers.
  A reboot is expected to reset these registers, and the disabled shortcut
  prevents the experimental values being automatically reapplied.
- An independent process owns the hardware snapshot and a monotonic 12-second
  deadline. If the dialog/app closes or crashes, it restores the snapshot.
  In an app-sliders preview, the app sends adjustments to that process and
  never maps display registers or acquires the driver lock. Stopping the app
  therefore cannot abandon a driver lock held by the frontend.
  This cannot recover a frozen kernel or a failed display driver.
- The production Display Color app must be closed during hardware tests. The
  guard refuses tests if that app is running or the login shortcut exists.
- Panel self-refresh is checked before testing. Tests are refused if enabled,
  because Intel documents CSC/PSR interactions that can produce black output.
  This is a precaution, not a diagnosis of the previous failure.

## Run

Build only the diagnostic tool, without rebuilding/replacing the normal app:

```sh
make check-safety
open ./SafeDisplayTest
```

`check-safety` simulates the watchdog in separate processes without accessing
display hardware. It tests timeout, late and stale confirmation rejection,
explicit keep, exit restoration, and frontend-process death restoration.

The native window starts with all hardware-changing tests disabled. First run
**Timeout rehearsal (no display change)** and let its dialog expire. Then:

1. Click **RGB identity + plane-gamma bypass**. No color change is intended;
   this tests a revised pipeline setup separately from saturation arithmetic.
2. If the display is visible and correct, explicitly click **OK, keep this
   test**. Enter/Escape default to **Revert now**. OK is initially disabled,
   then becomes available after the preview has been active for two seconds.
3. If the screen is black or incorrect, do nothing. The independent process
   restores the original snapshot after 12 seconds.
4. Only a visually confirmed identity test unlocks the 50% red, green and blue
   saturation tests and **All RGB 0% — whole display grayscale**. The colored
   patches help show the difference; the grayscale test should remove color
   from the whole desktop. The 15% blue-light test is separate.
5. Each new test starts from the original snapshot. OK retains the test only
   for this session; **Restore original display**, closing the tool, or a
   frontend crash restores the original snapshot, even after OK.

The separate **App sliders (guarded 12-second preview)** launches the complete
companion app under the independent guard with `--preview`. Try an RGB slider
and watch its colored reference patches. The original contrast/gamma controls
and blue-light tab, intensity and schedule remain available. This mode keeps
all changes in memory, skips settings saves, and disables login startup.
Its confirmation button remains disabled for eight seconds.
If the display goes black at any time, wait; the guard will
terminate the app and restore the snapshot when the timer expires. Clicking OK
also ends this app preview, terminates the app and restores the snapshot.
For diagnosis, a separately compiled tracing build can be placed next to the
guard as `Display Color`; it records refresh decisions without personal data.

The first guarded identity test without plane-gamma bypass also blacked out;
the user confirmed that waiting for the independent timer restored the screen.
The revised candidate sets the pre-GLK plane-gamma-disable bit, as Linux's
`skl_plane_ctl()` does. Haiku's existing plane control lacks that bit.
The user subsequently clicked OK and confirmed that the revised identity test
looked normal. The guard recorded confirmation, and active pipe A read back
`0xc4802000`, with the CSC and plane-gamma-bypass bits enabled. This supports the
bypass change. The user also confirmed each independent 105% RGB preview (with
subtle changes) and the clearly visible 15% blue-filter preview. The production
backend now uses the same bypass. The rollback snapshot includes this bit so
the candidate is fully reversible.

In a native package, the optional tool is alongside the app:
`~/config/apps/DisplayColor/SafeDisplayTest` for a home install, or
`/boot/system/apps/DisplayColor/SafeDisplayTest` for a system install. It is not
launched at login and is not required for normal scheduled filtering.

No tests advance automatically. Do not change screen modes or launch the normal
Display Color app while testing. Stop after any black-screen result and report
which button was used. Leave startup disabled until the incident is resolved.

`safe-test-events.log` records guard readiness, application, confirmation,
timeout and restoration events. Guard code is in `SafeDisplayTest.cpp`; it
reuses the production backend in a separate diagnostic translation unit.

For a GUI-only simulation:

```sh
./SafeDisplayTest --launch-simulation
```

The simulation banner explicitly states that no hardware is changed.
