# Validation on Intel HD Graphics 620

The hardware results below describe development on Haiku hrev59866+92 x86_64,
not compatibility testing on other Haiku versions.

## RGB latch correction and interactive preview (November 1, 2026 PST)

- The user requested continued work on visible RGB output while retaining the
  original contrast/gamma controls and the working blue-light tab and schedule.
- Found a CSC sequencing defect: coefficient/offset readback followed
  `CSC_MODE` arming without waiting for frame latching. The generic posted-write
  flush also read each just-written CSC data register. The Intel reference
  driver documents read-disarming behavior; ordinary array-backed register
  tests did not model it. This is a candidate RGB fault, not a confirmed
  explanation for the historical blackout.
- CSC writes now flush through pipe control, data is checked before arming,
  and frame-counter advances separate arming from subsequent data reads.
  Capture and rollback also wait before accessing buffered CSC data. Waits
  are bounded and return an error for stopped scanout.
- A read-only hardware probe observed active pipe A advance three frames in
  50 ms. Inactive pipes B and C did not advance.
- Added simulated matrix latching and read-disarming. Tests check the matrix
  that actually latched, independent RGB changes while CSC remains enabled,
  unchanged refreshes without writes, and stopped-frame timeout without writes.
- Added an isolated-settings test for the complete app's `--preview` mode:
  normal settings save correctly, preview changes do not overwrite them,
  and preview startup actions are ignored. No hardware is accessed by it.
- `make all check check-safety` passes. Contrast/gamma, RGB coefficient
  encoding, filter intensity, schedule boundaries, rollback and child cleanup
  continue to pass.
- A mutation check removed the wait after CSC arming from a temporary source
  copy. The hardware test failed because the expected matrix never latched,
  confirming that this regression is detected.
- Both the full app and diagnostic now show colored reference patches.
  Diagnostic RGB presets use 50% rather than subtle 105% changes, and a
  full-grayscale preset makes matrix output easy to verify.
- The **App sliders (guarded 12-second preview)** opens the complete app with
  live, temporary controls. It does not save settings or allow login startup.
  The guard stops the preview and restores the original hardware snapshot
  on confirmation, timeout, or frontend death.
- Updated candidates are staged in `/tmp/displaycolor-rgb-preview/`.
  The rebuilt native guard completed its no-change timeout rehearsal with
  `result=0`, `dirty=0`, `expired=1`; the app-sliders preview is unlocked.
  The saved settings hash is unchanged and login startup remains disabled.
  The user subsequently confirmed that RGB adjustment worked using the
  complete app's controls.

## Preview cleanup correction for version 1.1.1

- The first successful RGB preview exposed a cleanup defect: killing the
  frontend while it held the driver's lock left the rollback guard waiting.
  Verified that the app was dead, lock count was two, the semaphore had one
  waiter, and that waiter was the guard. Released only the abandoned owner's
  lock; the guard then restored the original snapshot successfully.
- Preview frontends now never initialize the hardware backend or acquire its
  driver lock. They send temporary settings to the independent guard, which
  performs all hardware operations and rollback. The frontend can be killed
  during a request without abandoning a hardware lock.
- The simulated guard checks include accepted live adjustment requests and
  rejection of stale and expired requests, plus child and frontend death.
- A native full-app preview reached its deadline and recorded `result=0`,
  `dirty=0`, `expired=1`. The driver semaphore was free afterward, and a
  read-only check confirmed all saved palette entries and control bits were
  restored. The settings hash remained unchanged.
- From the extracted 1.1.1 package, confirmed that the preview frontend has
  no hardware mappings and forcibly stopped only that frontend. The guard
  restored promptly with `result=0`, `dirty=0`; the driver lock was free.
- Installed and opened version 1.1.1 through the actual Applications shortcut.
  Verified the running binary/version, both control tabs, the original saved
  settings and disabled login startup. See `INSTALL-VALIDATION.md`.

## Startup-blackout investigation (November 1, 2026 PST)

- The user reported a brief blackout a few seconds after opening the installed
  app without changing any controls. This later event remains unexplained;
  the earlier successful CSC previews do not establish that startup is safe.
  Packaging was paused at that stage; subsequent corrections and confirmations
  are recorded above and in `packaging/SAFETY-HISTORY.md`.
- Rebuilt the pending refresh changes: unchanged hardware state causes no
  register writes, individual palette repairs touch only mismatched entries,
  and plane/surface updates occur only when their control state needs changing.
  This reduces unnecessary writes; it is not a confirmed blackout diagnosis.
- `make all check check-safety` passed with warnings enabled. Expanded the
  simulated hardware checks to cover ten consecutive unchanged CSC refreshes,
  complete snapshot restoration, repeated restoration without writes, and
  a sleeping display without writes. The independent watchdog tests also
  verify startup-child termination before restoration on timeout, confirmation,
  and frontend death.
- A read-only live snapshot matched all 256 palette entries and control bits
  for saved contrast 100%, gamma 0.80, neutral RGB and filtering disabled on
  active pipe A. No live display registers were written by this inspection.
- Prepared the rebuilt guard and a traced candidate in
  `/tmp/displaycolor-guarded-startup/`. Startup trace output goes to
  `/tmp/displaycolor-package-validation/startup-trace.log`; preview events
  go to `safe-test-events.log` alongside the guard.
- Next required observation: let the no-change rehearsal expire, then
  manually select **App startup (guarded 12-second test)**. Leave the app
  controls alone and confirm only if the display stays correct. The guard
  terminates the app and restores its snapshot even on confirmation.
  The user subsequently completed the rehearsal and clicked OK on the startup
  preview after approximately 10.7 seconds. The guard recorded `state=kept`,
  `test=6`, `result=0`, and `dirty=0`: the app was terminated and the original
  display restored successfully.
- The startup trace records the initial apply and five two-second refreshes.
  Each reports zero mismatches and zero changed pipes, with plane control
  `0xc4000000` and bottom control `0x80000000` unchanged. This confirms the
  candidate's idle startup path made no display-register writes during the
  attended preview. It does not identify the cause of the earlier blackout
  or validate physical sleep/wake and package activation.
- Login startup remains disabled. The installed package and release assets
  have not been replaced during this investigation. The saved settings hash
  before preparing the diagnostic was
  `13f878af2c758c8fad8636dd0141fc873cbb26669d5543453a34d1747af42a56`.
  The hash remained identical after the confirmed startup preview.

## RGB saturation and blue-light update

- An early CSC implementation blacked out the display despite register
  readback success. The independent native preview guard restored the original
  snapshot after 12 seconds; the user confirmed recovery.
- Comparing the pipeline setup with the reference driver identified the
  missing plane-gamma bypass. The revised identity preview read back active
  pipe A control `0xc4802000` and was visually confirmed by the user with OK.
- The guard recorded separate OK confirmations for red, green and blue at
  105%, plus the blue filter at 15%. The user reported the 5% RGB changes were
  subtle and the blue filter was clearly visible, similar to f.lux.
- Integrated that exact bypass into the production backend. Returning RGB to
  neutral restores the captured plane-gamma bit during the session. CSC is
  refused when panel self-refresh is enabled.
- Self-tests cover every RGB percentage from 0 through 200 for coefficient
  encoding accuracy, neutral identity, independent matrix rows, grayscale,
  tone curves, filter intensity, time parsing and all minutes in both daytime
  and overnight schedules.
- Simulated independent-process guard tests passed for timeout, late/stale
  confirmation rejection, explicit keep, exit restoration and frontend death.
  A native no-change countdown also expired and unlocked hardware tests.
- Earlier GUI tests exercised filter intensity, invalid-time rejection,
  automatic minute-boundary activation/deactivation, persistence across app
  restart and Defaults. Tone settings were restored to the user's 100%
  contrast and 0.80 gamma. The corresponding schedule code is unchanged.
- Saved-settings hashes stayed identical throughout the attended previews.
  Login startup remains disabled; no firmware or kernel drivers were changed.

## Native package validation (November 2, 2026 UTC)

- Built the app and native startup helper with warnings enabled. Curve
  self-tests passed both before packaging and from the extracted `.hpkg`.
- Package metadata has unversioned dependencies and no Haiku revision gate;
  the package architecture is explicitly x86_64.
- Extracted both packaged binaries and verified byte-for-byte equality with
  the build outputs. Checked the relative Deskbar Applications symlink.
- Tested startup-link creation against an isolated settings directory using
  the production `SetStartup` implementation: relocated companion resolution,
  repeated enable/disable, and refusal to replace an unrelated file or link.
- Ran the native helper in a relocated directory with a harmless stand-in
  application: it launched its sibling with `--background`. No display hardware
  was touched by these packaging tests.
- Checked ZIP integrity and both outer and bundled package checksums. Source
  and build instructions are included; personal settings and development
  backups are excluded.
- Cross-version operation, real login, physical sleep/wake, and package removal
  have not been exercised. The local package installation result is recorded
  separately in `INSTALL-VALIDATION.md` on the development machine.

## Historical hardware validation

- Built with the installed Haiku compiler and headers, with warnings enabled.
- Curve tests passed for neutral output, monotonicity, equal channel values,
  and the direction of contrast and gamma adjustments.
- Hardware readback verified all 256 neutral lookup table entries and the
  gamma enable bits on active pipe A. Inactive pipes B and C were untouched.
- Applying an identical curve a second time performed no writes.
- Through the application scripting interface, set contrast to 110% and gamma
  to 1.20. Hardware lookup table samples changed as expected.
- Quit the app and read back the curve: the adjustment remained active.
- Relaunched through the actual boot shortcut: the window stayed hidden and
  the controls loaded 110% contrast and 1.20 gamma from the saved settings.
- Replaced the active curve with the neutral curve externally. Within three
  seconds, the background process restored the saved adjustment.
- Reopened and hid the window through application messages.
- Restored defaults and saved 100% contrast and 1.00 gamma.
- Disabled and reenabled startup through the checkbox; the boot shortcut was
  removed and recreated correctly.
- Opened the Desktop shortcut and confirmed that the existing background
  instance showed its window with neutral values and active status.

Login itself and physical sleep/wake were not exercised. Startup was tested
by opening the same shortcut used by the existing UserBootscript. Recovery was
tested by resetting the hardware curve while the background process ran.

## Hidden background app and Applications menu

- Rebuilt with `B_SINGLE_LAUNCH | B_BACKGROUND_APP`; the installed executable's
  app flags read back as `0x4`.
- Startup opened a hidden window. Enumerating the live Deskbar's running-app
  menu showed Tracker, Firefox, and Terminal, with no Display Color entry.
- Added `~/config/settings/deskbar/menu/Applications/Display Color`, pointing
  directly at the installed application.
- Opening that menu shortcut showed and activated the existing window.
- After quitting, opening the menu shortcut launched a new instance and showed
  the controls successfully.
- Hiding the controls kept the app running without a Deskbar entry.
- The user's saved 104% contrast and 1.00 gamma settings were retained. The
  saved settings file's SHA-256 was unchanged.
- Curve self-tests still passed. Hardware curve programming was not changed
  or reset during this update.
