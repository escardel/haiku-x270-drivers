# ThinkPad X270 TrackPoint and touchpad support for Haiku

TrackPoint and touchpad section of the combined X270 driver checkpoint.

## Current state

The X270's existing PS/2 edits have been exported as [patches/x270-ps2-working.patch](patches/x270-ps2-working.patch). They are the uncommitted working-tree diff from `/boot/home/haiku` on branch `x270-hda-mic`; the diff is unchanged from the earlier standalone repository snapshot based on commit `78dbf767662993f34e503dc3907ac0a5923c49ef`. The patch contains clickpad behavior changes and extensive diagnostics. It has **not** been independently built or validated from this repository.

Next, separate the functional changes from diagnostics, build the PS/2 module, and document hardware results. Preserve the machine's existing checkout and unrelated changes while doing so.

## Hardware and development target

- Lenovo ThinkPad X270
- Haiku R1 beta6, hrev59866+92, x86_64 (last observed)
- Haiku source checkout: `/boot/home/haiku`

## Recovery checklist

1. Review the exported PS/2 patch against its exact base commit.
2. Split functional fixes and diagnostic logging into reviewable commits.
3. Build and test on the X270; record exact results here.

No driver behavior is claimed as verified by this repository yet.

See [RESEARCH.md](RESEARCH.md) for upstream source findings and the next hardware checks.
