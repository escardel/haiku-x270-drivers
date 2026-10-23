# ThinkPad X270 drivers for Haiku

Combined TrackPoint, touchpad, camera, and microphone development checkpoint for Haiku R1 beta 6, `hrev59866+92`, x86_64 on a Lenovo ThinkPad X270. Camera and microphone measurements are in [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md); [trackpoint-touchpad/README.md](trackpoint-touchpad/README.md) records the PS/2 work and its validation status.

The TrackPoint/touchpad directory includes the exact five-file PS/2 working-tree patch from the X270. Its SHA-256 matched the previously published standalone input-driver repository at this checkpoint. That patch has not been independently built and validated. The existing standalone repository is preserved.

## Verified on the X270

- Built-in SunplusIT camera (`5986:2118`) supplies real 640×480 frames through Media Kit and Firefox. The final combined Firefox test delivered 258 video frame callbacks in 15 seconds. The UVC driver selects USB alternate 0 after capture stops; syslog confirms this, while physical LED state has not been observed after the change.
- Internal Realtek ALC298 microphone, via Intel HDA (`8086:9d71`, codec `10ec:0298`, Lenovo subsystem `17aa:5062`), records through ADC 9. Firefox recorded a 3.003-second audio file at 48 kHz with nonzero decoded samples (AC RMS 0.001328). A second capture succeeded and released the Media Kit input afterward.
- Media services remained running after the combined test. A desktop mixer crash report predates the patched mixer override; no new report appeared during these tests.

## Source and binaries

- `patches/haiku/`: five patches against the X270's Haiku source tree, starting after commit `b33d698431`. These include HDA selector dispatch, controller lifecycle, X270 ADC 9 selection, SoundRecorder start/stop, and `BMediaRecorder` time-source producer start/stop.
- `patches/uvc/`: two patches against `atomozero/haiku-uvc-webcam` 0.8.1, starting after commit `6885e78`.
- `trackpoint-touchpad/patches/`: the current PS/2 TrackPoint and touchpad working-tree diff, plus its research and recovery notes.
- `artifacts/haiku-x86_64-hrev59866+92/`: binaries deployed on the test machine. They are tied to this Haiku build and should not be installed on a different revision without rebuilding.
- `media_capture.cpp`, `media_connections.cpp`, `media_input_gain.cpp`, `media_settings.cpp`, and the local HTML pages are focused diagnostic tools. `recorder_start_shim.cpp` is an older diagnostic interposer; the working Firefox test used the patched `libmedia.so` **without** this shim.

The mixer override was built from the X270's Haiku source tree, whose `AudioMixer::_AutoStart()` already checks for a null mixer output. The packaged mixer that crashed lacks that check. No new mixer source patch was required.

## Current installation on the X270

```text
/boot/home/config/non-packaged/add-ons/kernel/drivers/bin/hda
/boot/home/config/non-packaged/add-ons/media/aukey_webcam_v4.media_addon
/boot/home/config/non-packaged/add-ons/media/mixer.media_addon
/boot/system/non-packaged/lib/libmedia.so
/boot/home/config/non-packaged/bin/SoundRecorder-X270
```

The packaged originals remain available. Candidate binaries and backups are under `/boot/home/x270-camera-mic/`. The saved Media Kit settings file sets HDA input parameter 140 to 12 dB on both channels and parameter 142 to selector value 5 (the internal microphone). The settings file was backed up at `/boot/home/x270-camera-mic/backups/multi_audio_settings-before-gain` before editing.

To roll back the Media Kit library, remove `/boot/system/non-packaged/lib/libmedia.so` and restart `x-vnd.haiku-media_server` and `media_addon_server` or reboot. Remove any other non-packaged override at the paths above to return to the packaged component. The HDA override should be removed from the Haiku boot menu's disable user add-ons mode if SSH is unavailable after a fault.

## Open validation

- Reboot validation of the final `libmedia.so` override and persisted input settings has not yet been performed.
- Long-term camera stability remains uncertain: the 45-second Media Kit test delivered 1,026 frames but logged intermittent xHCI transfer errors.
- The physical camera LED should be checked after capture stops. The USB idle alternate transition is confirmed in syslog.
- The browser recording is non-silent and time-varying, but its acoustic content has not been heard by a person. An earlier native capture detected a 1 kHz tone played over the laptop speaker.
