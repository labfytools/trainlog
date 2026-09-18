# Web Sessions V1

The Sessions route implements three distinct views:

- Preparation: manual preparations and immutable AI proposals;
- Resume: lifecycle-backed execution drafts, continued only on Android;
- History: completed sessions, including explicit unknown end times.

Manual preparation supports a title, session type, optional calendar date,
note, catalogue selection, duplicate occurrences, keyboard reordering,
equipment/load semantics and profile-compatible targets. Draft saves may remain
incomplete. Marking ready validates executable content; preparing for Android
then creates a distinct immutable delivery and reserved execution identity.

Opening or deriving from an AI proposal never accepts, starts, deletes or
rewrites the source. Derivation records the exact proposal identity and imported
payload fingerprint. The Android screen labels manual preparations separately
and refuses to overwrite an active draft.

This V1 does not provide live Web capture, historical correction, automatic
training generation, Programs, Analysis or the standalone Exercises route.
