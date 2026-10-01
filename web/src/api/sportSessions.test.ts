import { afterEach, describe, expect, it, vi } from "vitest";
import { fetchSportSessions } from "./sportSessions";

afterEach(() => vi.unstubAllGlobals());

describe("completed sport history read", () => {
  const item = (index: number) => ({
    identity: `se_${index}`,
    label: `Session ${index}`,
    session_type: "training",
    started_at: `2026-10-01T0${index}:00:00+02:00`,
    ended_at: null,
  });

  it("follows all pages and retains each factual stable identity", async () => {
    const fetch = vi.fn()
      .mockResolvedValueOnce({ ok: true, json: async () => ({
        api_version: 1, offset: 0, more: true, next_offset: 64, items: [item(1)],
      }) })
      .mockResolvedValueOnce({ ok: true, json: async () => ({
        api_version: 1, offset: 64, more: false, next_offset: 128, items: [item(2)],
      }) });
    vi.stubGlobal("fetch", fetch);
    const sessions = await fetchSportSessions("2026-10-01T00:00:00Z", "2026-10-02T00:00:00Z");
    expect(sessions.map((session) => session.identity)).toEqual(["se_1", "se_2"]);
    expect(String(fetch.mock.calls[1][0])).toContain("offset=64");
  });

  it("fails closed on stale cursors and duplicate identities", async () => {
    vi.stubGlobal("fetch", vi.fn()
      .mockResolvedValueOnce({ ok: true, json: async () => ({
        api_version: 1, offset: 0, more: true, next_offset: 64, items: [item(1)],
      }) })
      .mockResolvedValueOnce({ ok: true, json: async () => ({
        api_version: 1, offset: 64, more: false, next_offset: 128, items: [item(1)],
      }) }));
    await expect(fetchSportSessions("a", "b")).rejects.toThrow(/Duplicate/);
  });
});
