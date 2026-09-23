import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { AnalysisPage } from "./AnalysisPage";
import { analysisFixture } from "../api/analysis.test";
import type { AnalysisSnapshot } from "../api/analysis";

describe("AnalysisPage", () => {
  beforeEach(() => {
    document.documentElement.lang = "fr";
    window.history.replaceState(
      null,
      "",
      "/analyse?section=invalid&period=broken&metric=unknown&exercise_id=nope",
    );
    vi.spyOn(globalThis, "fetch").mockResolvedValue(
      new Response(JSON.stringify(analysisFixture), { status: 200 }),
    );
  });
  afterEach(() => vi.restoreAllMocks());

  it("falls back safely and changes sections, periods and metrics", async () => {
    render(<AnalysisPage />);
    expect(
      await screen.findByRole("heading", { name: "Analyse" }),
    ).toBeInTheDocument();
    expect(screen.getByRole("tab", { name: "Vue d’ensemble" })).toHaveAttribute(
      "aria-selected",
      "true",
    );
    expect(screen.getByLabelText("Période")).toHaveValue("30d");
    fireEvent.change(screen.getByLabelText("Période"), {
      target: { value: "7d" },
    });
    await waitFor(() =>
      expect(globalThis.fetch).toHaveBeenLastCalledWith(
        expect.stringContaining("period=7d"),
        expect.anything(),
      ),
    );
    fireEvent.click(screen.getByRole("tab", { name: "Mensurations" }));
    fireEvent.change(screen.getByLabelText("Mesure"), {
      target: { value: "waist" },
    });
    expect(window.location.search).toContain("section=measurements");
  });

  it("supports exercise and BODY ZONE deep-link sections without duplicating the global language control", async () => {
    window.history.replaceState(
      null,
      "",
      `/analyse?section=exercise&exercise_id=${analysisFixture.exercises[0].exercise_id}&period=30d`,
    );
    render(<AnalysisPage />);
    expect(
      await screen.findByRole("heading", { name: "Presse" }),
    ).toBeInTheDocument();
    fireEvent.click(screen.getByRole("tab", { name: "Répartition" }));
    expect(screen.getByText(/pourcentages physiologiques/)).toBeInTheDocument();
    expect(screen.queryByLabelText("Langue")).not.toBeInTheDocument();
    expect(document.querySelector(".analysis-zone-track span")).toHaveStyle({
      width: "100%",
    });
    expect(document.body).not.toHaveTextContent("%");
  });

  it("humanizes durations and hides set counts for a continuous exercise", async () => {
    const continuous: AnalysisSnapshot = {
      ...analysisFixture,
      exercises: [
        {
          ...analysisFixture.exercises[0],
          name: "Marche",
          tracking_mode: "duration",
          recording_mode: "continuous",
        },
      ],
      exercise: {
        ...analysisFixture.exercise!,
        name: "Marche",
        tracking_mode: "duration",
        recording_mode: "continuous",
        points: [
          {
            ...analysisFixture.exercise!.points[0],
            sets: 0,
            reps: null,
            external_load_kg: null,
            external_volume_kg: null,
            explicit_max_kg: null,
            continuous_duration_seconds: 900,
            distance_km: 1.43,
            speed_kmh: 6.4,
          },
        ],
      },
    };
    vi.mocked(globalThis.fetch).mockResolvedValue(
      new Response(JSON.stringify(continuous), { status: 200 }),
    );
    window.history.replaceState(
      null,
      "",
      `/analyse?section=exercise&exercise_id=${continuous.exercises[0].exercise_id}&period=30d`,
    );
    render(<AnalysisPage />);
    expect(
      await screen.findByRole("heading", { name: "Marche" }),
    ).toBeInTheDocument();
    expect(screen.getByText("15 min")).toBeInTheDocument();
    expect(screen.getByText("1,43 km")).toBeInTheDocument();
    expect(screen.getByText("6,4 km/h")).toBeInTheDocument();
    expect(screen.queryByText("Séries")).not.toBeInTheDocument();
    expect(screen.getByRole("tab", { name: "Durée" })).toBeInTheDocument();
    expect(screen.getByRole("tab", { name: "Distance" })).toBeInTheDocument();
    expect(screen.getByRole("tab", { name: "Vitesse" })).toBeInTheDocument();
    expect(
      screen.queryByRole("tab", { name: "Charge" }),
    ).not.toBeInTheDocument();
    expect(
      screen.getByText(/Durée · min · 1 points réels/),
    ).toBeInTheDocument();
  });

  it("offers only available loaded-repetition metrics and labels chart axes", async () => {
    window.history.replaceState(
      null,
      "",
      `/analyse?section=exercise&exercise_id=${analysisFixture.exercises[0].exercise_id}&period=30d`,
    );
    render(<AnalysisPage />);
    expect(await screen.findByRole("tab", { name: "Charge" })).toHaveAttribute(
      "aria-selected",
      "true",
    );
    expect(
      screen.getByRole("tab", { name: "Répétitions" }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole("tab", { name: "Volume externe" }),
    ).toBeInTheDocument();
    expect(screen.getByRole("tab", { name: "MAX" })).toBeInTheDocument();
    expect(
      screen.queryByRole("tab", { name: "Durée" }),
    ).not.toBeInTheDocument();
    expect(
      screen.getByText(/Charge · kg · 1 points réels/),
    ).toBeInTheDocument();
    expect(document.querySelectorAll(".chart-x-label")).toHaveLength(1);
    expect(document.querySelectorAll(".chart-y-label")).toHaveLength(2);
  });

  it("renders a neutral factual measurement delta with first and last dates", async () => {
    const summaries = analysisFixture.measurements.summaries.map((summary) =>
      summary.metric === "weight"
        ? { ...summary, count: 2, first: 83.7, last: 85.1, delta: 1.4 }
        : summary,
    );
    const measurements: AnalysisSnapshot = {
      ...analysisFixture,
      measurements: {
        selected_metric: "weight",
        unit: "kg",
        summaries,
        series: [{
          metric: "weight",
          unit: "kg",
          points: [
            { timestamp: "2026-08-20T08:00:00Z", value: 83.7 },
            { timestamp: "2026-09-20T08:00:00Z", value: 85.1 },
          ],
        }],
        points: [
          { timestamp: "2026-08-20T08:00:00Z", value: 83.7 },
          { timestamp: "2026-09-20T08:00:00Z", value: 85.1 },
        ],
      },
    };
    vi.mocked(globalThis.fetch).mockResolvedValue(
      new Response(JSON.stringify(measurements), { status: 200 }),
    );
    window.history.replaceState(
      null,
      "",
      "/analyse?section=measurements&metric=weight&period=30d",
    );
    render(<AnalysisPage />);
    const delta = await screen.findByText("+1,4 kg");
    expect(delta.parentElement).toHaveClass("analysis-delta");
    expect(screen.getByText(/Poids · kg · 2 points réels/)).toBeInTheDocument();
    expect(document.querySelectorAll(".chart-x-label")).toHaveLength(2);
  });

  it("uses a factual exercises-by-date histogram in the overview", async () => {
    render(<AnalysisPage />);
    expect(await screen.findByText("Exercices réalisés par date")).toBeInTheDocument();
    expect(
      screen.getByRole("img", { name: "Histogramme des exercices réalisés par date" }),
    ).toBeInTheDocument();
    expect(document.querySelector(".activity-bar")).toHaveAttribute(
      "title",
      expect.stringContaining("1 exercice(s)"),
    );
  });
});
