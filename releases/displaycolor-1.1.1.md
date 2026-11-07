# Display Color 1.1.1

Optional native Haiku application, distributed separately from the kernel/input
driver installers. This repository addition preserves the existing corrected
1.1.1 source and license notice.

- Whole-display contrast, gamma, independent RGB saturation and grayscale/color reference.
- Blue-light filter with intensity and optional automatic overnight scheduling.
- Opt-in background startup, saved settings, periodic reapplication after display resets.
- SafeDisplayTest companion with an independent rollback process for attended previews.
- Native `.hpkg`, source-inclusive ZIP and SHA-256 checksums.

Intel Skylake/Kaby Lake with `intel_extreme`, x86_64 only. Developed on the X270's
Intel HD Graphics 620 with hrev59866+92. No revision gate; other revisions remain
untested. Private driver structures/registers can change. No kernel drivers or
system libraries are replaced. Installation does not launch the app or enable
startup. Preserve the documented safety history and recovery instructions.

## Install and remove

Check `DisplayColor-SHA256SUMS`, then install with
`pkgman install ./displaycolor-1.1.1-1-x86_64.hpkg`, or open the native package
installer. Open Display Color from Applications. Per-user installation is
supported with `pkgman install --home`.

Before removal, choose Defaults, disable login startup and Quit; then run
`pkgman uninstall displaycolor` (add `--home` for a per-user install).
Settings are retained. See `display-color/packaging/README.md` for manual-install
migration and recovery details.

## Validation of this repository addition

Rebuilt all three executables from the copied source on the X270. All four test
suites passed: curve/matrix/schedule, simulated hardware behavior, isolated
preview settings and simulated independent guard/rollback behavior. No display
hardware writes or installation were performed during this preparation.
ZIP integrity, bundled-source parity, native package contents and distribution
checksums were verified. Earlier live hardware/user validation is historical
and remains documented in the source tree. No new physical login or sleep/wake
testing was performed.

No explicit open-source license has been supplied. The original license notice
is included unchanged; source availability does not imply a license grant.
