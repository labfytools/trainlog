import { describe, expect, it } from "vitest";
import type { SleepEntry, SleepSnapshot } from "../api/sleepDiary";
import { buildSleepDiaryPdf } from "./sleepDiaryPdf";

const day = (value: number) => `2026-09-${String(value).padStart(2, "0")}`;
const timestamp = (date: string, time: string) => `${date}T${time}:00+02:00`;

const completeEntry = (index: number): SleepEntry => {
  const startDate = day(20 - index);
  const endDate = day(21 - index);
  const at = (time: string) =>
    timestamp(Number(time.slice(0, 2)) >= 18 ? startDate : endDate, time);
  return {
    publication_status: index < 2 ? "draft" : "synchronized",
    entry_id: `sl_${index}`,
    night_start_date: startDate,
    night_end_date: endDate,
    created_at: at("18:00"),
    updated_at: at("18:00"),
    revision_id: `slr_${index}`,
    sleep_quality: "B",
    wake_quality: "Moy",
    day_form: "TB",
    treatment_and_notes: "Remarque synthétique complète.",
    events: [
      {
        event_id: `bed_${index}`,
        type: "bed_time",
        start_at: at("22:30"),
        end_at: null,
      },
      {
        event_id: `sleep_a_${index}`,
        type: "sleep",
        start_at: at("23:00"),
        end_at: at("03:00"),
      },
      {
        event_id: `awake_${index}`,
        type: "long_awake",
        start_at: at("03:00"),
        end_at: at("03:45"),
      },
      {
        event_id: `sleep_b_${index}`,
        type: "sleep",
        start_at: at("03:45"),
        end_at: at("07:00"),
      },
      {
        event_id: `half_${index}`,
        type: "half_sleep",
        start_at: at("07:00"),
        end_at: at("07:10"),
      },
      {
        event_id: `night_up_${index}`,
        type: "night_get_up",
        start_at: at("04:30"),
        end_at: null,
      },
      {
        event_id: `up_${index}`,
        type: "final_get_up",
        start_at: at("07:15"),
        end_at: null,
      },
      {
        event_id: `nap_${index}`,
        type: "nap",
        start_at: at("14:00"),
        end_at: at("14:35"),
      },
      {
        event_id: `sleepiness_${index}`,
        type: "daytime_sleepiness",
        start_at: at("15:00"),
        end_at: null,
      },
    ],
    intakes: [
      ["loxapine", 12.5],
      ["diazepam", 5],
      ["venlafaxine", 75],
      ["venlafaxine", 37.5],
    ].map(([name, dose], intakeIndex) => ({
      intake_id: `mdi_${index}_${intakeIndex}`,
      medication_id: `med_${index}_${intakeIndex}`,
      medication_name: String(name),
      taken_at: at("22:30"),
      dose_value: Number(dose),
      dose_unit: "mg",
      note: "",
      created_at: at("18:00"),
    })),
  };
};

const snapshot = (count: number): SleepSnapshot => ({
  api_version: 1,
  entries: Array.from({ length: count }, (_, index) => completeEntry(index)),
  summary: {
    nights: count,
    long_awake_count: count,
    nap_count: count,
    sleepiness_count: count,
    intake_count: count * 4,
    sleep_duration_seconds: count * 26_100,
    long_awake_duration_seconds: count * 2_700,
    nap_duration_seconds: count * 2_100,
    average_bed_minute: 270,
    average_get_up_minute: 795,
  },
});

const textualContent = async (blob: Blob) => {
  const source = await blob.text();
  const bytes = source.replace(/\\([0-7]{3})/g, (_, octal: string) =>
    String.fromCharCode(Number.parseInt(octal, 8)),
  );
  return new TextDecoder("windows-1252").decode(
    Uint8Array.from(bytes, (character) => character.charCodeAt(0)),
  );
};

describe("sleep diary PDF", () => {
  it("builds a vector PDF without screen capture", async () => {
    const bytes = new Uint8Array(
      await buildSleepDiaryPdf(snapshot(7), "fr").arrayBuffer(),
    );
    expect(new TextDecoder().decode(bytes.slice(0, 8))).toBe("%PDF-1.4");
    expect(
      await textualContent(buildSleepDiaryPdf(snapshot(7), "fr")),
    ).toContain("AGENDA TRAINLOG");
  });

  it("fully localizes the complete French document", async () => {
    const content = await textualContent(buildSleepDiaryPdf(snapshot(1), "fr"));
    for (const expected of [
      "QUALITÉ DU",
      "QUALITÉ DU",
      "RÉVEIL",
      "FORME DE LA",
      "JOURNÉE",
      "TRAITEMENT ET",
      "REMARQUES",
      "SOMMEIL",
      "Mise au lit",
      "Lever",
      "Lever nocturne",
      "Somnolence",
      "Prise de médicament",
      "Sieste",
      "Long réveil",
      "Demi-sommeil",
      "1 nuit",
      "Sommeil déclaré : 7 h 15 min",
      "AVERTISSEMENT : 1 jour non validé dans cet export.",
      "20/09/2026",
      "au 21/09/2026",
      "loxapine",
      "12,5 mg",
      "venlafaxine",
      "37,5 mg",
    ])
      expect(content).toContain(expected);

    for (const forbidden of [
      "SLEEP",
      "WAKE",
      "DAY",
      "TREATMENT",
      "AWAKE",
      "NAP",
      "HALF",
      "sleepiness",
      "medication intake",
      "nights",
    ])
      expect(content).not.toContain(forbidden);
  });

  it("draws a proportional 45-minute long-awakening interval between sleep ranges", async () => {
    const source = await buildSleepDiaryPdf(snapshot(1), "fr").text();
    const firstSleep = source.indexOf("0.75 g 216.17 509.00 83.33 18.00 re f");
    const longAwakening = source.indexOf(
      "0.5 g 299.50 509.00 15.63 18.00 re f",
    );
    const secondSleep = source.indexOf("0.75 g 315.13 509.00 67.71 18.00 re f");
    expect(firstSleep).toBeGreaterThan(-1);
    expect(longAwakening).toBeGreaterThan(firstSleep);
    expect(secondSleep).toBeGreaterThan(longAwakening);
    expect(
      await textualContent(buildSleepDiaryPdf(snapshot(1), "fr")),
    ).not.toContain("AWAKE");
  });

  it("uses correct French plurals for multiple draft days", async () => {
    const content = await textualContent(buildSleepDiaryPdf(snapshot(2), "fr"));
    expect(content).toContain("2 nuits");
    expect(content).toContain(
      "AVERTISSEMENT : 2 jours non validés dans cet export.",
    );
    expect(content).not.toContain("jour(s)");
    expect(content).not.toContain("nuit(s)");
  });

  it("keeps the English document independently localized", async () => {
    const content = await textualContent(buildSleepDiaryPdf(snapshot(1), "en"));
    expect(content).toContain("SLEEP QUALITY");
    expect(content).toContain("WAKE QUALITY");
    expect(content).toContain("Long awakening");
    expect(content).toContain("Medication intake");
    expect(content).not.toContain("QUALITÉ");
    expect(content).not.toContain("Long réveil");
  });

  it("paginates fourteen, twenty-one and thirty days", async () => {
    for (const count of [14, 21, 30]) {
      const source = await buildSleepDiaryPdf(snapshot(count), "en").text();
      expect((source.match(/\/Type \/Page /g) ?? []).length).toBe(
        Math.ceil(count / 9),
      );
      expect(source).toContain("OBSERVATIONS");
    }
  });
});
