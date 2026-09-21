import { mutationCsrfToken } from "./sync";

export type SleepQuality = "TB" | "B" | "Moy" | "M" | "TM";
export type SleepEventType =
  | "bed_time"
  | "final_get_up"
  | "night_get_up"
  | "sleep"
  | "nap"
  | "long_awake"
  | "half_sleep"
  | "daytime_sleepiness";

export interface SleepEvent {
  event_id: string;
  type: SleepEventType;
  start_at: string;
  end_at: string | null;
}
export interface MedicationIntake {
  intake_id: string;
  medication_id: string;
  medication_name: string;
  taken_at: string;
  dose_value: number | null;
  dose_unit: string | null;
  note: string;
  created_at: string;
}
export interface SleepMedication {
  medication_id: string;
  revision_id: string;
  name: string;
  default_dose_value: number | null;
  default_dose_unit: string | null;
  form: string;
  note: string;
  active: boolean;
}
export interface SleepEntry {
  entry_id: string;
  night_start_date: string;
  night_end_date: string;
  created_at: string;
  updated_at: string;
  revision_id: string;
  sleep_quality: SleepQuality | null;
  wake_quality: SleepQuality | null;
  day_form: SleepQuality | null;
  treatment_and_notes: string;
  events: SleepEvent[];
  intakes: MedicationIntake[];
  publication_status: "draft" | "ready" | "synchronized" | "modified";
}
export interface SleepSnapshot {
  api_version: 1;
  entries: SleepEntry[];
  summary: {
    nights: number;
    long_awake_count: number;
    nap_count: number;
    sleepiness_count: number;
    intake_count: number;
    sleep_duration_seconds: number;
    long_awake_duration_seconds: number;
    nap_duration_seconds: number;
    average_bed_minute: number | null;
    average_get_up_minute: number | null;
  };
}
export type SleepEntryInput = Omit<
  SleepEntry,
  "revision_id" | "publication_status"
> & {
  expected_revision: string | null;
};

// The C17 timestamp parser intentionally accepts second-precision RFC 3339.
// Keep browser-created causal timestamps on that public boundary.
export const sleepTimestamp = (date = new Date()): string =>
  date.toISOString().replace(/\.\d{3}Z$/, "Z");

export function newSleepId(prefix: "sle" | "mdi"): string {
  /*
   * WHY: trainlog.perf is intentionally served over loopback HTTP and is not
   * a browser secure context, so crypto.randomUUID() is unavailable there.
   * CONTRACT: getRandomValues() supplies all entropy; Trainlog sets the UUIDv4
   * version and RFC 4122 variant bits and never falls back to Math.random().
   */
  const bytes = new Uint8Array(16);
  crypto.getRandomValues(bytes);
  bytes[6] = (bytes[6] & 0x0f) | 0x40;
  bytes[8] = (bytes[8] & 0x3f) | 0x80;
  const hexadecimal = Array.from(bytes, (value) =>
    value.toString(16).padStart(2, "0"),
  );
  const uuid = [
    hexadecimal.slice(0, 4).join(""),
    hexadecimal.slice(4, 6).join(""),
    hexadecimal.slice(6, 8).join(""),
    hexadecimal.slice(8, 10).join(""),
    hexadecimal.slice(10, 16).join(""),
  ].join("-");
  return `${prefix}_${uuid}`;
}

const object = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);
const quality = (value: unknown): value is SleepQuality | null =>
  value === null || ["TB", "B", "Moy", "M", "TM"].includes(String(value));
const eventType = (value: unknown): value is SleepEventType =>
  [
    "bed_time",
    "final_get_up",
    "night_get_up",
    "sleep",
    "nap",
    "long_awake",
    "half_sleep",
    "daytime_sleepiness",
  ].includes(String(value));

function validEntry(value: unknown): value is SleepEntry {
  return (
    object(value) &&
    typeof value.entry_id === "string" &&
    typeof value.night_start_date === "string" &&
    typeof value.night_end_date === "string" &&
    typeof value.created_at === "string" &&
    typeof value.updated_at === "string" &&
    typeof value.revision_id === "string" &&
    ["draft", "ready", "synchronized", "modified"].includes(
      String(value.publication_status),
    ) &&
    quality(value.sleep_quality) &&
    quality(value.wake_quality) &&
    quality(value.day_form) &&
    typeof value.treatment_and_notes === "string" &&
    Array.isArray(value.events) &&
    value.events.length <= 64 &&
    value.events.every(
      (event) =>
        object(event) &&
        typeof event.event_id === "string" &&
        eventType(event.type) &&
        typeof event.start_at === "string" &&
        (event.end_at === null || typeof event.end_at === "string"),
    ) &&
    Array.isArray(value.intakes) &&
    value.intakes.length <= 32 &&
    value.intakes.every(
      (intake) =>
        object(intake) &&
        typeof intake.intake_id === "string" &&
        typeof intake.medication_id === "string" &&
        typeof intake.medication_name === "string" &&
        typeof intake.taken_at === "string" &&
        (intake.dose_value === null || typeof intake.dose_value === "number") &&
        (intake.dose_unit === null || typeof intake.dose_unit === "string") &&
        typeof intake.note === "string" &&
        typeof intake.created_at === "string",
    )
  );
}

export async function fetchSleepDiary(
  startDate?: string,
  endDate?: string,
  signal?: AbortSignal,
): Promise<SleepSnapshot> {
  const query = new URLSearchParams({ limit: "3660" });
  if (startDate) query.set("start_date", startDate);
  if (endDate) query.set("end_date", endDate);
  const response = await fetch(`/api/v1/sleep-diary?${query}`, {
    headers: { Accept: "application/json" },
    signal,
  });
  const value: unknown = await response.json();
  if (
    !response.ok ||
    !object(value) ||
    value.api_version !== 1 ||
    !Array.isArray(value.entries) ||
    !value.entries.every(validEntry) ||
    !object(value.summary)
  )
    throw new TypeError("sleep_diary_invalid");
  return value as unknown as SleepSnapshot;
}

async function mutation(
  method: "POST" | "DELETE",
  body: unknown,
): Promise<{ entry_id: string; revision_id: string }> {
  const csrf = await mutationCsrfToken();
  const response = await fetch("/api/v1/sleep-diary", {
    method,
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
      "X-Trainlog-CSRF-Token": csrf,
    },
    body: JSON.stringify(body),
  });
  const value: unknown = await response.json();
  if (
    !response.ok ||
    !object(value) ||
    typeof value.entry_id !== "string" ||
    typeof value.revision_id !== "string"
  ) {
    throw new Error(
      object(value) && typeof value.error === "string"
        ? value.error
        : "sleep_diary_mutation_failed",
    );
  }
  return value as { entry_id: string; revision_id: string };
}

export const saveSleepEntry = async (entry: SleepEntryInput) =>
  await mutation("POST", entry);
export async function validateSleepEntry(
  entryId: string,
  revisionId: string,
): Promise<void> {
  const csrf = await mutationCsrfToken();
  const response = await fetch("/api/v1/sleep-diary/validate", {
    method: "POST",
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
      "X-Trainlog-CSRF-Token": csrf,
    },
    body: JSON.stringify({
      entry_id: entryId,
      expected_revision: revisionId,
      validated_at: sleepTimestamp(),
    }),
  });
  if (!response.ok) throw new Error("sleep_diary_validation_failed");
}
export const deleteSleepEntry = async (
  entry: Pick<SleepEntry, "entry_id" | "revision_id">,
) =>
  await mutation("DELETE", {
    entry_id: entry.entry_id,
    expected_revision: entry.revision_id,
    deleted_at: sleepTimestamp(),
  });

export async function fetchSleepMedications(): Promise<SleepMedication[]> {
  const response = await fetch("/api/v1/sleep-medications", {
    headers: { Accept: "application/json" },
  });
  const value: unknown = await response.json();
  if (
    !response.ok ||
    !object(value) ||
    value.api_version !== 1 ||
    !Array.isArray(value.medications) ||
    !value.medications.every(
      (item) =>
        object(item) &&
        typeof item.medication_id === "string" &&
        typeof item.revision_id === "string" &&
        typeof item.name === "string" &&
        (item.default_dose_value === null ||
          typeof item.default_dose_value === "number") &&
        (item.default_dose_unit === null ||
          typeof item.default_dose_unit === "string") &&
        typeof item.form === "string" &&
        typeof item.note === "string" &&
        typeof item.active === "boolean",
    )
  )
    throw new TypeError("sleep_medications_invalid");
  return value.medications as SleepMedication[];
}

export async function saveSleepMedication(
  input: Omit<SleepMedication, "revision_id"> & {
    expected_revision: string | null;
    created_at: string;
    updated_at: string;
  },
): Promise<void> {
  const csrf = await mutationCsrfToken();
  const response = await fetch("/api/v1/sleep-medications", {
    method: "POST",
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
      "X-Trainlog-CSRF-Token": csrf,
    },
    body: JSON.stringify(input),
  });
  if (!response.ok) throw new Error("sleep_medication_mutation_failed");
}
