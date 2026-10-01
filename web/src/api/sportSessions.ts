export interface SportSession {
  identity: string;
  session_type: string;
  label: string;
  started_at: string;
  ended_at: string | null;
}

interface SportPage {
  api_version: 1;
  offset: number;
  more: boolean;
  next_offset: number;
  items: SportSession[];
}

function validSession(value: unknown): value is SportSession {
  if (typeof value !== "object" || value === null) return false;
  const item = value as Record<string, unknown>;
  return typeof item.identity === "string" &&
    typeof item.session_type === "string" &&
    typeof item.label === "string" &&
    typeof item.started_at === "string" &&
    (item.ended_at === null || typeof item.ended_at === "string");
}

/** CONTRACT: each read is a fresh factual history projection. Pagination must
 * complete before callers may present absence or permit a complete PDF. */
export async function fetchSportSessions(
  start: string,
  end: string,
  signal?: AbortSignal,
): Promise<SportSession[]> {
  const sessions: SportSession[] = [];
  const ids = new Set<string>();
  let offset = 0;
  for (;;) {
    const query = new URLSearchParams({ start, end, offset: String(offset), limit: "64" });
    const response = await fetch(`/api/v1/sessions/sport?${query}`, { signal });
    if (!response.ok) throw new Error(`Sport history HTTP ${response.status}`);
    const page: SportPage = await response.json();
    if (page.api_version !== 1 || page.offset !== offset ||
        !Array.isArray(page.items) || !page.items.every(validSession) ||
        typeof page.more !== "boolean" ||
        !Number.isSafeInteger(page.next_offset) || page.next_offset <= offset) {
      throw new TypeError("Invalid sport history page");
    }
    for (const item of page.items) {
      if (ids.has(item.identity)) throw new TypeError("Duplicate sport session identity");
      ids.add(item.identity);
      sessions.push(item);
    }
    if (!page.more) return sessions;
    if (page.items.length === 0) throw new TypeError("Empty sport history page");
    offset = page.next_offset;
  }
}
