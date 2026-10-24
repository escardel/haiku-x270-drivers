# Separate X270 installers

Four independent ZIP installers are published in [GitHub Releases](https://github.com/escardel/haiku-x270-drivers/releases):

| Component | Included changes |
| --- | --- |
| TrackPoint/touchpad | Existing PS/2 held-button routing fix plus native mouse add-on for tap-to-click, three-finger middle-click and stable two-finger scroll lift-off |
| Webcam | UVC media add-on and streaming/idle fixes |
| Microphone/audio | HDA, Media Kit recorder and mixer, SoundRecorder, input/output configuration |
| Bluetooth | AX210 signed firmware bootstrap, patched Bluetooth Kit/server, bounded startup checks |

These binaries target **Haiku hrev59866+92 x86_64 on the tested ThinkPad X270**. Other revisions require rebuilding. They are experimental components, with the limits documented in the development log. Bluetooth inquiry works; pairing and profiles have not been validated. The camera is validated at 640x480; high-resolution reopen and long-term USB reliability remain open.

Extract the desired ZIP using Expander. In Terminal, enter its directory and run:

```sh
bash install.sh --verify
bash install.sh --install
```

Save work before installation and reboot afterward. The microphone installer briefly restarts media services and configures HD Audio output and the internal microphone. The webcam requires `libjpeg_turbo`, available with `pkgman install libjpeg_turbo`. No installer changes Firefox preferences or reboots automatically.

To remove one component, use its extracted installer:

```sh
bash install.sh --uninstall
```

Each installer retains previous overrides and relevant settings in `/boot/home/config/settings/x270-drivers/COMPONENT`. Packaged system originals remain intact. Uninstall restores that component's prior files and rejects changed installed binaries. Microphone uninstall also restores the saved pre-install audio settings. Completed uninstall receipts are archived so reinstall is possible. Failed installations attempt rollback and retain a recovery receipt.

To upgrade, first use the previous extracted component installer's `--uninstall`, then run the new installer's `--install`. Keep the old installer and receipt backups until the upgrade is verified. Installation refuses an existing receipt rather than overwriting it. The touchpad update retains the working PS/2 binary and adds `~/config/non-packaged/add-ons/input_server/devices/mouse`; uninstall also restores or removes that add-on as appropriate. Tapping follows Input preferences: zero sensitivity disables it, and a positive value enables light taps.

If a kernel override prevents boot, use Haiku's boot menu to disable user add-ons, then restore the affected module from its receipt backups or remove the override. Keep a known-good boot option available. Bluetooth has a marked background command in UserBootscript; removing it disables automatic bootstrap.

## Rebuild the distributions

From the repository root on Windows with Python 3.10 or newer:

```powershell
python installer/build_installers.py
```

This packages the revision-specific artifacts, relevant source/patches, licenses and checksums into four ZIPs under `dist/`. It does not compile drivers. See [BUILD.md](../BUILD.md), [Bluetooth notes](../bluetooth/README.md), and the development log for source baselines and compilation.

Validation on the target includes ZIP extraction, shell syntax, checksum verification, installation of all four components, and webcam uninstall/reinstall. Source and firmware licenses accompany each applicable package. No recorded audio, camera images, browser data or pairing keys are included.

For release 2026.10.23.1, all four ZIPs passed checksum and shell validation on the X270. Both pointing-device payloads match the physically tested installation. The included source rebuilt on Haiku and passed 22 gesture checks. `test_trackpoint_bundle.py` additionally exercised the combined installer's install/uninstall, changed-file refusal and second-file failure rollback in isolated filesystem paths, without replacing live drivers. Run it on the matching Haiku target as `python3 installer/test_trackpoint_bundle.py /path/to/dist` with all four ZIPs and their `SHA256SUMS` present. Results are in [package-validation.log](../trackpoint-touchpad/validation/package-validation.log).
