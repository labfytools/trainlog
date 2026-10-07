import type {
  SleepEntry,
  SleepEventType,
  SleepSnapshot,
} from "../api/sleepDiary";
import { formatDose, formatDuration } from "../dashboard/dashboardFormat";
import { projectSleepTimeline } from "../routes/sleepVisualProjection";
import { projectSportSegments, sleepRowWindow, timelinePosition } from "../routes/sleepTimelineGeometry";
import type { SportSession } from "../api/sportSessions";

type Language = "fr" | "en";

const eventLabels: Record<Language, Record<SleepEventType, string>> = {
  fr: {
    bed_time: "Mise au lit",
    final_get_up: "Lever",
    night_get_up: "Lever nocturne",
    sleep: "SOMMEIL",
    nap: "SIESTE",
    long_awake: "LONG RÉVEIL",
    half_sleep: "DEMI-SOMMEIL",
    daytime_sleepiness: "Somnolence",
  },
  en: {
    bed_time: "Bedtime",
    final_get_up: "Final get-up",
    night_get_up: "Night get-up",
    sleep: "SLEEP",
    nap: "NAP",
    long_awake: "LONG AWAKENING",
    half_sleep: "HALF-SLEEP",
    daytime_sleepiness: "Sleepiness",
  },
};

/*
 * WHY: the local vector report uses PDF's built-in Helvetica and must retain
 * French accents without embedding a second font asset.
 * CONTRACT: user-facing strings are emitted as WinAnsi octal bytes and the
 * font dictionary declares the same encoding.
 * INVARIANT: content streams stay ASCII, so byte offsets and lengths remain
 * deterministic under TextEncoder.
 */
const winAnsi: Record<string, number> = {
  "€": 0x80,
  "‚": 0x82,
  ƒ: 0x83,
  "„": 0x84,
  "…": 0x85,
  "†": 0x86,
  "‡": 0x87,
  ˆ: 0x88,
  "‰": 0x89,
  Š: 0x8a,
  "‹": 0x8b,
  Œ: 0x8c,
  Ž: 0x8e,
  "‘": 0x91,
  "’": 0x92,
  "“": 0x93,
  "”": 0x94,
  "•": 0x95,
  "–": 0x96,
  "—": 0x97,
  "˜": 0x98,
  "™": 0x99,
  š: 0x9a,
  "›": 0x9b,
  œ: 0x9c,
  ž: 0x9e,
  Ÿ: 0x9f,
};

const pdfString = (value: string) =>
  Array.from(value, (character) => {
    const codePoint = character.codePointAt(0) ?? 0x3f;
    if (character === "\\" || character === "(" || character === ")")
      return `\\${character}`;
    if (codePoint >= 0x20 && codePoint <= 0x7e) return character;
    const byte =
      codePoint >= 0xa0 && codePoint <= 0xff
        ? codePoint
        : (winAnsi[character] ?? 0x3f);
    return `\\${byte.toString(8).padStart(3, "0")}`;
  }).join("");

const text = (x: number, y: number, size: number, value: string,
  color = "0 g") =>
  `${color} BT /F1 ${size} Tf ${x.toFixed(1)} ${y.toFixed(1)} Td (${pdfString(value)}) Tj ET\n`;

// CONTRACT: restrained accents retain contrast when the report is printed in
// grayscale; the blue sport mark remains outlined independently of color.
const pdfAccent = "0.25 0.23 0.40 rg";
const pdfMuted = "0.33 0.34 0.40 rg";

// CONTRACT: all timeline marks, grid ticks, headers, and sport bands use these
// same coordinates. The notes column gets over twice its former 84 pt width.
const layout = {
  left: 30,
  right: 812,
  dateX: 33,
  timelineX: 94,
  timelineWidth: 430,
  qualityX: [528, 562, 596],
  notesX: 632,
  notesRight: 812,
  rowTop: 538,
  rowBottom: 137,
  minRowHeight: 40,
  noteFont: 7.2,
  noteLeading: 9.5,
  notePadding: 5,
} as const;

type NoteLine = { value: string; separator?: boolean };
type RowFragment = { entry: SleepEntry; lines: NoteLine[]; first: boolean; height: number };

// WHY: PDF Helvetica is not monospaced. Wrapping by character count loses
// content or puts it outside the cell. These are the built-in Helvetica widths
// in thousandths of an em; unsupported Unicode uses the same '?' fallback as
// pdfString. An extra margin absorbs viewer/font rounding differences.
const helveticaAscii = [
  278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,
  556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,
  1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
  667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,
  333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
  556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584,
];
const glyphWidth = (character: string) => {
  const code = character.codePointAt(0) ?? 63;
  if (code >= 32 && code <= 126) return helveticaAscii[code - 32];
  const base = character.normalize("NFD").charAt(0);
  const baseCode = base.codePointAt(0) ?? 63;
  if (baseCode >= 32 && baseCode <= 126) return helveticaAscii[baseCode - 32];
  return character === "œ" ? 833 : character === "Œ" ? 1000 : 600;
};
const lineWidth = (value: string) =>
  Array.from(value).reduce((sum, character) => sum + glyphWidth(character), 0) * layout.noteFont / 1000;
const maxNoteWidth = layout.notesRight - layout.notesX - layout.notePadding * 2 - 4;

function wrapParagraph(paragraph: string): string[] {
  if (paragraph === "") return [""];
  const words = paragraph.match(/\S+|\s+/gu) ?? [];
  const lines: string[] = [];
  let current = "";
  for (const word of words) {
    if (/^\s+$/u.test(word) && current === "") continue;
    if (lineWidth(current + word) <= maxNoteWidth) {
      current += word;
      continue;
    }
    if (current.trim()) lines.push(current.trimEnd());
    current = "";
    if (/^\s+$/u.test(word)) continue;
    if (lineWidth(word) <= maxNoteWidth) {
      current = word;
      continue;
    }
    for (const character of Array.from(word)) {
      if (current && lineWidth(current + character) > maxNoteWidth) {
        lines.push(current);
        current = "";
      }
      current += character;
    }
  }
  if (current) lines.push(current.trimEnd());
  return lines;
}

function noteLines(entry: SleepEntry, language: Language): NoteLine[] {
  const lines: NoteLine[] = [];
  if (entry.intakes.length) {
    lines.push({ value: language === "fr" ? "TRAITEMENTS" : "TREATMENTS" });
    // INVARIANT: a night is newest-first only as a whole. Its timed content
    // remains chronological even if a caller supplied unsorted intake rows.
    for (const intake of [...entry.intakes].sort((left, right) =>
      Date.parse(left.taken_at) - Date.parse(right.taken_at) ||
      left.intake_id.localeCompare(right.intake_id))) {
      const dose = intake.dose_value === null ? "" :
        ` — ${formatDose(intake.dose_value)}${intake.dose_unit ? ` ${intake.dose_unit}` : ""}`;
      const quantity = intake.quantity > 1 ? ` ×${intake.quantity}` : "";
      for (const value of wrapParagraph(`${intake.taken_at.slice(11, 16)} — ${intake.medication_name}${dose}${quantity}`))
        lines.push({ value });
    }
  }
  if (entry.treatment_and_notes) {
    lines.push({ value: language === "fr" ? "REMARQUES" : "NOTES", separator: lines.length > 0 });
    for (const paragraph of entry.treatment_and_notes.split(/\r\n|\n|\r/u))
      for (const value of wrapParagraph(paragraph)) lines.push({ value });
  }
  return lines;
}

const rowHeight = (lineCount: number) =>
  Math.max(layout.minRowHeight, 9 + lineCount * layout.noteLeading + 4);

// INVARIANT: a fragment is emitted only when it fits. Oversized entries are
// split into dated continuations; every note line belongs to exactly one page.
function paginate(entries: readonly SleepEntry[], language: Language): RowFragment[][] {
  const pages: RowFragment[][] = [[]];
  let remaining = layout.rowTop - layout.rowBottom;
  const fresh = () => {
    pages.push([]);
    remaining = layout.rowTop - layout.rowBottom;
  };
  for (const entry of entries) {
    const lines = noteLines(entry, language);
    let offset = 0;
    let first = true;
    do {
      const fullHeight = rowHeight(lines.length - offset);
      if (fullHeight > remaining && pages[pages.length - 1].length > 0) fresh();
      const maxLines = Math.floor((remaining - 13) / layout.noteLeading);
      const count = Math.min(lines.length - offset, Math.max(1, maxLines));
      const fragmentLines = lines.slice(offset, offset + count);
      const height = rowHeight(fragmentLines.length);
      if (height > remaining) throw new Error("PDF row exceeds available page height");
      pages[pages.length - 1].push({ entry, lines: fragmentLines, first, height });
      remaining -= height;
      offset += count;
      first = false;
      if (offset < lines.length) fresh();
    } while (offset < lines.length);
  }
  return pages;
}

const compactDate = (isoDate: string) => {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(isoDate)) return isoDate;
  const [year, month, day] = isoDate.split("-");
  return `${day}/${month}/${year}`;
};

const plural = (count: number, singular: string, pluralForm: string) =>
  `${count} ${count === 1 ? singular : pluralForm}`;

const duration = (seconds: number, language: Language) => {
  if (language === "fr")
    return formatDuration(seconds).replace(/ h 00 min$/, " h");
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  if (hours > 0)
    return `${hours} h${minutes > 0 ? ` ${minutes.toString().padStart(2, "0")} min` : ""}`;
  return `${minutes} min`;
};

const pointMarker = (type: SleepEventType) => {
  if (type === "bed_time") return "v";
  if (type === "final_get_up" || type === "night_get_up") return "^";
  return type === "daytime_sleepiness" ? "S" : "";
};

function row(
  stream: string[],
  fragment: RowFragment,
  y: number,
  language: Language,
  sport: readonly SportSession[],
  shaded: boolean,
) {
  const { entry, lines, first, height } = fragment;
  const projection = projectSleepTimeline(entry);
  const window = sleepRowWindow(entry);
  const timelineX = layout.timelineX;
  const width = layout.timelineWidth;
  if (shaded) stream.push(`q 0.975 0.974 0.985 rg ${layout.left} ${y - height} ` +
    `${layout.right - layout.left} ${height} re f Q\n`);
  stream.push(`0.75 G ${layout.left} ${y - height} ${layout.right - layout.left} ${height} re S\n`);
  stream.push(`q 0.40 0.35 0.57 rg ${layout.left} ${y - height} 2 ${height} re f Q\n`);
  stream.push(text(layout.dateX, y - 13, 6.5, compactDate(entry.night_start_date), pdfAccent));
  stream.push(
    text(
      layout.dateX,
      y - 23,
      5,
      `${language === "fr" ? "au" : "to"} ${compactDate(entry.night_end_date)}`,
    ),
  );
  if (!first) stream.push(text(layout.dateX, y - 34, 6, language === "fr" ? "suite" : "continued"));
  if (first) {
    for (let hour = 0; hour <= 24; hour++) {
      const x = timelineX + (hour / 24) * width;
      stream.push(`0.9 G ${x} ${y - 40} m ${x} ${y} l S\n`);
    }
    [...projection.sleep, ...projection.awake, ...projection.otherIntervals]
      .sort((left, right) => left.start - right.start || left.end - right.end)
      .forEach((range) => {
        const event = range.event;
        const x =
          timelineX +
          timelinePosition(range.start, window.start, window.end) * width;
        // INVARIANT: every projected interval keeps its canonical absolute
        // start/end. An awakening interrupts rather than moves adjacent sleep;
        // an estimated band is presentation-only and stays visually distinct.
        const eventWidth = Math.max(
          2,
          timelineX +
            timelinePosition(range.end, window.start, window.end) * width -
            x,
        );
        const shade = range.kind === "sleep"
          ? range.estimated ? 0.88 : 0.75
          : event?.type === "half_sleep" ? 0.87 : 0.93;
        if (range.kind === "awake") {
          // CONTRACT: structured awakening intervals use the same orange semantic
          // role in Web, HR, preview, and PDF; point markers remain separate.
          stream.push(
            `0.98 0.70 0.53 rg ${x.toFixed(2)} ${(y - 29).toFixed(2)} ${eventWidth.toFixed(2)} 18.00 re f 0.78 0.42 0.22 RG ${x.toFixed(2)} ${(y - 29).toFixed(2)} ${eventWidth.toFixed(2)} 18.00 re S 0 G 0 g\n`,
          );
        } else {
          stream.push(
            `${shade} g ${x.toFixed(2)} ${(y - 29).toFixed(2)} ${eventWidth.toFixed(2)} 18.00 re f 0 G ${x.toFixed(2)} ${(y - 29).toFixed(2)} ${eventWidth.toFixed(2)} 18.00 re S\n`,
          );
        }
        if (eventWidth > 24) {
          const label = event
            ? eventLabels[language][event.type]
            : language === "fr" ? "SOMMEIL ESTIMÉ" : "ESTIMATED SLEEP";
          stream.push(text(x + 2, y - 23, 5, label));
        }
      });
    projection.pointEvents.forEach((event) => {
      const x = timelineX +
        timelinePosition(event.start_at, window.start, window.end) * width;
      stream.push(text(x, y - 25, 8, pointMarker(event.type)));
    });
    projection.intakes.forEach((intake) => {
      const x =
        timelineX +
        timelinePosition(intake.taken_at, window.start, window.end) * width;
      stream.push(text(x, y - 36, 6, "M"));
    });
    stream.push(text(layout.qualityX[0], y - 16, 7, entry.sleep_quality ?? "-"));
    stream.push(text(layout.qualityX[1], y - 16, 7, entry.wake_quality ?? "-"));
    stream.push(text(layout.qualityX[2], y - 16, 7, entry.day_form ?? "-"));
    projectSportSegments(sport, window).forEach((segment) => {
      const x = timelineX + segment.left * width;
      const segmentWidth = segment.end === null ? 2 : Math.max(2, segment.width * width);
      // PDF graphics state contains the blue fill and black outline. It cannot
      // tint later sleep marks, grid lines, medication labels, or body text.
      stream.push(`q 0.10 0.34 0.82 rg 0 G ${x.toFixed(2)} ${(y - 9).toFixed(2)} ` +
        `${segmentWidth.toFixed(2)} 5 re B Q\n`);
    });
  }
  lines.forEach((line, index) => {
    const baseline = y - 12 - index * layout.noteLeading;
    if (line.separator) stream.push(`0.8 G ${layout.notesX + 3} ${(baseline + 5).toFixed(2)} m ${layout.notesRight - 3} ${(baseline + 5).toFixed(2)} l S 0 G\n`);
    const heading = line.value === "TRAITEMENTS" || line.value === "TREATMENTS" ||
      line.value === "REMARQUES" || line.value === "NOTES";
    stream.push(text(layout.notesX + layout.notePadding, baseline, layout.noteFont,
      line.value, heading ? pdfAccent : "0 g"));
  });
}

function reportHeader(title: string, index: number, count: number,
  language: Language, first: string, last: string, generatedAt: Date,
  dividerY = 555): string[] {
  const generationDate = new Intl.DateTimeFormat(language === "fr" ? "fr-FR" : "en-GB",
    { year: "numeric", month: "2-digit", day: "2-digit" }).format(generatedAt);
  return [
    "0 G 0 g\n",
    text(30, 575, 15, title, pdfAccent),
    text(760, 575, 7, `${index + 1}/${count}`, pdfMuted),
    text(30, 560, 7,
      `${language === "fr" ? "Période" : "Period"} : ${compactDate(first)} – ${compactDate(last)}`,
      pdfMuted),
    text(700, 560, 6,
      `${language === "fr" ? "Généré le" : "Generated"} ${generationDate}`, pdfMuted),
    `0.55 0.51 0.69 RG 30 ${dividerY} m 812 ${dividerY} l S 0 G\n`,
  ];
}

function summaryLine(snapshot: SleepSnapshot, language: Language) {
  if (language === "fr")
    return [
      plural(snapshot.summary.nights, "nuit", "nuits"),
      `Sommeil déclaré : ${duration(snapshot.summary.sleep_duration_seconds, language)}`,
      `Longs réveils : ${snapshot.summary.long_awake_count}`,
      `Siestes : ${snapshot.summary.nap_count}`,
      `Somnolences : ${snapshot.summary.sleepiness_count}`,
      `Prises de médicaments : ${snapshot.summary.intake_count}`,
    ].join(" · ");
  return [
    plural(snapshot.summary.nights, "night", "nights"),
    `Declared sleep: ${duration(snapshot.summary.sleep_duration_seconds, language)}`,
    `Long awakenings: ${snapshot.summary.long_awake_count}`,
    `Naps: ${snapshot.summary.nap_count}`,
    `Sleepiness: ${snapshot.summary.sleepiness_count}`,
    `Medication intakes: ${snapshot.summary.intake_count}`,
  ].join(" · ");
}

function page(
  snapshot: SleepSnapshot,
  fragments: RowFragment[],
  index: number,
  count: number,
  language: Language,
  sport: readonly SportSession[],
  first: string,
  last: string,
  generatedAt: Date,
) {
  const stream = reportHeader(language === "fr" ? "Trainlog — Agenda sommeil" :
    "Trainlog — Sleep diary", index, count, language, first, last, generatedAt, 540);
  stream.push(text(30, 548, 7, "DATE", pdfAccent));
  for (let hour = 0; hour < 24; hour++)
    stream.push(
      text(layout.timelineX + (hour / 24) * layout.timelineWidth, 548, 4, String((18 + hour) % 24)),
    );
  if (language === "fr") {
    stream.push(text(layout.qualityX[0], 552, 4, "QUALITÉ DU"));
    stream.push(text(layout.qualityX[0], 545, 4, "SOMMEIL"));
    stream.push(text(layout.qualityX[1], 552, 4, "QUALITÉ DU"));
    stream.push(text(layout.qualityX[1], 545, 4, "RÉVEIL"));
    stream.push(text(layout.qualityX[2], 552, 4, "FORME DE LA"));
    stream.push(text(layout.qualityX[2], 545, 4, "JOURNÉE"));
    stream.push(text(layout.notesX, 552, 4, "TRAITEMENT ET"));
    stream.push(text(layout.notesX, 545, 4, "REMARQUES"));
  } else {
    stream.push(text(layout.qualityX[0], 552, 4, "SLEEP QUALITY"));
    stream.push(text(layout.qualityX[1], 552, 4, "WAKE QUALITY"));
    stream.push(text(layout.qualityX[2], 552, 4, "DAY FORM"));
    stream.push(text(layout.notesX, 552, 4, "TREATMENT / NOTES"));
  }
  let rowTop = layout.rowTop;
  fragments.forEach((fragment, fragmentIndex) => {
    row(stream, fragment, rowTop, language, sport, fragmentIndex % 2 === 0);
    rowTop -= fragment.height;
  });
  const y = 120;
  stream.push(text(30, y, 9, "OBSERVATIONS"));
  stream.push(text(30, y - 14, 6, summaryLine(snapshot, language)));
  if (language === "fr") {
    stream.push(text(30, 38, 6, "Bleu : séance de sport effectuée (horaires enregistrés ; détails en fin de rapport)."));
    stream.push(
      text(
        30,
        28,
        6,
        "Légende : v Mise au lit ; ^ Lever ; ^ Lever nocturne ; S Somnolence ; M Prise de médicament.",
      ),
    );
    stream.push(
      text(
        30,
        18,
        6,
        "Sommeil : plage de sommeil ; Sieste : plage de sieste ; Long réveil : interruption prolongée ; Demi-sommeil : plage de demi-sommeil.",
      ),
    );
  } else {
    stream.push(text(30, 38, 6, "Blue: completed sport session (recorded times; details at end of report)."));
    stream.push(
      text(
        30,
        28,
        6,
        "Legend: v Bedtime; ^ Final get-up; ^ Night get-up; S Sleepiness; M Medication intake.",
      ),
    );
    stream.push(
      text(
        30,
        18,
        6,
        "Sleep: sleep range; Nap: nap range; Long awakening: prolonged interruption; Half-sleep: half-sleep range.",
      ),
    );
  }
  return stream.join("");
}

function sportPage(
  sessions: readonly SportSession[],
  entries: readonly SleepEntry[],
  index: number,
  count: number,
  language: Language,
  first: string,
  last: string,
  generatedAt: Date,
): string {
  const fr = language === "fr";
  const stream = reportHeader(fr ? "Trainlog — Séances de sport" :
    "Trainlog — Sport sessions", index, count, language, first, last, generatedAt);
  stream.push(text(30, 539, 8, fr ? "SÉANCES EFFECTUÉES" : "COMPLETED SESSIONS", pdfAccent));
  stream.push(text(30, 526, 7, fr
      ? "Horaires enregistrés ; la durée est celle de la séance. Aucun lien médical déduit."
      : "Recorded times and session duration. No medical conclusion is inferred."));
  sessions.forEach((session, rowIndex) => {
    const y = 505 - rowIndex * 16;
    const start = Date.parse(session.started_at);
    const end = session.ended_at === null ? NaN : Date.parse(session.ended_at);
    const validEnd = Number.isFinite(start) && Number.isFinite(end) && end > start;
    const durationText = validEnd ? duration(Math.floor((end - start) / 1000), language)
      : (fr ? "fin non renseignée" : "end not recorded");
    const hasRow = entries.some((entry) =>
      projectSportSegments([session], sleepRowWindow(entry)).length > 0);
    const missing = hasRow ? "" : (fr ? " · sommeil non renseigné" : " · sleep not recorded");
    stream.push(`q 0.10 0.34 0.82 rg 0 G 30 ${y - 2} 9 7 re B Q\n`);
    stream.push(text(45, y, 7,
      `${session.label.slice(0, 48)} (${session.session_type}) · ` +
      `${session.started_at.slice(0, 16)} - ${validEnd ? session.ended_at?.slice(0, 16) : "—"}` +
      ` · ${durationText}${missing}`));
  });
  stream.push(text(30, 25, 6, fr
    ? "Bleu : séance de sport effectuée. Contour noir pour impression monochrome."
    : "Blue: completed sport session. Black outline for monochrome printing."));
  return stream.join("");
}

/** Builds a deterministic local vector document from the Core snapshot, never from screen pixels. */
export function buildSleepDiaryPdf(
  snapshot: SleepSnapshot,
  language: Language,
  sport: readonly SportSession[] = [],
  reportStart?: string,
  reportEnd?: string,
  generatedAt = new Date(),
): Blob {
  // CONTRACT: reverse only day/appendix presentation order. Timed details in
  // each night continue to use the shared chronological projection.
  const orderedEntries = [...snapshot.entries].sort((left, right) =>
    right.night_start_date.localeCompare(left.night_start_date) ||
    left.entry_id.localeCompare(right.entry_id));
  const orderedSport = [...sport].sort((left, right) =>
    right.started_at.slice(0, 10).localeCompare(left.started_at.slice(0, 10)) ||
    Date.parse(left.started_at) - Date.parse(right.started_at) ||
    left.identity.localeCompare(right.identity));
  const first = reportStart ?? orderedEntries[orderedEntries.length - 1]?.night_start_date ?? "—";
  const last = reportEnd ?? orderedEntries[0]?.night_start_date ?? "—";
  const groups = paginate(orderedEntries, language);
  const sportGroups = Array.from({ length: Math.ceil(orderedSport.length / 28) },
    (_, index) => orderedSport.slice(index * 28, (index + 1) * 28));
  const reportEndTime = Date.parse(`${last}T12:00:00Z`);
  const reportEndExclusive = Number.isFinite(reportEndTime)
    ? new Date(reportEndTime + 86400000).toISOString().slice(0, 10)
    : last;
  const contents = [
    ...groups.map((entries, index) => page(snapshot, entries, index,
      groups.length + sportGroups.length, language, orderedSport, first, last, generatedAt)),
    ...sportGroups.map((sessions, index) => sportPage(sessions, orderedEntries,
      groups.length + index, groups.length + sportGroups.length, language,
      first,
      orderedEntries[0]?.night_end_date ?? reportEndExclusive, generatedAt)),
  ];
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    `<< /Type /Pages /Kids [${contents.map((_, index) => `${4 + index * 2} 0 R`).join(" ")}] /Count ${contents.length} >>`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
  ];
  contents.forEach((content, index) => {
    objects.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 842 595] /Resources << /Font << /F1 3 0 R >> >> /Contents ${5 + index * 2} 0 R >>`,
      `<< /Length ${new TextEncoder().encode(content).length} >>\nstream\n${content}endstream`,
    );
  });
  let output = "%PDF-1.4\n%Trainlog\n";
  const offsets: number[] = [];
  objects.forEach((object, index) => {
    offsets.push(new TextEncoder().encode(output).length);
    output += `${index + 1} 0 obj\n${object}\nendobj\n`;
  });
  const xref = new TextEncoder().encode(output).length;
  output += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n${offsets.map((value) => `${String(value).padStart(10, "0")} 00000 n \n`).join("")}trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return new Blob([new TextEncoder().encode(output)], {
    type: "application/pdf",
  });
}

export function presentSleepDiaryPdf(blob: Blob, download: boolean) {
  const url = URL.createObjectURL(blob);
  if (download) {
    const link = document.createElement("a");
    link.href = url;
    link.download = "trainlog-sleep-diary.pdf";
    link.click();
  } else window.open(url, "_blank", "noopener,noreferrer");
  window.setTimeout(() => URL.revokeObjectURL(url), 60_000);
}
