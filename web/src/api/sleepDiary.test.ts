import { describe, expect, it, vi } from "vitest";
import { fetchSleepDiary, newSleepId, sleepTimestamp } from "./sleepDiary";

describe("sleepTimestamp", () => {
  it("serializes browser timestamps at the C17 contract precision", () => {
    expect(sleepTimestamp(new Date("2026-09-21T05:51:41.140Z"))).toBe(
      "2026-09-21T05:51:41Z",
    );
  });
});

describe("newSleepId", () => {
  it("creates UUIDv4 sleep identities without randomUUID", () => {
    vi.stubGlobal("crypto", {
      getRandomValues: (bytes: Uint8Array) => {
        bytes.set(Array.from({ length: 16 }, (_, index) => index));
        return bytes;
      },
    });
    expect(newSleepId("sle")).toBe("sle_00010203-0405-4607-8809-0a0b0c0d0e0f");
    expect(newSleepId("mdi")).toBe("mdi_00010203-0405-4607-8809-0a0b0c0d0e0f");
    vi.unstubAllGlobals();
  });
});

describe("fetchSleepDiary", () => {
  it("refuses to call a capped unbounded history complete", async () => {
    const entry = {
      entry_id: "sd_test",
      revision_id: "sdr_test",
      night_start_date: "2026-09-20",
      night_end_date: "2026-09-21",
      created_at: "2026-09-20T18:00:00+02:00",
      updated_at: "2026-09-21T08:00:00+02:00",
      sleep_quality: null,
      wake_quality: null,
      day_form: null,
      treatment_and_notes: "",
      publication_status: "draft",
      events: [],
      intakes: [],
    };
    const entries = Array.from({ length: 3660 }, (_, index) => ({
      ...entry,
      entry_id: `sd_${index}`,
    }));
    const response = { api_version: 1, entries, summary: { nights: 3660 } };
    const fetch = vi.spyOn(globalThis, "fetch").mockImplementation(
      async () => new Response(JSON.stringify(response)),
    );
    try {
      await expect(fetchSleepDiary()).rejects.toThrow("sleep_diary_history_limit_reached");
      expect(fetch.mock.calls[0][0]).toContain("limit=3660");
    } finally {
      fetch.mockRestore();
    }
  });
});
