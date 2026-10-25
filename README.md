# ThinkPad X270 drivers for Haiku

TrackPoint, touchpad, camera, microphone and Bluetooth development for Haiku R1 beta 6, `hrev59866+92`, x86_64 on a Lenovo ThinkPad X270. Measurements and limitations are in [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md).

## Download installers

**Experimental, untested pre-release:** [version-unlocked X270 installers](https://github.com/escardel/haiku-x270-drivers/releases/tag/v2026.10.25.1-unlocked) automatically bypass the Haiku revision check on any x86_64 revision. Compatibility is unknown; they may fail or crash. See the pre-release description for installation, removal and boot recovery.

[GitHub Releases](https://github.com/escardel/haiku-x270-drivers/releases) contains **four separate installers**: TrackPoint/touchpad, webcam, microphone/audio, and Bluetooth. Each has checksums, backups and uninstall. The regular releases reject other Haiku revisions/architectures. See [installation and recovery instructions](installer/README.md).

Bluetooth AX210 (`8087:0032`) now loads signed operational firmware and completes device discovery through Haiku's Bluetooth Kit. Pairing, HID, BLE and Bluetooth audio remain untested. See [Bluetooth implementation and validation](bluetooth/README.md).

The TrackPoint/touchpad directory includes the original five-file PS/2 patch and the subsequent TrackPoint drag fix. The resulting module was built, installed, rebooted, and tested on the X270. The existing standalone input-driver repository is preserved.

Release **2026.10.23.1** adds the tested native touchpad gesture add-on to the combined TrackPoint/touchpad installer: one-finger tap-to-click, three-finger tap middle-click, and stable two-finger scrolling through uneven finger lift-off. The working PS/2 module is unchanged. See [gesture source and build instructions](trackpoint-touchpad/gesture-addon/README.md) and [validation](trackpoint-touchpad/validation/TOUCHPAD-GESTURES.md).

## Verified on the X270

- Built-in SunplusIT camera (`5986:2118`) supplies real 640Ã—480 frames through Media Kit and Firefox. The combined Firefox test delivered 258 video frame callbacks in 15 seconds. The UVC driver selects USB alternate 0 after capture stops; the user confirmed the camera light is off when unused.
- Internal Realtek ALC298 microphone, via Intel HDA (`8086:9d71`, codec `10ec:0298`, Lenovo subsystem `17aa:5062`), records through ADC 9. Firefox recorded a 3.003-second audio file at 48 kHz with nonzero decoded samples (AC RMS 0.001328). A second capture succeeded and released the Media Kit input afterward.
- Media services remained running after the combined test. A desktop mixer crash report predates the patched mixer override; no new report appeared during these tests.
- Physical TrackPoint button dragging in Tracker now works. Before the fix, all 32 held movement messages lost the left-button state; afterward, captured drag sequences preserved it on every movement. The user confirmed icon dragging, normal clicks, and right-clicks work correctly.
- The user confirmed touchpad movement and taps, three-finger middle-click and two-finger scrolling without pointer jumps. The gesture add-on also passes 22 native checks.
- The final module set passed a reboot. The Media Kit library override and microphone settings loaded, followed by 57 valid camera frames and 71 audio buffers in separate three-second captures.

## Source and binaries

- `patches/haiku/`: five patches against the X270's Haiku source tree, starting after commit `b33d698431`. These include HDA selector dispatch, controller lifecycle, X270 ADC 9 selection, SoundRecorder start/stop, and `BMediaRecorder` time-source producer start/stop.
- `patches/uvc/`: two patches against `atomozero/haiku-uvc-webcam` 0.8.1, starting after commit `6885e78`.
- `trackpoint-touchpad/patches/`: the original PS/2 changes followed by the tested button-routing fix. Validation logs, research, and recovery notes are included.
- `artifacts/haiku-x86_64-hrev59866+92/`: binaries deployed on the test machine. They are tied to this Haiku build and should not be installed on a different revision without rebuilding.
- `media_capture.cpp`, `media_connections.cpp`, `media_input_gain.cpp`, `media_settings.cpp`, and the local HTML pages are focused diagnostic tools. `recorder_start_shim.cpp` is an older diagnostic interposer; the working Firefox test used the patched `libmedia.so` **without** this shim.

The mixer override was built from the X270's Haiku source tree, whose `AudioMixer::_AutoStart()` already checks for a null mixer output. The packaged mixer that crashed lacks that check. No new mixer source patch was required.

## Current installation on the X270

```text
/boot/home/config/non-packaged/add-ons/kernel/drivers/bin/hda
/boot/home/config/non-packaged/add-ons/kernel/bus_managers/ps2
/boot/home/config/non-packaged/add-ons/input_server/devices/mouse
/boot/home/config/non-packaged/add-ons/media/aukey_webcam_v4.media_addon
/boot/home/config/non-packaged/add-ons/media/mixer.media_addon
/boot/system/non-packaged/lib/libmedia.so
/boot/home/config/non-packaged/bin/SoundRecorder-X270
```

The packaged originals remain available. Candidate binaries and backups are under `/boot/home/x270-camera-mic/`. The saved Media Kit settings file sets HDA input parameter 140 to 12 dB on both channels and parameter 142 to selector value 5 (the internal microphone). The settings file was backed up at `/boot/home/x270-camera-mic/backups/multi_audio_settings-before-gain` before editing.

To roll back the Media Kit library, remove `/boot/system/non-packaged/lib/libmedia.so` and restart `x-vnd.haiku-media_server` and `media_addon_server` or reboot. Remove any other non-packaged override at the paths above to return to the packaged component. For PS/2, restore `/boot/home/x270-camera-mic/backups/ps2-before-drag-fix` to the installed bus-manager path and reboot to return to the previous custom module. If SSH is unavailable after a kernel fault, boot with user add-ons disabled and remove the failing kernel override before restarting normally.

## Open validation

- Long-term camera stability remains uncertain: the 45-second Media Kit test delivered 1,026 frames but logged intermittent xHCI transfer errors.
- The browser recording is non-silent and time-varying, but its acoustic content has not been heard by a person. An earlier native capture detected a 1 kHz tone played over the laptop speaker.

Upstream code and binaries retain their original licenses; see `licenses/`.

## Playback and Google Meet configuration

The user confirmed playback and Google Meet work after two configuration repairs: select HD Audio as the default Media Kit output, and remove a Firefox user-agent override impersonating Chrome on Windows. These repairs changed configuration, not driver binaries. Backups remain on the X270.

A local 640x480 WebRTC loopback encoded/sent 190 video frames, decoded 189, and received 598 audio packets / 574,080 samples over twelve seconds. All twelve sampled camera snapshots differed. A subsequent 1280x720 attempt stalled; that mode is not validated.

`media_graph.cpp`, `media_output_setup.cpp`, and `audio_playback_probe.cpp` diagnose the audio graph and output selection. `restore_firefox_identity.py` targets this X270's specific profile and should run only after Firefox exits. `firefox-loopback-user.js` belongs only to an isolated diagnostic profile. The localhost test server does not require external network access.
