import {
  fireEvent,
  render,
  screen,
  waitFor,
  within,
} from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import type {
  SleepEntry,
  SleepEntryInput,
  SleepMedication,
  SleepSnapshot,
} from "../api/sleepDiary";
import * as pdfReport from "../report/sleepDiaryPdf";
import { SleepDiaryWorkspace } from "./SleepDiaryWorkspace";

const api = vi.hoisted(() => ({
  deleteSleepEntry: vi.fn(),
  fetchSleepDiary: vi.fn(),
  fetchSleepMedications: vi.fn(),
  saveSleepEntry: vi.fn(),
  saveSleepMedication: vi.fn(),
  validateSleepEntry: vi.fn(),
  nextId: 0,
}));

const heartRateApi = vi.hoisted(() => ({
  fetchHeartRateTimeline: vi.fn(),
}));

vi.mock("../api/sleepDiary", async (importOriginal) => {
  const original = await importOriginal<typeof import("../api/sleepDiary")>();
  return {
    ...original,
    deleteSleepEntry: api.deleteSleepEntry,
    fetchSleepDiary: api.fetchSleepDiary,
    fetchSleepMedications: api.fetchSleepMedications,
    saveSleepEntry: api.saveSleepEntry,
    saveSleepMedication: api.saveSleepMedication,
    validateSleepEntry: api.validateSleepEntry,
    newSleepId: (prefix: "sle" | "mdi") =>
      `${prefix}_00000000-0000-4000-8000-${String(++api.nextId).padStart(12, "0")}`,
  };
});

vi.mock("../api/heartRate", async (importOriginal) => {
  const original = await importOriginal<typeof import("../api/heartRate")>();
  return { ...original, fetchHeartRateTimeline: heartRateApi.fetchHeartRateTimeline };
});

const emptySnapshot: SleepSnapshot = {
  api_version: 1,
  entries: [],
  summary: {
    nights: 0,
    long_awake_count: 0,
    nap_count: 0,
    sleepiness_count: 0,
    intake_count: 0,
    sleep_duration_seconds: 0,
    long_awake_duration_seconds: 0,
    nap_duration_seconds: 0,
    average_bed_minute: null,
    average_get_up_minute: null,
  },
};

const medication: SleepMedication = {
  medication_id: "med_00000000-0000-4000-8000-000000000001",
  revision_id: "mr_00000000-0000-4000-8000-000000000001",
  name: "TestMed",
  default_dose_value: 5,
  default_dose_unit: "mg",
  form: "",
  note: "",
  active: true,
};

const snapshotWith = (entries: SleepEntry[]): SleepSnapshot => {
  const duration = (start: string, end: string | null) =>
    end === null ? 0 : (Date.parse(end) - Date.parse(start)) / 1000;
  const events = entries.flatMap((entry) => entry.events);
  const selected = (type: string) =>
    events.filter((event) => event.type === type);
  return {
    api_version: 1,
    entries,
    summary: {
      nights: entries.length,
      long_awake_count: selected("long_awake").length,
      nap_count: selected("nap").length,
      sleepiness_count: selected("daytime_sleepiness").length,
      intake_count: entries.reduce(
        (total, entry) => total + entry.intakes.length,
        0,
      ),
      sleep_duration_seconds: selected("sleep").reduce(
        (total, event) => total + duration(event.start_at, event.end_at),
        0,
      ),
      long_awake_duration_seconds: selected("long_awake").reduce(
        (total, event) => total + duration(event.start_at, event.end_at),
        0,
      ),
      nap_duration_seconds: selected("nap").reduce(
        (total, event) => total + duration(event.start_at, event.end_at),
        0,
      ),
      average_bed_minute: null,
      average_get_up_minute: null,
    },
  };
};

const persistedEntry = (
  input: SleepEntryInput,
  entryId: string,
  revisionId: string,
): SleepEntry => ({
  ...input,
  entry_id: entryId,
  revision_id: revisionId,
  publication_status: "draft",
  events: input.events.map((event) => ({ ...event })),
  intakes: input.intakes.map((intake) => ({ ...intake })),
});

const dayAEntry = (): SleepEntry => ({
  entry_id: "sd_00000000-0000-4000-8000-000000000020",
  revision_id: "sdr_00000000-0000-4000-8000-000000000020",
  night_start_date: "2026-09-20",
  night_end_date: "2026-09-21",
  created_at: "2026-09-20T18:00:00+02:00",
  updated_at: "2026-09-21T16:00:00+02:00",
  sleep_quality: "B",
  wake_quality: "TB",
  day_form: "Moy",
  treatment_and_notes: "Observation du jour A",
  publication_status: "ready",
  events: [
    {
      event_id: "sle_bed",
      type: "bed_time",
      start_at: "2026-09-20T22:30:00+02:00",
      end_at: null,
    },
    {
      event_id: "sle_sleep",
      type: "sleep",
      start_at: "2026-09-20T22:30:00+02:00",
      end_at: "2026-09-21T03:30:00+02:00",
    },
    {
      event_id: "sle_up",
      type: "final_get_up",
      start_at: "2026-09-21T04:45:00+02:00",
      end_at: null,
    },
    {
      event_id: "sle_sleepiness",
      type: "daytime_sleepiness",
      start_at: "2026-09-21T15:30:00+02:00",
      end_at: null,
    },
  ],
  intakes: [
    {
      intake_id: "mdi_day_a",
      medication_id: medication.medication_id,
      medication_name: medication.name,
      taken_at: "2026-09-20T22:30:00+02:00",
      dose_value: 5,
      dose_unit: "mg",
      quantity: 1,
      note: "",
      created_at: "2026-09-20T22:30:00+02:00",
    },
  ],
});

describe("SleepDiaryWorkspace", () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ["Date"] });
    vi.setSystemTime(new Date("2026-09-21T16:00:00+02:00"));
    vi.clearAllMocks();
    api.nextId = 0;
    api.fetchSleepDiary.mockResolvedValue(emptySnapshot);
    api.fetchSleepMedications.mockResolvedValue([]);
    heartRateApi.fetchHeartRateTimeline.mockImplementation(async (contextId) => ({
      api_version: 1,
      context_id: contextId,
      available: false,
      capture: null,
      events: [],
      guidance: null,
      calibration: null,
    }));
    let revision = 0;
    api.saveSleepEntry.mockImplementation(async (input) => ({
      entry_id: input.entry_id || "sd_00000000-0000-4000-8000-000000000001",
      revision_id: `sdr_00000000-0000-4000-8000-${String(++revision).padStart(12, "0")}`,
    }));
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("renders every deduplicated period night and changes presentation selection", async () => {
    const first = dayAEntry();
    const second = {
      ...dayAEntry(),
      entry_id: "sl_00000000-0000-4000-8000-000000000021",
      night_start_date: "2026-09-18",
      night_end_date: "2026-09-19",
    };
    const third = {
      ...dayAEntry(),
      entry_id: "sl_00000000-0000-4000-8000-000000000022",
      night_start_date: "2026-09-19",
      night_end_date: "2026-09-20",
    };
    api.fetchSleepDiary.mockResolvedValue(snapshotWith([first, third, second, third]));
    render(<SleepDiaryWorkspace period="30d" language="fr" />);

    await screen.findByTestId(`sleep-agenda-row-${first.entry_id}`);
    expect(screen.getAllByTestId(/^sleep-agenda-row-/)).toHaveLength(3);
    const secondRow = screen.getByTestId(`sleep-agenda-row-${second.entry_id}`);
    fireEvent.click(secondRow);
    expect(secondRow).toHaveClass("sleep-row-selected");
    expect(screen.getByTestId("sleep-night-start")).toHaveValue("2026-09-18");
    expect(screen.getByTestId("sleep-night-end")).toHaveValue("2026-09-19");
    await waitFor(() =>
      expect(heartRateApi.fetchHeartRateTimeline).toHaveBeenLastCalledWith(
        second.entry_id,
        expect.any(AbortSignal),
      ),
    );
    expect(api.saveSleepEntry).not.toHaveBeenCalled();
    expect(screen.getByLabelText("Traitement et remarques particulières")).toHaveValue(
      "Observation du jour A",
    );
  });

  it("uses the same inclusive date-range subset for PDF preview and export", async () => {
    const first = dayAEntry();
    const second = {
      ...dayAEntry(),
      entry_id: "sl_selected",
      revision_id: "slr_selected",
      night_start_date: "2026-09-18",
      night_end_date: "2026-09-19",
      treatment_and_notes: "Selected observation",
    };
    api.fetchSleepDiary.mockResolvedValue(snapshotWith([first, second]));
    const build = vi.spyOn(pdfReport, "buildSleepDiaryPdf").mockReturnValue(
      new Blob(["selected"], { type: "application/pdf" }),
    );
    const present = vi.spyOn(pdfReport, "presentSleepDiaryPdf").mockImplementation(() => {});
    render(<SleepDiaryWorkspace period="30d" language="fr" />);

    fireEvent.click(await screen.findByTestId(`sleep-agenda-row-${second.entry_id}`));
    fireEvent.change(screen.getByLabelText("PDF du"), {
      target: { value: "2026-09-18" },
    });
    fireEvent.change(screen.getByLabelText("au"), {
      target: { value: "2026-09-18" },
    });
    fireEvent.click(screen.getByRole("button", { name: "Prévisualiser" }));
    fireEvent.click(screen.getByRole("button", { name: "Exporter PDF" }));

    expect(build).toHaveBeenCalledTimes(2);
    for (const [selected] of build.mock.calls) {
      expect(selected.entries.map((entry) => entry.entry_id)).toEqual([second.entry_id]);
      expect(selected.summary.nights).toBe(1);
      expect(selected.summary.sleep_duration_seconds).toBe(5 * 3600);
    }
    expect(present.mock.calls.map((call) => call[1])).toEqual([false, true]);
    expect(api.saveSleepEntry).not.toHaveBeenCalled();
  });

  it("exports every night whose start date is inside the chosen range", async () => {
    const first = dayAEntry();
    const second = {
      ...dayAEntry(),
      entry_id: "sl_range_second",
      revision_id: "slr_range_second",
      night_start_date: "2026-09-19",
      night_end_date: "2026-09-20",
    };
    api.fetchSleepDiary.mockResolvedValue(snapshotWith([first, second]));
    const build = vi.spyOn(pdfReport, "buildSleepDiaryPdf").mockReturnValue(
      new Blob(["range"], { type: "application/pdf" }),
    );
    vi.spyOn(pdfReport, "presentSleepDiaryPdf").mockImplementation(() => {});
    render(<SleepDiaryWorkspace period="30d" language="fr" />);

    await screen.findByTestId(`sleep-agenda-row-${second.entry_id}`);
    await waitFor(() => {
      expect(screen.getByLabelText("PDF du")).toHaveValue("2026-09-19");
      expect(screen.getByLabelText("au")).toHaveValue(first.night_start_date);
    });
    fireEvent.change(screen.getByLabelText("PDF du"), {
      target: { value: "2026-09-21" },
    });
    expect(screen.getByRole("button", { name: "Prévisualiser" })).toBeDisabled();
    expect(screen.getByText("Aucune nuit dans cette plage.")).toBeInTheDocument();
    fireEvent.change(screen.getByLabelText("PDF du"), {
      target: { value: "2026-09-19" },
    });
    fireEvent.change(screen.getByLabelText("au"), {
      target: { value: first.night_start_date },
    });
    fireEvent.click(screen.getByRole("button", { name: "Prévisualiser" }));

    const [selection] = build.mock.calls[0];
    expect(selection.entries.map((entry) => entry.entry_id).sort()).toEqual(
      [first.entry_id, second.entry_id].sort(),
    );
    expect(selection.summary.nights).toBe(2);
  });

  it.each([
    ["bed_time", "Mise au lit", "22:30", null],
    ["sleep", "Sommeil", "23:00", "06:30"],
    ["long_awake", "Long réveil", "03:00", "03:30"],
    ["final_get_up", "Lever", "07:00", null],
    ["nap", "Sieste", "14:00", "14:30"],
    ["daytime_sleepiness", "Somnolence", "15:00", null],
  ])(
    "keeps the workspace mounted after adding %s and advances the same entry revision",
    async (type, label, start, end) => {
      render(<SleepDiaryWorkspace period="30d" language="fr" />);
      await screen.findByTestId("sleep-workspace");
      fireEvent.change(screen.getByTestId("sleep-event-type"), {
        target: { value: type },
      });
      fireEvent.change(screen.getByTestId("sleep-event-start"), {
        target: { value: start },
      });
      if (end !== null)
        fireEvent.change(screen.getByTestId("sleep-event-end"), {
          target: { value: end },
        });
      fireEvent.click(screen.getByTestId("sleep-add-event"));

      expect(
        await screen.findByTestId(`sleep-event-${type}`),
      ).toHaveTextContent(label);
      expect(screen.getByTestId("sleep-workspace")).toBeInTheDocument();
      expect(screen.getByTestId("sleep-add-event")).toBeEnabled();
      await waitFor(() => expect(api.saveSleepEntry).toHaveBeenCalledTimes(1));
      expect(api.saveSleepEntry.mock.calls[0][0]).toMatchObject({
        entry_id: "",
        expected_revision: null,
      });

      fireEvent.click(screen.getByTestId("sleep-add-event"));
      await waitFor(() => expect(api.saveSleepEntry).toHaveBeenCalledTimes(2));
      expect(api.saveSleepEntry.mock.calls[1][0]).toMatchObject({
        entry_id: "sd_00000000-0000-4000-8000-000000000001",
        expected_revision: "sdr_00000000-0000-4000-8000-000000000001",
      });
      expect(
        screen.queryByText("Impossible d’afficher cette section."),
      ).not.toBeInTheDocument();
    },
  );

  it("adds a medication, exposes it immediately for an intake, and supports edit and delete mutations", async () => {
    api.fetchSleepMedications
      .mockResolvedValueOnce([])
      .mockResolvedValue([medication]);
    render(<SleepDiaryWorkspace period="30d" language="fr" />);
    await screen.findByText("Aucun médicament enregistré.");

    fireEvent.click(
      screen.getByRole("button", { name: "+ Ajouter un médicament" }),
    );
    fireEvent.change(screen.getByTestId("sleep-medication-name"), {
      target: { value: "TestMed" },
    });
    fireEvent.change(screen.getByTestId("sleep-medication-dose"), {
      target: { value: "5" },
    });
    fireEvent.click(screen.getByTestId("sleep-add-medication"));

    const medicationList = await screen.findByTestId("sleep-medication-list");
    expect(within(medicationList).getByText("TestMed")).toBeInTheDocument();
    expect(screen.getByTestId("sleep-intake-medication")).toHaveTextContent(
      "TestMed",
    );
    fireEvent.change(screen.getByTestId("sleep-intake-medication"), {
      target: { value: medication.medication_id },
    });
    fireEvent.click(screen.getByTestId("sleep-add-intake"));
    const intake = await screen.findByTestId("sleep-intake-row");
    expect(intake).toHaveTextContent("TestMed");
    expect(screen.getByTestId("sleep-workspace")).toBeInTheDocument();

    fireEvent.change(within(intake).getByLabelText("Heure"), {
      target: { value: "22:15" },
    });
    await waitFor(() => expect(api.saveSleepEntry).toHaveBeenCalledTimes(2));
    fireEvent.click(
      within(intake).getByRole("button", { name: "Supprimer TestMed" }),
    );
    await waitFor(() => expect(api.saveSleepEntry).toHaveBeenCalledTimes(3));
    expect(screen.queryByTestId("sleep-intake-row")).not.toBeInTheDocument();
  });

  it("modifies and removes an event without losing the workspace", async () => {
    render(<SleepDiaryWorkspace period="30d" language="fr" />);
    await screen.findByTestId("sleep-workspace");
    fireEvent.change(screen.getByTestId("sleep-event-type"), {
      target: { value: "sleep" },
    });
    fireEvent.click(screen.getByTestId("sleep-add-event"));
    const row = await screen.findByTestId("sleep-event-sleep");
    fireEvent.change(within(row).getByLabelText("Début"), {
      target: { value: "23:00" },
    });
    fireEvent.click(within(row).getByRole("button", { name: "Modifier" }));
    fireEvent.click(
      within(row).getByRole("button", { name: "Supprimer Sommeil" }),
    );
    await waitFor(() =>
      expect(screen.queryByTestId("sleep-event-sleep")).not.toBeInTheDocument(),
    );
    expect(screen.getByTestId("sleep-workspace")).toBeInTheDocument();
    expect(screen.getByTestId("sleep-add-event")).toBeEnabled();
  });

  it("projects every persisted revision live and keeps distinct medication doses", async () => {
    let persisted: SleepEntry[] = [];
    let catalog: SleepMedication[] = [];
    let revision = 0;
    api.fetchSleepDiary.mockImplementation(async () => snapshotWith(persisted));
    api.fetchSleepMedications.mockImplementation(async () => catalog);
    api.saveSleepEntry.mockImplementation(async (input: SleepEntryInput) => {
      const entryId =
        input.entry_id || "sd_00000000-0000-4000-8000-000000000001";
      const revisionId = `sdr_00000000-0000-4000-8000-${String(++revision).padStart(12, "0")}`;
      persisted = [persistedEntry(input, entryId, revisionId)];
      return { entry_id: entryId, revision_id: revisionId };
    });
    api.saveSleepMedication.mockImplementation(async (input) => {
      const index = catalog.length + 1;
      const saved: SleepMedication = {
        ...input,
        medication_id: `med_00000000-0000-4000-8000-${String(index).padStart(12, "0")}`,
        revision_id: `mr_00000000-0000-4000-8000-${String(index).padStart(12, "0")}`,
      };
      catalog = [...catalog, saved];
      return {
        medication_id: saved.medication_id,
        revision_id: saved.revision_id,
      };
    });

    render(<SleepDiaryWorkspace period="30d" language="fr" />);
    await screen.findByText("Aucun médicament enregistré.");

    const addMedication = async (dose: string) => {
      fireEvent.click(
        screen.getByRole("button", { name: "+ Ajouter un médicament" }),
      );
      fireEvent.change(screen.getByTestId("sleep-medication-name"), {
        target: { value: "venlafaxine" },
      });
      fireEvent.change(screen.getByTestId("sleep-medication-dose"), {
        target: { value: dose },
      });
      fireEvent.click(screen.getByTestId("sleep-add-medication"));
      await waitFor(() =>
        expect(screen.queryByTestId("sleep-medication-name")).toBeNull(),
      );
    };
    await addMedication("75");
    await addMedication("37.5");

    const medicationSelect = screen.getByTestId("sleep-intake-medication");
    expect(medicationSelect).toHaveTextContent("venlafaxine — 75 mg");
    expect(medicationSelect).toHaveTextContent("venlafaxine — 37,5 mg");
    expect(screen.getAllByText("venlafaxine")).toHaveLength(2);

    const addIntake = async (medicationId: string, expectedDose: string) => {
      const calls = api.saveSleepEntry.mock.calls.length;
      fireEvent.change(medicationSelect, { target: { value: medicationId } });
      expect(screen.getByTestId("sleep-intake-dose")).toHaveValue(
        Number(expectedDose),
      );
      fireEvent.click(screen.getByTestId("sleep-add-intake"));
      await waitFor(() =>
        expect(api.saveSleepEntry).toHaveBeenCalledTimes(calls + 1),
      );
      await waitFor(() =>
        expect(screen.getByTestId("sleep-save-state")).toHaveTextContent(
          "Enregistré localement",
        ),
      );
    };
    await addIntake(catalog[0].medication_id, "75");
    await addIntake(catalog[1].medication_id, "37.5");
    expect(screen.getByTestId("sleep-agenda-medication")).toHaveTextContent(
      "M ×2",
    );
    expect(screen.getByTestId("sleep-agenda-medication")).toHaveAttribute(
      "title",
      expect.stringContaining("venlafaxine — 37,5 mg"),
    );

    const addEvent = async (type: string, start: string, end?: string) => {
      const calls = api.saveSleepEntry.mock.calls.length;
      fireEvent.change(screen.getByTestId("sleep-event-type"), {
        target: { value: type },
      });
      fireEvent.change(screen.getByTestId("sleep-event-start"), {
        target: { value: start },
      });
      if (end !== undefined)
        fireEvent.change(screen.getByTestId("sleep-event-end"), {
          target: { value: end },
        });
      fireEvent.click(screen.getByTestId("sleep-add-event"));
      await waitFor(() =>
        expect(api.saveSleepEntry).toHaveBeenCalledTimes(calls + 1),
      );
      await waitFor(() =>
        expect(screen.getByTestId("sleep-save-state")).toHaveTextContent(
          "Enregistré localement",
        ),
      );
    };
    await addEvent("bed_time", "22:45");
    expect(screen.getByTestId("sleep-agenda-bed-time")).toHaveTextContent(
      "22:45",
    );
    await addEvent("sleep", "23:15", "03:00");
    expect(screen.getByTestId("sleep-agenda-sleep-duration")).toHaveTextContent(
      "3 h 45 min",
    );
    await addEvent("long_awake", "03:00", "03:30");
    expect(screen.getByTestId("sleep-agenda-long-awake")).toHaveTextContent(
      "1 · 30 min",
    );
    await addEvent("sleep", "03:30", "06:45");
    expect(screen.getByTestId("sleep-agenda-sleep-duration")).toHaveTextContent(
      "7 h 00 min",
    );
    await addEvent("final_get_up", "07:10");
    expect(screen.getByTestId("sleep-agenda-final-get-up")).toHaveTextContent(
      "07:10",
    );
    expect(screen.getByTestId("sleep-agenda-time-in-bed")).toHaveTextContent(
      "8 h 25 min",
    );

    const getUpRow = screen.getByTestId("sleep-event-final_get_up");
    fireEvent.change(within(getUpRow).getByLabelText("Heure"), {
      target: { value: "07:30" },
    });
    await waitFor(() =>
      expect(screen.getByTestId("sleep-agenda-time-in-bed")).toHaveTextContent(
        "8 h 45 min",
      ),
    );

    const sleepRows = screen.getAllByTestId("sleep-event-sleep");
    fireEvent.change(within(sleepRows[1]).getByLabelText("Fin"), {
      target: { value: "07:00" },
    });
    await waitFor(() =>
      expect(
        screen.getByTestId("sleep-agenda-sleep-duration"),
      ).toHaveTextContent("7 h 15 min"),
    );

    fireEvent.click(
      within(screen.getByTestId("sleep-event-long_awake")).getByRole("button", {
        name: "Supprimer Long réveil",
      }),
    );
    await waitFor(() =>
      expect(screen.queryByTestId("sleep-agenda-long-awake")).toBeNull(),
    );
    expect(screen.queryByTestId("sleep-event-long_awake")).toBeNull();
    await addEvent("nap", "14:00", "14:35");
    expect(screen.getByTestId("sleep-agenda-naps")).toHaveTextContent(
      "1 · 35 min",
    );
    await addEvent("daytime_sleepiness", "15:00");
    expect(screen.getByTestId("sleep-agenda-sleepiness")).toHaveTextContent(
      "1 occurrence",
    );
    expect(screen.getByTestId("sleep-workspace")).toBeInTheDocument();
  });

  it("ignores a stale diary read that finishes after a newer persisted revision", async () => {
    let resolveInitial: ((value: SleepSnapshot) => void) | undefined;
    const initial = new Promise<SleepSnapshot>((resolve) => {
      resolveInitial = resolve;
    });
    let persisted: SleepEntry[] = [];
    api.fetchSleepDiary
      .mockImplementationOnce(async () => initial)
      .mockImplementation(async () => snapshotWith(persisted));
    api.saveSleepEntry.mockImplementation(async (input: SleepEntryInput) => {
      const result = {
        entry_id: "sd_00000000-0000-4000-8000-000000000001",
        revision_id: "sdr_00000000-0000-4000-8000-000000000099",
      };
      persisted = [persistedEntry(input, result.entry_id, result.revision_id)];
      return result;
    });

    render(<SleepDiaryWorkspace period="30d" language="fr" />);
    fireEvent.change(screen.getByTestId("sleep-event-type"), {
      target: { value: "bed_time" },
    });
    fireEvent.change(screen.getByTestId("sleep-event-start"), {
      target: { value: "22:45" },
    });
    fireEvent.click(screen.getByTestId("sleep-add-event"));
    expect(
      await screen.findByTestId("sleep-agenda-event-bed_time"),
    ).toBeInTheDocument();

    resolveInitial?.(emptySnapshot);
    await Promise.resolve();
    expect(screen.getByTestId("sleep-agenda-bed-time")).toHaveTextContent(
      "22:45",
    );
  });

  it("serializes autosaves so the next mutation uses the returned revision", async () => {
    let resolveFirst:
      ((value: { entry_id: string; revision_id: string }) => void) | undefined;
    let persisted: SleepEntry[] = [];
    const first = new Promise<{ entry_id: string; revision_id: string }>(
      (resolve) => {
        resolveFirst = resolve;
      },
    );
    api.fetchSleepDiary.mockImplementation(async () => snapshotWith(persisted));
    api.saveSleepEntry
      .mockImplementationOnce(async () => first)
      .mockImplementationOnce(async (input: SleepEntryInput) => {
        const result = {
          entry_id: input.entry_id,
          revision_id: "sdr_00000000-0000-4000-8000-000000000002",
        };
        persisted = [
          persistedEntry(input, result.entry_id, result.revision_id),
        ];
        return result;
      });

    render(<SleepDiaryWorkspace period="30d" language="fr" />);
    await screen.findByTestId("sleep-workspace");
    fireEvent.change(screen.getByTestId("sleep-event-type"), {
      target: { value: "bed_time" },
    });
    fireEvent.click(screen.getByTestId("sleep-add-event"));
    fireEvent.change(screen.getByTestId("sleep-event-type"), {
      target: { value: "sleep" },
    });
    fireEvent.click(screen.getByTestId("sleep-add-event"));
    await waitFor(() => expect(api.saveSleepEntry).toHaveBeenCalledTimes(1));
    expect(api.saveSleepEntry).toHaveBeenCalledTimes(1);

    const firstInput = api.saveSleepEntry.mock.calls[0][0] as SleepEntryInput;
    const firstResult = {
      entry_id: "sd_00000000-0000-4000-8000-000000000001",
      revision_id: "sdr_00000000-0000-4000-8000-000000000001",
    };
    persisted = [
      persistedEntry(firstInput, firstResult.entry_id, firstResult.revision_id),
    ];
    resolveFirst?.(firstResult);

    await waitFor(() => expect(api.saveSleepEntry).toHaveBeenCalledTimes(2));
    expect(api.saveSleepEntry.mock.calls[1][0]).toMatchObject({
      entry_id: firstResult.entry_id,
      expected_revision: firstResult.revision_id,
    });
    await waitFor(() =>
      expect(screen.getByTestId("sleep-save-state")).toHaveTextContent(
        "Enregistré localement",
      ),
    );
    expect(
      screen.getByTestId("sleep-agenda-event-bed_time"),
    ).toBeInTheDocument();
    expect(screen.getByTestId("sleep-agenda-event-sleep")).toBeInTheDocument();
  });

  it("aligns ticks, events, intervals, and medication markers on one 18:00 axis", async () => {
    api.fetchSleepDiary.mockResolvedValue(snapshotWith([dayAEntry()]));
    api.fetchSleepMedications.mockResolvedValue([medication]);
    render(<SleepDiaryWorkspace period="30d" language="fr" />);

    const bedtime = await screen.findByTestId("sleep-agenda-event-bed_time");
    const sleep = screen.getByTestId("sleep-agenda-event-sleep");
    const getUp = screen.getByTestId("sleep-agenda-event-final_get_up");
    const sleepiness = screen.getByTestId(
      "sleep-agenda-event-daytime_sleepiness",
    );
    const intake = screen.getByTestId("sleep-agenda-medication");
    expect(bedtime).toHaveStyle({ left: "18.75%" });
    expect(sleep).toHaveStyle({ left: "18.75%" });
    expect(Number.parseFloat(sleep.style.width)).toBeCloseTo((5 / 24) * 100, 9);
    expect(Number.parseFloat(getUp.style.left)).toBeCloseTo(
      (10.75 / 24) * 100,
      9,
    );
    expect(Number.parseFloat(sleepiness.style.left)).toBeCloseTo(
      (21.5 / 24) * 100,
      9,
    );
    expect(intake).toHaveStyle({ left: "18.75%" });
    const ticks = Array.from(document.querySelectorAll<HTMLElement>(".sleep-hour-axis > span"));
    expect(ticks).toHaveLength(25);
    expect(
      ticks
        .map((tick) => [tick.textContent, tick.style.left])
        .filter((_, index) => index % 2 === 0),
    ).toEqual([
      ["18", "0%"],
      ["20", `${(2 / 24) * 100}%`],
      ["22", `${(4 / 24) * 100}%`],
      ["00", "25%"],
      ["02", `${(8 / 24) * 100}%`],
      ["04", `${(10 / 24) * 100}%`],
      ["06", "50%"],
      ["08", `${(14 / 24) * 100}%`],
      ["10", `${(16 / 24) * 100}%`],
      ["12", "75%"],
      ["14", `${(20 / 24) * 100}%`],
      ["16", `${(22 / 24) * 100}%`],
      ["18", "100%"],
    ]);
    expect(ticks[1]).toHaveClass("sleep-hour-tick-odd");
    expect(ticks[2]).toHaveClass("sleep-hour-tick-even");
    expect(ticks[6]).toHaveClass("sleep-hour-tick-major");
  });

  it("shows the shared estimated sleep range only when factual sleep is absent", async () => {
    const estimated = {
      ...dayAEntry(),
      entry_id: "sl_estimated",
      revision_id: "slr_estimated",
      night_start_date: "2026-09-23",
      night_end_date: "2026-09-24",
      events: [
        {
          event_id: "bed",
          type: "bed_time" as const,
          start_at: "2026-09-23T22:05:00+02:00",
          end_at: null,
        },
        {
          event_id: "up",
          type: "final_get_up" as const,
          start_at: "2026-09-24T04:14:00+02:00",
          end_at: null,
        },
      ],
      intakes: [],
    };
    api.fetchSleepDiary.mockResolvedValue(snapshotWith([estimated]));
    api.fetchSleepMedications.mockResolvedValue([medication]);
    render(<SleepDiaryWorkspace period="30d" language="fr" />);

    const range = await screen.findByTestId("sleep-agenda-sleep-estimated");
    expect(range).toHaveAttribute(
      "aria-label",
      "Sommeil estimé 22:50 → 04:04",
    );
    expect(Number.parseFloat(range.style.left)).toBeCloseTo((4.833333333 / 24) * 100, 7);
    expect(Number.parseFloat(range.style.width)).toBeCloseTo((5.233333333 / 24) * 100, 7);
    expect(screen.queryByTestId("sleep-agenda-event-sleep")).not.toBeInTheDocument();
  });

  it("isolates an empty editor night while retaining the period agenda", async () => {
    api.fetchSleepDiary.mockResolvedValue(snapshotWith([dayAEntry()]));
    api.fetchSleepMedications.mockResolvedValue([medication]);
    render(<SleepDiaryWorkspace period="30d" language="fr" />);

    expect(
      await screen.findByTestId("sleep-agenda-event-bed_time"),
    ).toBeInTheDocument();
    expect(screen.getByTestId("sleep-publication-status")).toHaveTextContent(
      "Journée validée",
    );
    expect(screen.queryByText("Prête à synchroniser")).toBeNull();
    expect(
      screen.getByLabelText("Traitement et remarques particulières"),
    ).toHaveValue("Observation du jour A");

    fireEvent.change(screen.getByTestId("sleep-night-start"), {
      target: { value: "2026-09-21" },
    });
    expect(screen.getByTestId("sleep-night-end")).toHaveValue("2026-09-22");
    expect(screen.queryByTestId("sleep-event-bed_time")).toBeNull();
    expect(screen.queryByTestId("sleep-intake-row")).toBeNull();
    expect(screen.getByTestId("sleep-timeline")).toHaveTextContent(
      "Aucun événement enregistré pour cette nuit.",
    );
    expect(screen.getByTestId("sleep-agenda-event-bed_time")).toBeInTheDocument();
    expect(
      screen.getByLabelText("Traitement et remarques particulières"),
    ).toHaveValue("");
    expect(
      within(
        screen.getByRole("group", { name: "Qualité du sommeil" }),
      ).queryByRole("button", { pressed: true }),
    ).toBeNull();
    const summary = screen.getByRole("heading", { name: "Synthèse factuelle" })
      .parentElement as HTMLElement;
    expect(summary).toHaveTextContent("Sommeil déclaré—");
    expect(summary).toHaveTextContent("Somnolence—");
    expect(summary).toHaveTextContent("Prises—");

    await waitFor(() => expect(api.fetchSleepDiary).toHaveBeenCalledTimes(2));
    fireEvent.change(screen.getByTestId("sleep-night-start"), {
      target: { value: "2026-09-20" },
    });
    expect(
      await screen.findByTestId("sleep-event-bed_time"),
    ).toBeInTheDocument();
    expect(screen.getByTestId("sleep-intake-row")).toHaveTextContent("TestMed");
    expect(
      screen.getByLabelText("Traitement et remarques particulières"),
    ).toHaveValue("Observation du jour A");
    expect(screen.getByTestId("sleep-publication-status")).toHaveTextContent(
      "Journée validée",
    );
  });

  it("does not let a slow previous-night fetch contaminate the active empty night", async () => {
    let resolveNightA: ((value: SleepSnapshot) => void) | undefined;
    const slowNightA = new Promise<SleepSnapshot>((resolve) => {
      resolveNightA = resolve;
    });
    api.fetchSleepDiary
      .mockImplementationOnce(async () => slowNightA)
      .mockResolvedValue(emptySnapshot);
    render(<SleepDiaryWorkspace period="30d" language="fr" />);

    fireEvent.change(screen.getByTestId("sleep-night-start"), {
      target: { value: "2026-09-21" },
    });
    await waitFor(() => expect(api.fetchSleepDiary).toHaveBeenCalledTimes(2));
    expect(screen.getByTestId("sleep-night-start")).toHaveValue("2026-09-21");
    expect(
      screen.getByText("Aucune donnée pour cette nuit."),
    ).toBeInTheDocument();

    resolveNightA?.(snapshotWith([dayAEntry()]));
    await Promise.resolve();
    expect(screen.getByTestId("sleep-night-start")).toHaveValue("2026-09-21");
    expect(screen.queryByTestId("sleep-event-bed_time")).toBeNull();
    expect(screen.queryByTestId("sleep-intake-row")).toBeNull();
    expect(
      screen.getByText("Aucune donnée pour cette nuit."),
    ).toBeInTheDocument();
  });

  it("presents validation as a local completion state without transport wording", async () => {
    let current: SleepEntry = {
      ...dayAEntry(),
      publication_status: "draft",
    };
    api.fetchSleepDiary.mockImplementation(async () => snapshotWith([current]));
    api.saveSleepEntry.mockImplementation(async (input: SleepEntryInput) => {
      current = {
        ...persistedEntry(input, current.entry_id, current.revision_id),
        publication_status: "draft",
      };
      return {
        entry_id: current.entry_id,
        revision_id: current.revision_id,
      };
    });
    api.validateSleepEntry.mockImplementation(async () => {
      current = { ...current, publication_status: "ready" };
    });
    render(<SleepDiaryWorkspace period="30d" language="fr" />);
    expect(
      await screen.findByTestId("sleep-publication-status"),
    ).toHaveTextContent("Brouillon");

    fireEvent.click(screen.getByTestId("sleep-validate-day"));
    await waitFor(() =>
      expect(screen.getByTestId("sleep-publication-status")).toHaveTextContent(
        "Journée validée",
      ),
    );
    expect(screen.queryByText("Prête à synchroniser")).toBeNull();
    expect(screen.getByTestId("sleep-save-state")).toHaveTextContent(
      "Enregistré localement",
    );
  });
});
