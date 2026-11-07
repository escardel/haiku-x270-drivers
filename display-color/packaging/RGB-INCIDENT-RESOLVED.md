# Corrected RGB candidate: 1.1.1

The user confirmed visible RGB adjustment using the complete app after the CSC
readback/latching correction. The earlier brief blackout remains unexplained;
the incident and limitations remain documented in `SAFETY-HISTORY.md`.

A separate preview-cleanup lock defect was found and corrected by moving all
preview hardware operations into the independent guard. Native timeout cleanup
completed with the original display restored and the driver lock free.

Version 1.1.1 may be packaged with these documented results. The old 1.1.0
artifacts must not be published as the corrected build. Login startup remains
disabled on the development machine.
