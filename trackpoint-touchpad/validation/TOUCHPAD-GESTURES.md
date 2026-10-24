# Touchpad gesture validation, 2026-10-23

Target: ThinkPad X270, Synaptics PS/2 touchpad and IBM TrackPoint; Haiku R1 beta6 `hrev59866+92` x86_64. Hardware specs reported minimum pressure 30, real maximum 100, maximum 200, and area 1432,1368–5532,4508. Tap sensitivity was already 20; changing the preference alone had not fixed tapping.

## Final deployed components

| Component | SHA-256 |
| --- | --- |
| Existing working PS/2 module, unchanged | `3266c606dc52a34474ac93acb84c1028d43ec88d9321ed30b17655ed9b44ccff` |
| Final gesture-only mouse add-on | `58fef96a647be067f107be0b2c9479feac7eff4bf26954c633fb50f09d2e159c` |

The final add-on was built directly on the X270, installed as a user non-packaged override, and loaded by input_server. Kernel image enumeration confirmed the original PS/2 module remained loaded. Touchpad, TrackPoint and keyboard enumerated as running. Temporary per-event logging was removed before the final binary was built and loaded.

## Checks

`gesture_test.cpp` exercises the modified movement conversion on Haiku. All 22 checks pass: a light tap at minimum pressure; single-, two- and three-finger taps; middle/right button release with no subsequent hardware event; two-finger scrolling and uneven lift-off; resumed movement after all contacts lift; physical click-dragging; disabled tapping; and long or moving gestures that must not click. The output is preserved in `gesture-test.log`.

The user confirmed **“Movement and taps work”**, followed by **“Middle-click and scrolling both work”** after testing a three-finger tap and two-finger scrolling while lifting one finger before the other. These are physical confirmations, not deductions from synthetic tests.

An earlier candidate also changed the kernel decoder and failed the physical pointer-movement test. It was rolled back. That kernel candidate and its binaries are excluded from the public package; the final fix is confined to the input_server add-on.

Other touchpad models, Haiku revisions and long-term gesture reliability have not been validated. Package extraction, checksums, rebuild and installer lifecycle checks are recorded in `package-validation.log`.
