# Local installation validation

## Corrected version 1.1.1 (November 2, 2026 PST)

- The user confirmed visible RGB adjustment with the full app's sliders.
- Built `displaycolor-1.1.1-1-x86_64.hpkg` and its source ZIP after all curve,
  simulated hardware, isolated preview-settings and guard tests passed.
  Extracted executables match the build byte for byte; ZIP integrity and
  outer/inner checksums passed.
- The packaged preview frontend was verified to have no Display Color hardware
  mappings. Stopping that frontend abruptly caused prompt successful rollback,
  with a free driver semaphore and all original palette entries/control bits
  restored. A separate native timeout preview also restored successfully.
- Upgraded the home package from 1.1.0-1 to 1.1.1-1 using `pkgman install --home`.
  The installed executables match the validated build. Earlier unconfirmed
  1.1.0 artifacts and the prior installed package are archived under
  `backup-before-rgb-saturation/unconfirmed-1.1.0/`.
- Opened the actual Applications menu shortcut. The running executable resolves
  to `~/config/apps/DisplayColor/Display Color`; its version is 1.1.1 and its
  flags are `0x4` (background app).
- Confirmed the installed GUI loaded contrast 100%, gamma 0.80, all RGB
  saturation channels 100%, filter disabled, intensity 40%, schedule disabled,
  and on/off times 20:00 and 07:00. The login startup checkbox is unchecked
  and no startup shortcut exists.
- The saved settings file remains byte-for-byte unchanged, SHA-256
  `13f878af2c758c8fad8636dd0141fc873cbb26669d5543453a34d1747af42a56`.
  A read-only check after startup found all 256 palette entries and control
  bits matched those saved settings.
- The installed app is open for normal use. Physical sleep/wake and other
  Haiku revisions remain untested.

## Historical 1.1.0 installation

- Installed `displaycolor-1.1.0-1-x86_64.hpkg` with `pkgman install --home`.
  The native package was activated without installing additional dependencies.
- Replaced the previous non-packaged executable path with a compatibility link
  to the packaged application. The prior executable remains in
  `backup-before-rgb-saturation/installed-before-native-package`.
- Verified that the packaged main executable, startup helper and guarded
  diagnostic match the built binaries.
- Launched through the packaged helper in the background; its controls showed
  100% contrast, 0.80 gamma, 100% on each RGB saturation slider, the blue-light
  filter disabled and the login startup checkbox unchecked.
- Reopened the running instance via the existing Applications menu shortcut.
  The scheduled filter controls showed the defaults, 20:00 to 07:00.
- The settings file remained byte-for-byte identical to the saved backup.
  Login startup remains disabled.
- Following installation and startup, the user reported that the screen went
  black for a few seconds. Its exact timing and cause are unknown. The app was
  stopped, and no further display changes were initiated while investigating.
  A read-only check found the current palette and control bits match the saved
  neutral settings; the log records a display power-on event, but neither
  observation establishes the cause of the blackout.
- Packaging and publication were blocked while that event was investigated.
  Later corrections and confirmed results are recorded above and in
  `packaging/SAFETY-HISTORY.md`.
