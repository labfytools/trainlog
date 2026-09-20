# TRAINLOG_SYNC_USB_DRIVE_ANDROID_V1 manual validation

Automated tests use isolated directory and fake Drive transports. The first
live validation must use a disposable namespace only:

```text
Trainlog/Sync-Test/<run-id>/
```

It must never target `Trainlog/Sync`, `Trainlog/AI`, `Documents/Trainlog` from a
real user installation, or either SQLite database.

## Preconditions

1. Create a fresh UUIDv4-based `<run-id>` and an empty private Drive folder.
2. Configure a disposable rclone remote/path for that exact folder.
3. In a debug/test Android installation, select that same folder through the
   system document picker.
4. Use disposable desktop and Android databases containing one synthetic
   preparation and no real account data.
5. Record hashes and stable IDs before transport; never record credentials,
   URI grants, tokens or authorization headers.

## Procedure

1. Desktop publishes artifacts, verifies the uploaded bytes, then publishes
   `manifest.json` and the generation reference last.
2. Android consumes the generation, confirms the preparation exactly once and
   publishes its correlated ACK plus one synthetic completed-session
   generation.
3. Desktop consumes that generation, confirms one History row, one completed
   Program execution and one acknowledged preparation delivery, then publishes
   its ACK.
4. Replay the same Drive objects and confirm every count is unchanged.
5. Repeat the preserved facts over fake/isolated USB and confirm no duplicate
   delivery, execution session, History row or ACK.
6. Interrupt one upload before the manifest and confirm neither peer mutates.
7. Replace one artifact with a same-size SHA-mismatching payload and confirm
   rejection before mutation.
8. Stop network access after USB commit and confirm USB remains committed while
   Web reports the Drive mirror failure.
9. With SyncScreen closed and the user-enabled USB foreground notification
   visible, start a Web run and confirm the full request/generation/import/ACK
   conversation without touching Android.

## Cleanup

Remove only `Trainlog/Sync-Test/<run-id>/` and the two disposable databases.
Revoke the Android folder grant and remove the disposable rclone configuration.
Retain the redacted run report, generation/ACK identities and SHA-256 evidence.

Live status for this implementation checkout: `NOT_RUN`; no private Drive,
physical phone or real user data was accessed by automated validation.
