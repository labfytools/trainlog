# Sleep Diary capture UX and black-screen correction evidence

## Scope

This record covers the focused 0.1.4 Web correction and presentation tranche.
It does not change Sleep Diary domain semantics, desktop schema v29, Android
schema v26, synchronization, publication lifecycle, PDF semantics or any
frozen exchange format.

## Runtime diagnosis

The deployed bundle was run through `http://trainlog.perf/analyse?section=sleep`
in Firefox 156 with an isolated synthetic XDG data directory. The browser
reported `isSecureContext === false` and did not expose
`crypto.randomUUID`. Clicking **Ajouter** for a bedtime event raised:

```text
TypeError: crypto.randomUUID is not a function
```

The stack entered the `addEvent` state updater in
`web/src/routes/SleepDiaryWorkspace.tsx`. React then unmounted the application
tree, leaving an empty body. The router remained on the Sleep Analyse URL.
There was no mutation request, HTTP status or response body because the
exception happened while constructing the optimistic event. No unhandled
promise rejection was present. Medication intake identity construction used
the same unsupported API and had the same latent failure.

## Correction

Sleep event and medication-intake identities now use `crypto.getRandomValues`,
set the UUIDv4 version and RFC 4122 variant bits explicitly, and never fall back
to `Math.random`. A section-local React error boundary keeps the Trainlog shell,
header and navigation mounted and offers a retry card for unexpected future
render errors. This boundary is defense in depth; it is not the black-screen
fix.

The capture presentation now uses the existing Trainlog panel, quiet-action,
primary-action and danger-action vocabulary. Dates, lifecycle badge and local
save state form one night header. Morning/day appreciations, event creation,
timeline, intake creation, notes and actions have explicit hierarchy. The
medication catalog is a compact list whose creation form opens on demand and
whose saved item becomes immediately selectable for an intake.

## Validation evidence

- Web TypeScript, all Vitest tests and the Vite production build pass.
- Component regressions cover bedtime, sleep, long awakening, final get-up,
  nap and sleepiness, stable entry identity, advanced revisions, medication
  creation, intake creation, edit and delete.
- Real Firefox against the production C server created two medications, two
  intakes and five events, then reloaded the same durable entry and revision.
- Every captured mutation returned HTTP 200 with its expected JSON identity or
  revision response. The console error and unhandled-rejection collections
  remained empty.
- The responsive layout had no document-level horizontal overflow.
- The retained post-reload screenshot is
  [`evidence/sleep-diary-v1-final-firefox.png`](evidence/sleep-diary-v1-final-firefox.png).

## Private deployment evidence

The functional bundle was built from
`915728cec4ffbb2c90eb3f4b47d349a6ebd17fe9`. Its embedded desktop binary has
SHA-256 `e45c2fd8807dd9c28128206ee57e2869b64726397ae6072d521a903d3d4afdc9`.
Before the switch, the live schema-v29 database was backed up as
`trainlog-before-915728c97f420f824c7831024d61ee9c08645321.db`, SHA-256
`9579e1365164a082e328c4c5bc1ffc7b45bf234a1f5bf1812842837c8ecc05f7`;
its integrity check was `ok` and its foreign-key check was empty.

The versioned installation is
`~/.local/share/trainlog/installations/915728cec4ffbb2c90eb3f4b47d349a6ebd17fe9`.
After the atomic user-bin switch and service restart,
`trainlog-web.service` was active as PID `2322898`, and
`/proc/2322898/exe` resolved to that installation's `libexec/trainlog`.
The deployed executable and footer both reported version `0.1.4`.

Real Firefox 156 then used the actual deployed profile to create `TestMed A`
5 mg and `TestMed B` 25 mg intakes at 22:00, bedtime at 22:30, sleep from
23:00 to 03:00, a long awakening from 03:00 to 03:30, sleep from 03:30 to
06:30 and final get-up at 07:00. Reload retained entry
`sl_71b9c7a5-cabe-4d22-86fc-bde8a34590f9` at revision
`slr_c1e8f641-4db7-43f1-8a9c-e7ef15b2c1df`, with five events and two
intakes. All nine mutations returned HTTP 200. Browser console errors,
network fetch errors and unhandled rejections were all zero. The exact
390×844 viewport had zero document overflow. No main merge, tag or release
was created.
