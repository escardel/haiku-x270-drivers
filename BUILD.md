# Reproducing the X270 checkpoint

The tested target is Haiku R1 beta6 `hrev59866+92`, x86_64. The Haiku source base is commit `b33d698431`; the UVC base is `6885e78` in [atomozero/haiku-uvc-webcam](https://github.com/atomozero/haiku-uvc-webcam), version 0.8.1.

## Source order

In the Haiku checkout, apply `patches/haiku/0001` through `0005` with `git am`. Then apply `trackpoint-touchpad/patches/x270-ps2-working.patch`, commit the resulting PS/2 baseline, and apply `trackpoint-touchpad/patches/0002-route-trackpoint-buttons.patch` with `git am`.

The final target branch is `x270-ps2-drag` at `b7ed43bcd2`. Its parent `e9dacb5c0f` preserves the original PS/2 changes, and `8ccb95ad00` is the final audio source commit.

In the UVC checkout, apply `patches/uvc/0001` and `0002` with `git am`, then run `make -j2`. The target branch is `x270-camera` at `3bda7e4`.

## Builds

The normal PS/2 build passed with:

```sh
LIBRARY_PATH=/boot/system/lib:/boot/system/develop/lib jam -j2 ps2
```

That environment lets host build tools find the installed `libbsd.so`. Without it, the compiler and linker succeeded but the `settype` metadata action failed.

The Media Kit library was linked from the source tree's compiled objects as follows, after `jam -j2 libmedia.so` compiled those objects but failed on unrelated build-environment dependencies:

```sh
g++ -shared -Wl,-soname,libmedia.so \
  -o /boot/home/x270-camera-mic/libmedia-X270.so \
  generated/objects/haiku/x86_64/release/kits/media/*.o \
  generated/objects/haiku/x86_64/release/kits/shared/RWLocker.o \
  -Lgenerated/objects/haiku/x86_64/release/kits/locale \
  -lbe -llocalestub -lstdc++
```

`link-hda.sh` records the HDA manual link used earlier in development. The patched SoundRecorder and mixer were compiled from the target Haiku source tree with the installed Haiku development headers and libraries. Their deployed binaries are preserved in `artifacts/`, along with HDA, PS/2, UVC, and libmedia. These artifacts match the tested revision and are not general Haiku packages.

## Focused checks

The touchpad gesture update builds independently of the kernel from `trackpoint-touchpad/gesture-addon/`. On Haiku `hrev59866+92` x86_64, run `bash build.sh` in that directory. Installed GCC and private development headers are required; the script builds the mouse add-on and runs its 22 gesture checks. `trackpoint-touchpad/patches/0003-touchpad-gestures.patch` contains the corresponding downstream changes. The packaged PS/2 module remains unchanged; rebuilding it is unnecessary for this update.

Build `media_capture.cpp` with `g++ -std=c++17 -o media_capture media_capture.cpp -lbe -lmedia`. Setting `X270_RECORDER_ONLY=1` disables the probe's explicit node starts, so an audio capture tests `BMediaRecorder::Start()` itself.

```sh
X270_RECORDER_ONLY=1 ./media_capture audio test-audio.raw 3
./media_capture video test-video.raw 3
```

The final post-reboot runs returned 71 audio buffers and 57 nonuniform video frames. `media_connections` reported zero remaining input connections after stop. Browser tests are preserved in the HTML files, and the pointer tests and human confirmation are documented under `trackpoint-touchpad/validation`.

Installation paths and rollback instructions are in the root README. Keep the packaged originals and existing backups available before deploying to another build.

## Configuration diagnostics

Compile `media_graph.cpp`, `media_output_setup.cpp`, and `audio_playback_probe.cpp` individually with `g++ -o TOOL TOOL.cpp -lbe -lmedia`. The graph probe is read-only; output setup selects the existing HD Audio consumer as default. Back up `/boot/home/config/settings/Media/MDefaultManager` before changing it.

After selecting the output, stop `x-vnd.haiku-media_server` using `launch_roster stop`, gracefully quit the actual `media_addon_server` PID with `hey -o PID quit`, then start `x-vnd.haiku-media_server` using `launch_roster start`. Check the graph for a connected mixer and advancing HDA clock afterward.

The Firefox identity repair is profile-specific. Exit Firefox before running it; it preserves the first backup and removes only `general.useragent.override`. Normal session restoration remains enabled. Capture permission bypass preferences are confined to the separate diagnostic profile.
