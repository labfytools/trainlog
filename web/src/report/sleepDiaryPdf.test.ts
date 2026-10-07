import { describe, expect, it } from "vitest";
import type { SleepEntry, SleepSnapshot } from "../api/sleepDiary";
import { buildSleepDiaryPdf } from "./sleepDiaryPdf";

const day = (value: number) => new Date(Date.UTC(2026, 8, value)).toISOString().slice(0, 10);
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
      quantity: name === "venlafaxine" && dose === 75 ? 2 : 1,
      note: "",
      created_at: at("18:00"),
    })),
  };
};

const compactDateForTest = (date: string) => date.split("-").reverse().join("/");

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
    ).toContain("Trainlog — Agenda sommeil");
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
      "Trainlog — Agenda sommeil",
      "20/09/2026",
      "au 21/09/2026",
      "loxapine",
      "12,5 mg",
      "venlafaxine",
      "37,5 mg",
    ])
      expect(content).toContain(expected);
    expect(content).not.toContain("non validé");

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
    const firstSleep = source.indexOf("0.75 g 183.58 509.00 71.67 18.00 re f");
    const longAwakening = source.indexOf(
      "0.98 0.70 0.53 rg 255.25 509.00 13.44 18.00 re f",
    );
    const secondSleep = source.indexOf("0.75 g 268.69 509.00 58.23 18.00 re f");
    expect(firstSleep).toBeGreaterThan(-1);
    expect(longAwakening).toBeGreaterThan(firstSleep);
    expect(secondSleep).toBeGreaterThan(longAwakening);
    expect(
      await textualContent(buildSleepDiaryPdf(snapshot(1), "fr")),
    ).not.toContain("AWAKE");
  });

  it("keeps the factual summary without a global validation warning", async () => {
    const content = await textualContent(buildSleepDiaryPdf(snapshot(2), "fr"));
    expect(content).toContain("2 nuits");
    expect(content).not.toContain("non validé");
    expect(content).not.toContain("validation");
    expect(content).not.toContain("jour(s)");
    expect(content).not.toContain("nuit(s)");
  });

  it("starts with the newest night even when input is oldest first", async () => {
    const data = snapshot(3);
    data.entries.reverse();
    const source = await buildSleepDiaryPdf(data, "fr", [], undefined, undefined,
      new Date("2026-10-07T12:00:00Z")).text();
    const dates = ["20/09/2026", "19/09/2026", "18/09/2026"];
    const positions = dates.map((date) => source.indexOf(`(${date})`));
    expect(positions.every((position) => position > -1)).toBe(true);
    expect(positions).toEqual([...positions].sort((a, b) => a - b));
    const content = await textualContent(buildSleepDiaryPdf(data, "fr", [],
      undefined, undefined, new Date("2026-10-07T12:00:00Z")));
    expect(content).toContain("Période : 18/09/2026 – 20/09/2026");
    expect(content).toContain("Généré le 07/10/2026");
  });

  it("keeps timed content within one night chronological", async () => {
    const data = snapshot(1);
    data.entries[0].events.reverse();
    data.entries[0].intakes = [
      { ...data.entries[0].intakes[0], intake_id: "late", medication_name: "LateMed",
        taken_at: timestamp("2026-09-21", "07:30") },
      { ...data.entries[0].intakes[1], intake_id: "early", medication_name: "EarlyMed",
        taken_at: timestamp("2026-09-20", "21:30") },
    ];
    const source = await buildSleepDiaryPdf(data, "fr").text();
    const content = await textualContent(buildSleepDiaryPdf(data, "fr"));
    expect(content.indexOf("EarlyMed")).toBeLessThan(content.indexOf("LateMed"));
    expect(source.indexOf("0.75 g 183.58 509.00")).toBeLessThan(
      source.indexOf("0.98 0.70 0.53 rg 255.25 509.00"));
    expect(source.indexOf("0.98 0.70 0.53 rg 255.25 509.00")).toBeLessThan(
      source.indexOf("0.75 g 268.69 509.00"));
  });

  it("prints structured medication quantity without replacing the unit dose", async () => {
    const content = await textualContent(buildSleepDiaryPdf(snapshot(1), "fr"));
    expect(content).toContain("venlafaxine — 75");
    expect(content).toContain("mg ×2");
  });

  it("keeps the English document independently localized", async () => {
    const content = await textualContent(buildSleepDiaryPdf(snapshot(1), "en"));
    expect(content).toContain("SLEEP QUALITY");
    expect(content).toContain("WAKE QUALITY");
    expect(content).toContain("Long awakening");
    expect(content).toContain("Medication intake");
    expect(content).not.toContain("unvalidated day");
    expect(content).not.toContain("QUALITÉ");
    expect(content).not.toContain("Long réveil");
  });

  it("paginates fourteen, twenty-one and thirty days", async () => {
    for (const count of [14, 21, 28, 30]) {
      const source = await buildSleepDiaryPdf(snapshot(count), "en").text();
      const pageCount = (source.match(/\/Type \/Page /g) ?? []).length;
      expect(pageCount).toBeGreaterThan(Math.ceil(count / 9));
      expect(pageCount).toBeLessThan(count);
      expect(source).toContain("OBSERVATIONS");
    }
  });

  it("preserves every selected treatment and all remarks beyond former limits", async () => {
    const data = snapshot(7);
    data.entries.forEach((entry, index) => {
      entry.treatment_and_notes = `Paragraph ${index}: ${"unbrokenword".repeat(20)}\n\n` +
        `Second paragraph with accents: éèê œ, dose 2,5 mg ×3. END-${index}`;
    });
    const source = await buildSleepDiaryPdf(data, "fr").text();
    const content = await textualContent(buildSleepDiaryPdf(data, "fr"));
    for (let index = 0; index < 7; index++) {
      expect(content).toContain(`END-${index}`);
      expect(content).toContain(`Paragraph ${index}:`);
    }
    expect(content).toContain("œ, dose 2,5");
    expect(content).toContain("mg ×3. END-0");
    expect(content).toContain("TRAITEMENTS");
    expect(content).toContain("REMARQUES");
    expect(content).toContain("mg ×2");
    expect(source).not.toContain("slice(0, 160)");
  });

  it("creates dated continuations for a single oversized entry", async () => {
    const data = snapshot(1);
    data.entries[0].treatment_and_notes = Array.from({ length: 150 }, (_, index) =>
      `Unique paragraph ${index} with meaningful complete text.`).join("\n");
    const source = await buildSleepDiaryPdf(data, "en").text();
    const content = await textualContent(buildSleepDiaryPdf(data, "en"));
    expect((source.match(/\/Type \/Page /g) ?? []).length).toBeGreaterThan(1);
    expect(content).toContain("continued");
    expect(content).toContain("Unique paragraph 0");
    expect(content).toContain("Unique paragraph 149");
    expect((content.match(/Unique paragraph /g) ?? []).length).toBe(150);
  });

  it("keeps empty and short notes compact in both languages", async () => {
    const data = snapshot(7);
    data.entries.forEach((entry) => { entry.intakes = []; entry.treatment_and_notes = ""; });
    for (const language of ["fr", "en"] as const) {
      const source = await buildSleepDiaryPdf(data, language).text();
      expect((source.match(/\/Type \/Page /g) ?? []).length).toBe(1);
      expect(source).not.toContain("continued");
    }
    data.entries[0].treatment_and_notes = "Short note.";
    const content = await textualContent(buildSleepDiaryPdf(data, "en"));
    expect(content).toContain("Short note.");
  });

  it("paginates 7, 14, 21, 28 and 30 selected nights in FR and EN without empty pages", async () => {
    for (const count of [7, 14, 21, 28, 30]) {
      for (const language of ["fr", "en"] as const) {
        const data = snapshot(count);
        const source = await buildSleepDiaryPdf(data, language).text();
        const content = await textualContent(buildSleepDiaryPdf(data, language));
        const pages = (source.match(/\/Type \/Page /g) ?? []).length;
        expect(pages).toBeGreaterThan(0);
        expect((content.match(/OBSERVATIONS/g) ?? []).length).toBe(pages);
        for (const entry of data.entries)
          expect(content).toContain(compactDateForTest(entry.night_start_date));
        expect(content).not.toContain("OUTSIDE-SELECTION");
      }
    }
  });

  it("prints factual sport bands and a separate legible listing without changing sleep totals", async () => {
    const sessions = [
      { identity: "se_previous", session_type: "training", label: "Yoga",
        started_at: "2026-09-20T19:00:00+02:00", ended_at: "2026-09-20T19:30:00+02:00" },
      { identity: "se_morning", session_type: "training", label: "Musculation",
        started_at: "2026-09-21T05:00:00+02:00", ended_at: "2026-09-21T06:00:00+02:00" },
      { identity: "se_evening", session_type: "training", label: "Étirements",
        started_at: "2026-09-21T18:30:00+02:00", ended_at: "2026-09-21T19:00:00+02:00" },
    ];
    const source = await buildSleepDiaryPdf(snapshot(1), "fr", sessions).text();
    const content = await textualContent(buildSleepDiaryPdf(snapshot(1), "fr", sessions));
    expect(source).toContain("0.10 0.34 0.82 rg 0 G");
    expect(source).toContain(" re B Q");
    expect(content).toContain("Musculation");
    expect(content).toContain("05:00");
    expect(content).toContain("Étirements");
    expect(content.indexOf("Musculation")).toBeLessThan(content.indexOf("Étirements"));
    expect(content.indexOf("Étirements")).toBeLessThan(content.indexOf("Yoga"));
    expect(content).toContain("sommeil non renseigné");
    expect(content).toContain("1 nuit");
    expect((source.match(/\/Type \/Page /g) ?? []).length).toBe(2);
  });

  it("prints sport without inventing a Sleep entry", async () => {
    const report = buildSleepDiaryPdf(snapshot(0), "fr", [{
      identity: "se_only", session_type: "training", label: "Cardio",
      started_at: "2026-09-21T05:00:00+02:00", ended_at: null,
    }], "2026-09-20", "2026-09-20");
    const content = await textualContent(report);
    expect(content).toContain("0 nuits");
    expect(content).toContain("Cardio");
    expect(content).toContain("sommeil non renseigné");
    expect(content).toContain("fin non renseignée");
  });
});
