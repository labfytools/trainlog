import {
  fireEvent,
  render,
  screen,
  waitFor,
  within,
} from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import type { SleepMedication, SleepSnapshot } from "../api/sleepDiary";
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

describe("SleepDiaryWorkspace", () => {
  beforeEach(() => {
    vi.clearAllMocks();
    api.nextId = 0;
    api.fetchSleepDiary.mockResolvedValue(emptySnapshot);
    api.fetchSleepMedications.mockResolvedValue([]);
    let revision = 0;
    api.saveSleepEntry.mockImplementation(async (input) => ({
      entry_id: input.entry_id || "sd_00000000-0000-4000-8000-000000000001",
      revision_id: `sdr_00000000-0000-4000-8000-${String(++revision).padStart(12, "0")}`,
    }));
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
});
