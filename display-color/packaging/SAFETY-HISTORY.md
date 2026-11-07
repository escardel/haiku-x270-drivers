# RGB saturation safety history

## Current version: 1.1.1

The user confirmed visible RGB adjustment using the complete application's
sliders after correcting CSC staging, readback and frame latching. Contrast,
gamma and the blue-light filter and scheduling remain present.

That attended preview exposed a separate cleanup defect: the guard killed the
frontend while it held the driver's nonrecovering lock. Rollback waited until
the abandoned lock was released after verifying that the app was dead and the
guard was the sole waiter. The original hardware snapshot was then restored.

Version 1.1.1 removes frontend hardware access during previews. The frontend
sends temporary settings to the independent guard, which performs the hardware
operations and rollback in the same process. Killing the frontend cannot
abandon its driver lock. Stale and expired adjustment requests are rejected.
A native timeout test completed with successful rollback and a free lock.

The RGB correction flushes posted writes through pipe control instead of CSC
data, checks coefficients before arming, and waits for frame latching before
later reads. Simulated tests model read-disarming and visible matrix latching;
a deliberately regressed candidate fails them. The earlier brief blackout
cannot be conclusively attributed to a single cause. Physical sleep/wake and
other Haiku revisions remain untested. Login startup is still disabled on
the development machine.

## Earlier brief blackout report

After version 1.1.0 was installed and started, the user reported a screen
blackout lasting a few seconds, beginning a few seconds after the app opened.
The user had not touched any controls. Its cause remains unknown.
The application was stopped and startup remains disabled. With the current
saved settings and hardware state, an independent read-only check found that
all 256 palette entries and relevant control bits already matched the app's
neutral-RGB, filter-off update path; that path would make no register writes
in that state. This does not establish what happened during the blackout.

The system log also records a display power-on event following package
activation, but it does not establish the reason or exact relationship to the
reported blackout. Packaging and publication were paused during investigation.

The background refresh runs every two seconds. The original implementation
could write the plane control and re-arm its surface when only palette entries
needed changing. A candidate update writes only differing palette entries and
re-arms the plane only when its control or gamma mode changes. Simulated
register tests cover stable refreshes, palette repair, filter adjustments,
RGB transitions, mode recovery, inactive pipes and panel self-refresh safety.
This removes an unnecessary operation; it does not establish that operation as
the cause of the reported blackout.

The rebuilt candidate subsequently passed an attended guarded startup preview.
The user let the no-change rehearsal expire and clicked OK approximately
10.7 seconds into the app preview. The independent guard recorded successful
child termination and snapshot restoration. The diagnostic trace recorded the
initial application and five two-second refreshes, all with zero mismatches
and zero changed pipes. Saved settings remained byte-for-byte unchanged.
This verifies that startup path under the current settings; the earlier
blackout's cause, package activation, and physical sleep/wake remain unverified.
This first no-change startup preview did not establish visible RGB output;
the later full-app RGB confirmation is recorded above.

## Resolution and attended confirmation

The production backend now enables plane-gamma bypass whenever it enables RGB
CSC, matching the revised preview backend. The user visually confirmed the
identity test, each independent 105% red/green/blue preview, and the 15% blue-light
filter, clicking OK on each. The 5% RGB changes were subtle; the blue-light
filter was clearly visible and behaved as expected. This is not colorimetric
calibration or validation of every slider value.

The native confirmation guard remains available as `SafeDisplayTest`. Normal
application controls apply immediately; the guarded test tool is separate.
Both reuse the same hardware backend. The app restores its captured plane-gamma
bit when returning RGB saturation to neutral during a session, and refuses CSC
while panel self-refresh is active. Startup remains opt-in and is still disabled
on the test machine. Publication of the original unsafe binaries is not approved;
only the corrected, rebuilt version may be packaged.

The chronology below is retained for transparency and describes earlier states.

On November 2, 2026 UTC the user reported that the display went black during
live tests of the experimental RGB saturation/blue-light build. Register
readback and scripted control tests passed, but they did not validate visible
output. The test's final reset disabled the CSC matrix and restored the previous
tone curve; the user subsequently reported that the display had recovered.

The original installed binary and saved settings were restored. The source-tree
executable was also restored so the existing login helper cannot start the
experimental build. Contrast remains 100% and gamma 0.80. New source changes
are retained for investigation, not approved for installation or publication.

Do not resume hardware CSC tests without an independent recovery mechanism and
explicit user agreement. The exact black-screen cause has not been established.

The user subsequently authorized attended tests with timed confirmation and
requested that login startup remain disabled. The original startup symlink is
backed up as `backup-before-rgb-saturation/startup-disabled-during-testing`,
outside the boot launch directory. `SafeDisplayTest` is a separate diagnostic
tool with an independent rollback process; it does not install the experimental
app, save candidate settings, or enable startup. This does not lift the package
or publication block. See `SAFE-TESTING.md`.

The attended identity-only CSC test also produced a black screen. The user
waited and confirmed that the independent 12-second rollback restored output.
The diagnostic was stopped and the original app reopened, without reenabling
startup. The next diagnostic candidate additionally bypasses plane gamma,
matching the plane-control setup used in Linux v6.12's `skl_plane_ctl()`.
This is a hypothesis, not a confirmed fix, and no candidate runs automatically.

At that stage, the user visually confirmed the revised identity test and clicked OK.
The guard log records that confirmation; active pipe A read back `0xc4802000`.
Independent RGB changes and the complete installed app still required testing
at that stage. The subsequent RGB confirmation and guard correction are
recorded at the top of this document.
