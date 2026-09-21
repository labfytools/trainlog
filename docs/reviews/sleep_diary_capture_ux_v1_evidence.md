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

Deployment-specific commit, bundle and process evidence is added only after the
authorized private service switch.
