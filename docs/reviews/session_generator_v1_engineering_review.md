# SESSION_GENERATOR_V1 engineering review

Date: 2026-09-10. This is the independent bounded engineering delta review,
not the required deep final-reviewer audit.

## Decision

The initial independent delta review returned PASS. Its result is retained as
pre-repair evidence, but it did not detect the three repairable blockers found
by the subsequent one deep final audit. Its authorized bounded repair review
returned `BOUNDED_FINAL_REPAIR_REVIEW=PASS`; the engineering lifecycle is
`SESSION_GENERATOR_V1=PASS`. No second broad final review was run.

## Scope and evidence

The review covered the C generator/API and complete-history scan, TUI flow,
Android model/migration/repository/engine/preview/acceptance/editor paths, V3
Python and Android producers/readers, transport precedence, shared fixtures,
and tests. It confirmed the one-policy source, C/Kotlin fixture agreement,
deterministic scoring/coverage and time semantics, role-separated exposure,
observed-only load anchoring and actual-only bridge, MAX/assistance exclusions,
bounded ownership, additive Android planning migration, normal acceptance, and
V3 strictness, idempotency, conflict rollback, and legacy compatibility.

Final durable evidence passed: Meson 45/45; named ASan/UBSan 4/4; strict C17
public-header checks; deterministic policy, fixture and knowledge generation;
JSON/import and policy validators; Android 75 tests with zero failures/errors
and one known unavailable external real-v9 fixture skip; and `assembleDebug`. The new
structural Android v10 planning-migration test executed and passed. Generated C,
fixtures, knowledge data, merged assets and packaged assets were byte-identical
where checked. The real desktop v11 database logical hash/counts and protected
catalog/schema files were unchanged; the index was empty.

No real app upgrade/install and no hardware MTP exercise were performed. The
post-repair matrix is retained under `/tmp/trainlog-session-generator-v1/final-validation/`;
the parent will create `final-complete.patch` and `final-manifest.json` after
its final normal checks. Preservation reports schema v11, integrity `ok`,
logical SHA-256 `5bff76850581cc3abafd583377a5de2c16d6313607a4cacb812376cfebd36cce`,
baseline equality, ten unchanged protected catalog/format files, and an empty
Git index.

## Contract precision

An empty proposal cannot be accepted. A nonempty partial proposal retains its
shortage warnings and may enter normal draft editing. Normal completed history
still requires actual work; shortage is not a scientific rejection.

## Final-audit repair state

The final audit initially failed on V3 timestamp admission/publication,
current-document V2/V3 contradictions including the rest bound, and incomplete
shared full-output parity assertions. The bounded timestamp repair and the
16-case full normalized C/Kotlin fixture repair are implemented with focused
passing evidence. The bounded repair review verified all three repairs as
`BOUNDED_FINAL_REPAIR_REVIEW=PASS`. These repairs do not erase the initial audit
history and did not authorize another broad final-reviewer run.

## Remaining lifecycle work

The one deep final-reviewer audit has already run. Its bounded repair review and
the final normal validation matrix closed the lifecycle:
`SESSION_GENERATOR_V1=PASS`. The policy remains separately canonical/frozen.

## Retained limits

Known chest and calf coverage shortages remain explicit rather than inferred.
The policy heuristics do not measure recovery; an explicit MAX is not a 1RM and
never supplies a numeric target. Hardware MTP, installed-app migration, and a
real Android v9 fixture were not available for this validation. The new
structural v10 planning-migration test did execute and pass.
