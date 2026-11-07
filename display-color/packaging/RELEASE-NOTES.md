## Bonus app: Display Color

**Optional display tuning app for the ThinkPad X270 — separate from the four
driver installers.** Use `displaycolor-1.1.1-1-x86_64.hpkg` directly, or download
`displaycolor-1.1.1-any-haiku-revision-x86_64.zip` for the same native package,
source, build instructions, installation/removal notes and an internal checksum.
`DisplayColor-SHA256SUMS` covers both new downloads; the existing driver
`SHA256SUMS` is unchanged.

- Whole-display contrast (50–200%), gamma (0.50–3.00), independent RGB
  saturation (0–200%), grayscale reference, and Defaults. Higher gamma brightens
  midtones. Saturation defaults to neutral at 100% on each channel.
- Blue-light filter with an intensity slider (0–100%), master enable switch,
  and optional automatic local-time `HH:MM` on/off schedule, including overnight
  intervals. Filtering is initially off and existing tone settings are kept.
  Keep the app running for scheduled filtering; login startup is optional.
- Intel Skylake/Kaby Lake with Haiku's `intel_extreme` only; developed on the
  X270's Intel HD Graphics 620. **x86_64 only. No Haiku revision/version gate**,
  but private driver structures and library ABIs mean compatibility with every
  Haiku release is **not guaranteed**. Other revisions are untested.
- Installs as a normal Haiku application, with a Deskbar Applications entry;
  no replacement kernel drivers or system libraries. Installation does not
  launch the app or enable automatic startup. Startup is opt-in in the app.
- `pkgman install ./displaycolor-1.1.1-1-x86_64.hpkg`, then open **Display Color**
  from Applications. For removal: choose **Defaults**, disable startup, **Quit**,
  then `pkgman uninstall displaycolor`. Saved settings are retained.
- Curve/matrix/schedule self-tests and live hardware register-readback checks
  passed on hrev59866+92. Historical and current validation are documented
  separately in the ZIP. Other Haiku versions, real login and physical
  sleep/wake remain untested. The driver warnings elsewhere in this release
  still apply unchanged.
- Includes the corrected plane-gamma bypass used for RGB CSC. The user
  confirmed identity, separate mild RGB previews and the visibly effective
  15% blue-filter preview. Earlier unsafe development binaries are not included.
  An optional `SafeDisplayTest` companion offers 12-second attended previews
  with independent rollback; normal application sliders still apply immediately.
- Version 1.1.1 fixes RGB matrix staging/readback and frame latching. The user
  confirmed visible RGB changes using the complete app's sliders. Color patches
  make their effect easier to see. The optional temporary app preview applies
  adjustments through its guard and never holds the display driver's lock,
  preventing forced preview cleanup from abandoning that lock.

If upgrading a manually installed Display Color, quit the old app and move its
custom Applications shortcut and non-packaged executable aside first; disable
and reenable startup in the packaged app. See the bundled README for details.
