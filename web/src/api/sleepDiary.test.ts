import { describe, expect, it, vi } from "vitest";
import { newSleepId, sleepTimestamp } from "./sleepDiary";

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
