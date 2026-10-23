# TrackPoint drag validation, 2026-10-22

Hardware: ThinkPad X270, Synaptics 8.2, pass-through IBM TrackPoint, three physical stick buttons. OS: Haiku R1 beta6 `hrev59866+92`, x86_64.

| Test | Button-down source | Held movements retaining left button | Held movements losing left button |
| --- | --- | ---: | ---: |
| Original driver, TrackPoint | Touchpad (`subtype=2`), `clicks=1` | 0 | 32 |
| Original driver, USB mouse | Mouse (`subtype=1`), `clicks=1` | 72 | 0 |
| Fixed driver, first TrackPoint drag | Mouse (`subtype=1`), `clicks=1` | 21 | 0 |

All later held-button sequences in `after-events.log` also retained their pressed mask. Releases returned to zero. The user reported that icon dragging, normal clicks, and right-clicks all worked correctly after reboot.

Before:

```text
DOWN when=3089636933 buttons=1 previous=0 clicks=1 subtype=2 where=329,35
MOVE when=3089987135 buttons=0 previous=1 clicks=0 subtype=1 where=330,36
```

After:

```text
DOWN when=57084592 buttons=1 previous=0 clicks=1 subtype=1 where=460,200
MOVE when=57195181 buttons=1 previous=1 clicks=0 subtype=1 where=460,200
UP when=57851930 buttons=0 previous=1 clicks=0 subtype=1 where=317,135
```

The kernel trace confirmed that standard mouse decoding received `packet=09 buttons=1 clicks=1` at press, retained the button in movement headers, and received a stationary `packet=08 buttons=0` release. Full pointer-only traces and the successful build log are included here. The diagnostic input filter observed messages without modifying them and was removed afterward.

Source comparison:

- [Haiku Synaptics handler](https://github.com/haiku/haiku/blob/master/src/add-ons/kernel/bus_managers/ps2/ps2_synaptics.cpp) reports the extended buttons through the touchpad path.
- [Haiku mouse input device](https://github.com/haiku/haiku/blob/master/src/add-ons/input_server/devices/mouse/MouseInputDevice.cpp) uses each device's movement button mask when building application events.
- [Linux Synaptics handler](https://github.com/torvalds/linux/blob/master/drivers/input/mouse/synaptics.c), `synaptics_report_ext_buttons()`, routes capability-marked stick buttons through the pass-through interface and preserves reports on firmware 8.1/8.2 when ExtBit is absent. This informed the hardware handling; no Linux code was copied.

The fix changes the PS/2 bus manager. Tracker and input_server behavior were left unchanged.
