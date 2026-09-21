import { useEffect, useMemo, useRef, useState } from "react";
import {
  deleteSleepEntry,
  fetchSleepDiary,
  fetchSleepMedications,
  newSleepId,
  saveSleepEntry,
  saveSleepMedication,
  validateSleepEntry,
  sleepTimestamp,
  type MedicationIntake,
  type SleepEntry,
  type SleepEntryInput,
  type SleepEvent,
  type SleepEventType,
  type SleepMedication,
  type SleepQuality,
} from "../api/sleepDiary";
import type { AnalysisPeriod } from "../api/analysis";
import {
  buildSleepDiaryPdf,
  presentSleepDiaryPdf,
} from "../report/sleepDiaryPdf";

type Language = "fr" | "en";
const q: readonly SleepQuality[] = ["TB", "B", "Moy", "M", "TM"];
const pointTypes: readonly SleepEventType[] = [
  "bed_time",
  "final_get_up",
  "night_get_up",
  "daytime_sleepiness",
];
const copy = {
  fr: {
    agenda: "Agenda",
    edit: "Saisie et édition",
    morning: "Matin",
    day: "Journée / Soir",
    night: "Nuit du",
    bed_time: "Mise au lit",
    final_get_up: "Lever",
    night_get_up: "Lever nocturne",
    sleep: "Sommeil",
    nap: "Sieste",
    long_awake: "Long réveil",
    half_sleep: "Demi-sommeil",
    daytime_sleepiness: "Somnolence",
    sleepQuality: "Qualité du sommeil",
    wakeQuality: "Qualité du réveil",
    dayForm: "Forme de la journée",
    notes: "Traitement et remarques",
    add: "Ajouter",
    save: "Enregistrer",
    remove: "Supprimer",
    observations: "Observations",
    summary: "Synthèse factuelle",
    empty: "Aucune nuit enregistrée.",
    preview: "Prévisualiser",
    export: "Exporter PDF",
    medications: "Médicaments",
    addMedication: "Ajouter un médicament",
    medication: "Médicament",
    usualDose: "Dosage habituel",
    unit: "Unité",
    deactivate: "Désactiver",
    intake: "Ajouter une prise",
    time: "Heure",
    start: "Début",
    end: "Fin",
    dose: "Dosage",
    modify: "Modifier",
    validate: "Valider la journée",
    continue: "Continuer",
    saved: "Enregistré localement",
    saving: "Enregistrement…",
    registeredMedications: "Médicaments enregistrés",
    timeline: "Chronologie",
    addEventHeading: "Ajouter un événement",
    emptyEvents: "Aucun événement enregistré pour cette nuit.",
    addIntakeHeading: "Ajouter une prise",
    cancel: "Annuler",
    deleteEntry: "Supprimer la fiche",
    medicationEmpty: "Aucun médicament enregistré.",
    notesPlaceholder: "Notes utiles pour cette nuit…",
    statuses: {
      draft: "Brouillon",
      ready: "Prête à synchroniser",
      synchronized: "Synchronisée",
      modified: "Modifiée depuis la synchronisation",
    },
    qualities: ["Très bon", "Bon", "Moyen", "Mauvais", "Très mauvais"],
  },
  en: {
    agenda: "Diary",
    edit: "Entry editor",
    morning: "Morning",
    day: "Day / Evening",
    night: "Night from",
    bed_time: "Bedtime",
    final_get_up: "Final get-up",
    night_get_up: "Night get-up",
    sleep: "Sleep",
    nap: "Nap",
    long_awake: "Long awakening",
    half_sleep: "Half-sleep",
    daytime_sleepiness: "Sleepiness",
    sleepQuality: "Sleep quality",
    wakeQuality: "Wake quality",
    dayForm: "Day form",
    notes: "Treatment and notes",
    add: "Add",
    save: "Save",
    remove: "Delete",
    observations: "Observations",
    summary: "Factual summary",
    empty: "No recorded nights.",
    preview: "Preview",
    export: "Export PDF",
    medications: "Medications",
    addMedication: "Add a medication",
    medication: "Medication",
    usualDose: "Usual dose",
    unit: "Unit",
    deactivate: "Deactivate",
    intake: "Add an intake",
    time: "Time",
    start: "Start",
    end: "End",
    dose: "Dose",
    modify: "Edit",
    saved: "Saved locally",
    saving: "Saving…",
    registeredMedications: "Saved medications",
    timeline: "Timeline",
    addEventHeading: "Add an event",
    emptyEvents: "No events recorded for this night.",
    addIntakeHeading: "Add an intake",
    cancel: "Cancel",
    deleteEntry: "Delete entry",
    medicationEmpty: "No saved medications.",
    notesPlaceholder: "Useful notes about this night…",
    validate: "Validate day",
    continue: "Continue",
    statuses: {
      draft: "Draft",
      ready: "Ready to sync",
      synchronized: "Synchronized",
      modified: "Modified since synchronization",
    },
    qualities: ["Very good", "Good", "Average", "Bad", "Very bad"],
  },
} as const;

const clock = (iso: string) => (iso ? iso.slice(11, 16) : "");
const atClock = (value: string, startDate: string, endDate: string) => {
  if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(value)) return "";
  const hour = Number(value.slice(0, 2));
  const date = hour >= 18 ? startDate : endDate;
  const local = new Date(`${date}T${value}:00`);
  const offsetMinutes = -local.getTimezoneOffset();
  const sign = offsetMinutes >= 0 ? "+" : "-";
  const absoluteOffset = Math.abs(offsetMinutes);
  const offsetHours = String(Math.floor(absoluteOffset / 60)).padStart(2, "0");
  const offsetRemainder = String(absoluteOffset % 60).padStart(2, "0");
  return `${date}T${value}:00${sign}${offsetHours}:${offsetRemainder}`;
};
const offset = (iso: string, startDate: string) => {
  const start = new Date(`${startDate}T18:00:00`);
  return Math.max(
    0,
    Math.min(
      100,
      ((new Date(iso).getTime() - start.getTime()) / 86_400_000) * 100,
    ),
  );
};
const blank = (): SleepEntryInput => {
  const now = new Date();
  const start = new Date(now);
  if (now.getHours() < 18) start.setDate(start.getDate() - 1);
  const end = new Date(start);
  end.setDate(end.getDate() + 1);
  const date = (value: Date) => value.toLocaleDateString("en-CA");
  return {
    entry_id: "",
    expected_revision: null,
    night_start_date: date(start),
    night_end_date: date(end),
    created_at: sleepTimestamp(now),
    updated_at: sleepTimestamp(now),
    sleep_quality: null,
    wake_quality: null,
    day_form: null,
    treatment_and_notes: "",
    events: [],
    intakes: [],
  };
};

function QualityPicker({
  label,
  value,
  onChange,
  language,
}: {
  label: string;
  value: SleepQuality | null;
  onChange: (value: SleepQuality | null) => void;
  language: Language;
}) {
  return (
    <fieldset className="sleep-quality">
      <legend>{label}</legend>
      {q.map((item, index) => (
        <button
          type="button"
          key={item}
          aria-pressed={value === item}
          title={copy[language].qualities[index]}
          onClick={() => onChange(value === item ? null : item)}
        >
          {item}
        </button>
      ))}
    </fieldset>
  );
}

export function SleepDiaryWorkspace({
  period,
  language,
}: {
  period: AnalysisPeriod;
  language: Language;
}) {
  const t = copy[language];
  const [snapshot, setSnapshot] = useState<Awaited<
    ReturnType<typeof fetchSleepDiary>
  > | null>(null);
  const entries = snapshot?.entries ?? [];
  const [draft, setDraft] = useState<SleepEntryInput>(blank);
  const [eventType, setEventType] = useState<SleepEventType>("sleep");
  const [pending, setPending] = useState(false);
  const [error, setError] = useState("");
  const [eventStart, setEventStart] = useState("22:30");
  const [eventEnd, setEventEnd] = useState("23:30");
  const [publicationStatus, setPublicationStatus] =
    useState<SleepEntry["publication_status"]>("draft");
  const [saveState, setSaveState] = useState<"saved" | "saving" | "error">(
    "saved",
  );
  const [medications, setMedications] = useState<SleepMedication[]>([]);
  const [medName, setMedName] = useState("");
  const [medDose, setMedDose] = useState("");
  const [medUnit, setMedUnit] = useState("mg");
  const [editingMedication, setEditingMedication] =
    useState<SleepMedication | null>(null);
  const [medicationFormOpen, setMedicationFormOpen] = useState(false);
  const [intakeMedication, setIntakeMedication] = useState("");
  const [intakeTime, setIntakeTime] = useState("22:00");
  const [intakeDose, setIntakeDose] = useState("");
  const [intakeUnit, setIntakeUnit] = useState("mg");
  const identity = useRef<{ entry_id: string; revision_id: string } | null>(
    null,
  );
  const persistence = useRef<Promise<void>>(Promise.resolve());
  const range = useMemo(() => {
    if (period === "all") return {};
    const days = Number(period.slice(0, -1));
    const end = new Date();
    const start = new Date();
    start.setDate(end.getDate() - days + 1);
    return {
      startDate: start.toLocaleDateString("en-CA"),
      endDate: end.toLocaleDateString("en-CA"),
    };
  }, [period]);
  const reload = () =>
    Promise.all([
      fetchSleepDiary(range.startDate, range.endDate),
      fetchSleepMedications(),
    ])
      .then(([diary, catalog]) => {
        setSnapshot(diary);
        setMedications(catalog);
        if (identity.current === null) {
          const resume =
            diary.entries.find(
              (entry) =>
                entry.publication_status === "draft" ||
                entry.publication_status === "modified",
            ) ?? diary.entries[0];
          if (resume) {
            identity.current = {
              entry_id: resume.entry_id,
              revision_id: resume.revision_id,
            };
            setPublicationStatus(resume.publication_status);
            setDraft({
              ...resume,
              expected_revision: resume.revision_id,
              events: resume.events.map((event) => ({ ...event })),
              intakes: resume.intakes.map((intake) => ({ ...intake })),
            });
          }
        }
      })
      .catch(() => setError("sleep_diary_unavailable"));
  useEffect(() => {
    void reload();
  }, [period]);
  const edit = (entry: SleepEntry) => {
    identity.current = {
      entry_id: entry.entry_id,
      revision_id: entry.revision_id,
    };
    setPublicationStatus(entry.publication_status);
    setDraft({
      ...entry,
      expected_revision: entry.revision_id,
      events: entry.events.map((event) => ({ ...event })),
      intakes: entry.intakes.map((intake) => ({ ...intake })),
    });
  };
  const persist = (next: SleepEntryInput) => {
    setSaveState("saving");
    persistence.current = persistence.current
      .then(async () => {
        const current = identity.current;
        const result = await saveSleepEntry({
          ...next,
          entry_id: current?.entry_id ?? next.entry_id,
          expected_revision: current?.revision_id ?? next.expected_revision,
          updated_at: sleepTimestamp(),
        });
        identity.current = result;
        setPublicationStatus((status) =>
          status === "synchronized" || status === "modified"
            ? "modified"
            : "draft",
        );
        setSaveState("saved");
        setDraft((value) => ({
          ...value,
          entry_id: result.entry_id,
          expected_revision: result.revision_id,
        }));
      })
      .catch((reason: unknown) => {
        setSaveState("error");
        setError(
          reason instanceof Error
            ? reason.message
            : "sleep_diary_mutation_failed",
        );
      });
  };
  const mutate = (transform: (current: SleepEntryInput) => SleepEntryInput) =>
    setDraft((current) => {
      const next = transform(current);
      persist(next);
      return next;
    });
  const change = <K extends keyof SleepEntryInput>(
    key: K,
    value: SleepEntryInput[K],
  ) => mutate((current) => ({ ...current, [key]: value }));
  const addEvent = () => {
    setError("");
    const start = atClock(
      eventStart,
      draft.night_start_date,
      draft.night_end_date,
    );
    const end = pointTypes.includes(eventType)
      ? null
      : atClock(eventEnd, draft.night_start_date, draft.night_end_date);
    if (!start || (end !== null && new Date(end) <= new Date(start))) {
      setError("sleep_diary_invalid_time");
      return;
    }
    mutate((current) => ({
      ...current,
      events: [
        ...current.events,
        {
          event_id: newSleepId("sle"),
          type: eventType,
          start_at: start,
          end_at: end,
        },
      ].sort((a, b) => a.start_at.localeCompare(b.start_at)),
    }));
  };
  const changeEvent = (index: number, patch: Partial<SleepEvent>) =>
    mutate((current) => ({
      ...current,
      events: current.events.map((event, currentIndex) =>
        currentIndex === index ? { ...event, ...patch } : event,
      ),
    }));
  const selectMedication = (id: string) => {
    setIntakeMedication(id);
    const item = medications.find((med) => med.medication_id === id);
    setIntakeDose(item?.default_dose_value?.toString() ?? "");
    setIntakeUnit(item?.default_dose_unit ?? "mg");
  };
  const addIntake = () => {
    setError("");
    const medication = medications.find(
      (item) => item.medication_id === intakeMedication,
    );
    const takenAt = atClock(
      intakeTime,
      draft.night_start_date,
      draft.night_end_date,
    );
    if (!medication || !takenAt) {
      setError("sleep_diary_invalid_intake");
      return;
    }
    const dose = intakeDose === "" ? null : Number(intakeDose);
    if (dose !== null && !(dose > 0)) {
      setError("sleep_diary_invalid_intake");
      return;
    }
    const intake: MedicationIntake = {
      intake_id: newSleepId("mdi"),
      medication_id: medication.medication_id,
      medication_name: medication.name,
      taken_at: takenAt,
      dose_value: dose,
      dose_unit: dose === null ? null : intakeUnit,
      note: "",
      created_at: sleepTimestamp(),
    };
    mutate((current) => ({
      ...current,
      intakes: [...current.intakes, intake].sort((a, b) =>
        a.taken_at.localeCompare(b.taken_at),
      ),
    }));
  };
  const addMedication = async () => {
    if (!medName.trim()) return;
    const now = sleepTimestamp();
    const savedName = medName.trim();
    setPending(true);
    setError("");
    try {
      await saveSleepMedication({
        medication_id: editingMedication?.medication_id ?? "",
        expected_revision: editingMedication?.revision_id ?? null,
        created_at: now,
        updated_at: now,
        name: medName.trim(),
        default_dose_value: medDose === "" ? null : Number(medDose),
        default_dose_unit: medDose === "" ? null : medUnit,
        form: editingMedication?.form ?? "",
        note: editingMedication?.note ?? "",
        active: editingMedication?.active ?? true,
      });
      const catalog = await fetchSleepMedications();
      setMedications(catalog);
      const saved =
        catalog.find(
          (item) => item.medication_id === editingMedication?.medication_id,
        ) ?? catalog.find((item) => item.name === savedName);
      if (saved) {
        setIntakeMedication(saved.medication_id);
        setIntakeDose(saved.default_dose_value?.toString() ?? "");
        setIntakeUnit(saved.default_dose_unit ?? "mg");
      }
      setMedName("");
      setMedDose("");
      setEditingMedication(null);
      setMedicationFormOpen(false);
    } catch (reason) {
      setError(
        reason instanceof Error
          ? reason.message
          : "sleep_medication_mutation_failed",
      );
    } finally {
      setPending(false);
    }
  };
  const save = async () => {
    setPending(true);
    setError("");
    try {
      persist(draft);
      await persistence.current;
      await reload();
    } catch (reason) {
      setError(
        reason instanceof Error
          ? reason.message
          : "sleep_diary_mutation_failed",
      );
    } finally {
      setPending(false);
    }
  };
  const validate = async () => {
    setPending(true);
    setError("");
    try {
      persist(draft);
      await persistence.current;
      const current = identity.current;
      if (current)
        await validateSleepEntry(current.entry_id, current.revision_id);
      setPublicationStatus("ready");
      await reload();
    } catch (reason) {
      setError(
        reason instanceof Error
          ? reason.message
          : "sleep_diary_validation_failed",
      );
    } finally {
      setPending(false);
    }
  };
  const removeEntry = async () => {
    const current = identity.current;
    if (!current) return;
    setPending(true);
    setError("");
    try {
      await deleteSleepEntry({
        entry_id: current.entry_id,
        revision_id: current.revision_id,
      });
      identity.current = null;
      setDraft(blank());
      await reload();
    } catch (reason) {
      setError(
        reason instanceof Error
          ? reason.message
          : "sleep_diary_mutation_failed",
      );
    } finally {
      setPending(false);
    }
  };
  const timeline = [
    ...draft.events.map((event, index) => ({
      kind: "event" as const,
      at: event.start_at,
      event,
      index,
    })),
    ...draft.intakes.map((intake, index) => ({
      kind: "intake" as const,
      at: intake.taken_at,
      intake,
      index,
    })),
  ].sort((a, b) => a.at.localeCompare(b.at) || a.kind.localeCompare(b.kind));
  return (
    <div className="sleep-workspace" data-testid="sleep-workspace">
      <article className="analysis-panel analysis-wide">
        <div className="sleep-heading">
          <h2>{t.agenda}</h2>
          <div>
            <button
              className="quiet-action"
              type="button"
              disabled={!snapshot}
              onClick={() =>
                snapshot &&
                presentSleepDiaryPdf(
                  buildSleepDiaryPdf(snapshot, language),
                  false,
                )
              }
            >
              {t.preview}
            </button>
            <button
              className="quiet-action"
              type="button"
              disabled={!snapshot}
              onClick={() =>
                snapshot &&
                presentSleepDiaryPdf(
                  buildSleepDiaryPdf(snapshot, language),
                  true,
                )
              }
            >
              {t.export}
            </button>
          </div>
        </div>
        {entries.length === 0 ? (
          <p className="analysis-empty">{t.empty}</p>
        ) : (
          <div className="sleep-agenda-scroll">
            <div className="sleep-agenda" role="table">
              <div className="sleep-hours" aria-hidden="true">
                {Array.from({ length: 25 }, (_, index) => (
                  <span key={index}>{(18 + index) % 24}</span>
                ))}
              </div>
              {entries.map((entry) => (
                <button
                  type="button"
                  className="sleep-row"
                  key={entry.entry_id}
                  onClick={() => edit(entry)}
                >
                  <strong>
                    {t.night} {entry.night_start_date}
                    <small>
                      {t.statuses[entry.publication_status]} · {t.continue}
                    </small>
                  </strong>
                  <span className="sleep-track">
                    {entry.events.map((event) => {
                      const left = offset(
                        event.start_at,
                        entry.night_start_date,
                      );
                      const width = event.end_at
                        ? Math.max(
                            1,
                            offset(event.end_at, entry.night_start_date) - left,
                          )
                        : 1;
                      return (
                        <i
                          key={event.event_id}
                          className={`sleep-event sleep-${event.type}`}
                          style={{ left: `${left}%`, width: `${width}%` }}
                          title={`${t[event.type]} ${event.start_at}${event.end_at ? ` – ${event.end_at}` : ""}`}
                        >
                          {event.type === "bed_time"
                            ? "↓"
                            : event.type === "final_get_up" ||
                                event.type === "night_get_up"
                              ? "↑"
                              : event.type === "daytime_sleepiness"
                                ? "S"
                                : ""}
                        </i>
                      );
                    })}
                    {entry.intakes.map((intake) => (
                      <i
                        key={intake.intake_id}
                        className="sleep-event sleep-medication"
                        style={{
                          left: `${offset(intake.taken_at, entry.night_start_date)}%`,
                          width: "1%",
                        }}
                        title={`${t.medication}: ${intake.medication_name}`}
                      >
                        M
                      </i>
                    ))}
                  </span>
                  <span>
                    {entry.sleep_quality ?? "—"} / {entry.wake_quality ?? "—"} /{" "}
                    {entry.day_form ?? "—"}
                  </span>
                </button>
              ))}
            </div>
          </div>
        )}
      </article>

      <article className="analysis-panel analysis-wide sleep-entry-card">
        <header className="sleep-entry-header">
          <div className="sleep-night-block">
            <span>{t.night}</span>
            <div className="sleep-dates">
              <input
                aria-label={`${t.night} — ${t.start}`}
                type="date"
                value={draft.night_start_date}
                onChange={(event) =>
                  change("night_start_date", event.target.value)
                }
              />
              <span aria-hidden="true">→</span>
              <input
                aria-label={`${t.night} — ${t.end}`}
                type="date"
                value={draft.night_end_date}
                onChange={(event) =>
                  change("night_end_date", event.target.value)
                }
              />
            </div>
          </div>
          <div className="sleep-entry-state">
            <strong
              data-testid="sleep-publication-status"
              className={`sleep-status sleep-status-${publicationStatus}`}
            >
              {t.statuses[publicationStatus]}
            </strong>
            <span
              data-testid="sleep-save-state"
              className={`sleep-save-state sleep-save-${saveState}`}
              role="status"
            >
              {saveState === "saving"
                ? t.saving
                : saveState === "saved"
                  ? t.saved
                  : error}
            </span>
          </div>
        </header>

        <div className="sleep-editor-columns">
          <section className="sleep-subcard">
            <h3>{t.morning}</h3>
            <QualityPicker
              label={t.sleepQuality}
              value={draft.sleep_quality}
              onChange={(value) => change("sleep_quality", value)}
              language={language}
            />
            <QualityPicker
              label={t.wakeQuality}
              value={draft.wake_quality}
              onChange={(value) => change("wake_quality", value)}
              language={language}
            />
          </section>
          <section className="sleep-subcard">
            <h3>{t.day}</h3>
            <QualityPicker
              label={t.dayForm}
              value={draft.day_form}
              onChange={(value) => change("day_form", value)}
              language={language}
            />
          </section>
        </div>

        <section className="sleep-subcard">
          <h3>{t.addEventHeading}</h3>
          <div className="sleep-event-add">
            <label>
              {language === "fr" ? "Type" : "Type"}
              <select
                data-testid="sleep-event-type"
                value={eventType}
                onChange={(event) =>
                  setEventType(event.target.value as SleepEventType)
                }
              >
                {(
                  [
                    "bed_time",
                    "sleep",
                    "long_awake",
                    "night_get_up",
                    "final_get_up",
                    "half_sleep",
                    "nap",
                    "daytime_sleepiness",
                  ] as const
                ).map((type) => (
                  <option key={type} value={type}>
                    {t[type]}
                  </option>
                ))}
              </select>
            </label>
            <label>
              {pointTypes.includes(eventType) ? t.time : t.start}
              <input
                data-testid="sleep-event-start"
                type="time"
                value={eventStart}
                onChange={(event) => setEventStart(event.target.value)}
              />
            </label>
            {!pointTypes.includes(eventType) && (
              <label>
                {t.end}
                <input
                  data-testid="sleep-event-end"
                  type="time"
                  value={eventEnd}
                  onChange={(event) => setEventEnd(event.target.value)}
                />
              </label>
            )}
            <button
              className="primary-action"
              data-testid="sleep-add-event"
              type="button"
              onClick={addEvent}
            >
              + {t.add}
            </button>
          </div>
        </section>

        <section
          className="sleep-subcard sleep-timeline-editor"
          data-testid="sleep-timeline"
        >
          <h3>{t.timeline}</h3>
          {timeline.length === 0 ? (
            <p className="analysis-empty">{t.emptyEvents}</p>
          ) : (
            <ul className="sleep-event-list">
              {timeline.map((item) =>
                item.kind === "event" ? (
                  <li
                    key={item.event.event_id}
                    data-testid={`sleep-event-${item.event.type}`}
                  >
                    <time>
                      {clock(item.event.start_at)}
                      {item.event.end_at
                        ? ` → ${clock(item.event.end_at)}`
                        : ""}
                    </time>
                    <strong>{t[item.event.type]}</strong>
                    <label>
                      {pointTypes.includes(item.event.type) ? t.time : t.start}
                      <input
                        type="time"
                        value={clock(item.event.start_at)}
                        onChange={(event) =>
                          changeEvent(item.index, {
                            start_at: atClock(
                              event.target.value,
                              draft.night_start_date,
                              draft.night_end_date,
                            ),
                          })
                        }
                      />
                    </label>
                    {item.event.end_at && (
                      <label>
                        {t.end}
                        <input
                          type="time"
                          value={clock(item.event.end_at)}
                          onChange={(event) =>
                            changeEvent(item.index, {
                              end_at: atClock(
                                event.target.value,
                                draft.night_start_date,
                                draft.night_end_date,
                              ),
                            })
                          }
                        />
                      </label>
                    )}
                    <button
                      className="quiet-action"
                      type="button"
                      onClick={() => changeEvent(item.index, { ...item.event })}
                    >
                      {t.modify}
                    </button>
                    <button
                      className="quiet-action"
                      type="button"
                      aria-label={`${t.remove} ${t[item.event.type]}`}
                      onClick={() =>
                        change(
                          "events",
                          draft.events.filter(
                            (_, position) => position !== item.index,
                          ),
                        )
                      }
                    >
                      {t.remove}
                    </button>
                  </li>
                ) : (
                  <li
                    key={item.intake.intake_id}
                    data-testid="sleep-intake-row"
                  >
                    <time>{clock(item.intake.taken_at)}</time>
                    <strong>💊 {item.intake.medication_name}</strong>
                    <label>
                      {t.time}
                      <input
                        type="time"
                        value={clock(item.intake.taken_at)}
                        onChange={(event) =>
                          change(
                            "intakes",
                            draft.intakes.map((value, position) =>
                              position === item.index
                                ? {
                                    ...value,
                                    taken_at: atClock(
                                      event.target.value,
                                      draft.night_start_date,
                                      draft.night_end_date,
                                    ),
                                  }
                                : value,
                            ),
                          )
                        }
                      />
                    </label>
                    <label>
                      {t.dose}
                      <input
                        type="number"
                        min="0"
                        step="any"
                        value={item.intake.dose_value ?? ""}
                        onChange={(event) =>
                          change(
                            "intakes",
                            draft.intakes.map((value, position) =>
                              position === item.index
                                ? {
                                    ...value,
                                    dose_value:
                                      event.target.value === ""
                                        ? null
                                        : Number(event.target.value),
                                    dose_unit:
                                      event.target.value === ""
                                        ? null
                                        : (value.dose_unit ?? "mg"),
                                  }
                                : value,
                            ),
                          )
                        }
                      />
                    </label>
                    <span>{item.intake.dose_unit ?? ""}</span>
                    <button
                      className="quiet-action"
                      type="button"
                      aria-label={`${t.remove} ${item.intake.medication_name}`}
                      onClick={() =>
                        change(
                          "intakes",
                          draft.intakes.filter(
                            (_, position) => position !== item.index,
                          ),
                        )
                      }
                    >
                      {t.remove}
                    </button>
                  </li>
                ),
              )}
            </ul>
          )}
        </section>

        <section className="sleep-subcard">
          <h3>{t.addIntakeHeading}</h3>
          <div className="sleep-event-add sleep-intake-add">
            <label>
              {t.medication}
              <select
                data-testid="sleep-intake-medication"
                value={intakeMedication}
                onChange={(event) => selectMedication(event.target.value)}
              >
                <option value="" />
                {medications
                  .filter((item) => item.active)
                  .map((item) => (
                    <option key={item.medication_id} value={item.medication_id}>
                      {item.name}
                    </option>
                  ))}
              </select>
            </label>
            <label>
              {t.time}
              <input
                data-testid="sleep-intake-time"
                type="time"
                value={intakeTime}
                onChange={(event) => setIntakeTime(event.target.value)}
              />
            </label>
            <label>
              {t.dose}
              <input
                data-testid="sleep-intake-dose"
                type="number"
                min="0"
                step="any"
                value={intakeDose}
                onChange={(event) => setIntakeDose(event.target.value)}
              />
            </label>
            <label>
              {t.unit}
              <input
                data-testid="sleep-intake-unit"
                value={intakeUnit}
                onChange={(event) => setIntakeUnit(event.target.value)}
              />
            </label>
            <button
              className="primary-action"
              data-testid="sleep-add-intake"
              type="button"
              onClick={addIntake}
            >
              + {t.intake}
            </button>
          </div>
        </section>

        <label className="sleep-notes">
          {language === "fr"
            ? "Traitement et remarques particulières"
            : "Treatment and special notes"}
          <textarea
            value={draft.treatment_and_notes}
            maxLength={16384}
            placeholder={t.notesPlaceholder}
            onChange={(event) =>
              change("treatment_and_notes", event.target.value)
            }
          />
        </label>
        {error && (
          <p role="alert" className="error-panel">
            {error}
          </p>
        )}
        <div className="sleep-actions">
          <button
            className="quiet-action"
            type="button"
            disabled={pending}
            onClick={() => void save()}
          >
            {t.save}
          </button>
          <button
            className="primary-action"
            data-testid="sleep-validate-day"
            type="button"
            disabled={pending}
            onClick={() => void validate()}
          >
            {t.validate}
          </button>
          {draft.expected_revision && (
            <button
              className="danger-action"
              type="button"
              disabled={pending}
              onClick={() => void removeEntry()}
            >
              {t.deleteEntry}
            </button>
          )}
        </div>
      </article>

      <article className="analysis-panel analysis-wide sleep-medications-card">
        <div className="sleep-heading">
          <h2>{t.registeredMedications}</h2>
          {!medicationFormOpen && (
            <button
              className="quiet-action"
              type="button"
              onClick={() => setMedicationFormOpen(true)}
            >
              + {t.addMedication}
            </button>
          )}
        </div>
        {medicationFormOpen && (
          <div className="sleep-medication-form sleep-subcard">
            <label>
              {language === "fr" ? "Nom" : "Name"}
              <input
                data-testid="sleep-medication-name"
                value={medName}
                onChange={(event) => setMedName(event.target.value)}
              />
            </label>
            <label>
              {t.usualDose}
              <input
                data-testid="sleep-medication-dose"
                type="number"
                min="0"
                step="any"
                value={medDose}
                onChange={(event) => setMedDose(event.target.value)}
              />
            </label>
            <label>
              {t.unit}
              <input
                data-testid="sleep-medication-unit"
                value={medUnit}
                onChange={(event) => setMedUnit(event.target.value)}
              />
            </label>
            <button
              className="quiet-action"
              type="button"
              onClick={() => {
                setMedicationFormOpen(false);
                setEditingMedication(null);
                setMedName("");
                setMedDose("");
              }}
            >
              {t.cancel}
            </button>
            <button
              className="primary-action"
              data-testid="sleep-add-medication"
              type="button"
              disabled={pending}
              onClick={() => void addMedication()}
            >
              {t.save}
            </button>
          </div>
        )}
        {medications.length === 0 ? (
          <p className="analysis-empty">{t.medicationEmpty}</p>
        ) : (
          <ul
            data-testid="sleep-medication-list"
            className="sleep-medication-list"
          >
            {medications.map((item) => (
              <li key={item.medication_id}>
                <div>
                  <strong>{item.name}</strong>
                  <span>
                    {item.default_dose_value === null
                      ? "—"
                      : `${item.default_dose_value} ${item.default_dose_unit ?? ""}`}{" "}
                    {language === "fr" ? "habituel" : "usual"}
                  </span>
                </div>
                <div>
                  <button
                    className="quiet-action"
                    type="button"
                    onClick={() => {
                      setEditingMedication(item);
                      setMedName(item.name);
                      setMedDose(item.default_dose_value?.toString() ?? "");
                      setMedUnit(item.default_dose_unit ?? "mg");
                      setMedicationFormOpen(true);
                    }}
                  >
                    {t.modify}
                  </button>
                  {item.active && (
                    <button
                      className="quiet-action"
                      type="button"
                      onClick={async () => {
                        try {
                          const now = sleepTimestamp();
                          await saveSleepMedication({
                            ...item,
                            expected_revision: item.revision_id,
                            created_at: now,
                            updated_at: now,
                            active: false,
                          });
                          setMedications(await fetchSleepMedications());
                        } catch (reason) {
                          setError(
                            reason instanceof Error
                              ? reason.message
                              : "sleep_medication_mutation_failed",
                          );
                        }
                      }}
                    >
                      {t.deactivate}
                    </button>
                  )}
                </div>
              </li>
            ))}
          </ul>
        )}
      </article>

      <div className="analysis-section-grid">
        <article className="analysis-panel">
          <h2>{t.summary}</h2>
          <dl className="analysis-facts">
            <div>
              <dt>
                {language === "fr" ? "Nuits enregistrées" : "Recorded nights"}
              </dt>
              <dd>{entries.length}</dd>
            </div>
            <div>
              <dt>
                {language === "fr" ? "Sommeil déclaré" : "Declared sleep"}
              </dt>
              <dd>
                {Math.round(
                  (snapshot?.summary.sleep_duration_seconds ?? 0) / 60,
                )}{" "}
                min
              </dd>
            </div>
            <div>
              <dt>{language === "fr" ? "Longs réveils" : "Long awakenings"}</dt>
              <dd>{snapshot?.summary.long_awake_count ?? 0}</dd>
            </div>
            <div>
              <dt>{language === "fr" ? "Siestes" : "Naps"}</dt>
              <dd>{snapshot?.summary.nap_count ?? 0}</dd>
            </div>
          </dl>
        </article>
        <article className="analysis-panel">
          <h2>{t.observations}</h2>
          {entries
            .filter((entry) => entry.treatment_and_notes)
            .map((entry) => (
              <p key={entry.entry_id}>
                <strong>{entry.night_start_date}</strong> —{" "}
                {entry.treatment_and_notes}
              </p>
            ))}
        </article>
      </div>
    </div>
  );
}
