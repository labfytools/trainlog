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

const text = (x: number, y: number, size: number, value: string) =>
  `0 g BT /F1 ${size} Tf ${x.toFixed(1)} ${y.toFixed(1)} Td (${pdfString(value)}) Tj ET\n`;

const compactDate = (isoDate: string) => {
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
  entry: SleepEntry,
  y: number,
  language: Language,
  sport: readonly SportSession[],
) {
  const projection = projectSleepTimeline(entry);
  const window = sleepRowWindow(entry);
  const timelineX = 112;
  const width = 500;
  stream.push(`0.7 G 30 ${y - 40} 782 40 re S\n`);
  stream.push(text(33, y - 13, 5, compactDate(entry.night_start_date)));
  stream.push(
    text(
      33,
      y - 23,
      5,
      `${language === "fr" ? "au" : "to"} ${compactDate(entry.night_end_date)}`,
    ),
  );
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
  stream.push(text(620, y - 16, 7, entry.sleep_quality ?? "-"));
  stream.push(text(658, y - 16, 7, entry.wake_quality ?? "-"));
  const medicationText = entry.intakes
    .map(
      (intake) =>
        `${intake.taken_at.slice(11, 16)} — ${intake.medication_name}${
          intake.dose_value === null
            ? ""
            : ` — ${formatDose(intake.dose_value)} ${intake.dose_unit ?? ""}${intake.quantity > 1 ? ` ×${intake.quantity}` : ""}`
        }`,
    )
    .join(" ; ");
  const notes = [medicationText, entry.treatment_and_notes]
    .filter(Boolean)
    .join(" | ");
  stream.push(text(696, y - 16, 7, entry.day_form ?? "-"));
  stream.push(text(730, y - 7, 4, notes.slice(0, 40)));
  stream.push(text(730, y - 16, 4, notes.slice(40, 80)));
  stream.push(text(730, y - 25, 4, notes.slice(80, 120)));
  stream.push(text(730, y - 34, 4, notes.slice(120, 160)));
  projectSportSegments(sport, window).forEach((segment) => {
    const x = timelineX + segment.left * width;
    const segmentWidth = segment.end === null ? 2 : Math.max(2, segment.width * width);
    // PDF graphics state contains the blue fill and black outline. It cannot
    // tint later sleep marks, grid lines, medication labels, or body text.
    stream.push(`q 0.10 0.34 0.82 rg 0 G ${x.toFixed(2)} ${(y - 9).toFixed(2)} ` +
      `${segmentWidth.toFixed(2)} 5 re B Q\n`);
  });
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
  entries: SleepEntry[],
  index: number,
  count: number,
  language: Language,
  sport: readonly SportSession[],
) {
  const stream = [
    "0 G 0 g\n",
    text(
      30,
      565,
      14,
      language === "fr"
        ? "AGENDA TRAINLOG DE VIGILANCE ET DE SOMMEIL"
        : "TRAINLOG SLEEP AND ALERTNESS DIARY",
    ),
    text(740, 565, 7, `${index + 1}/${count}`),
    text(30, 548, 7, "DATE"),
  ];
  for (let hour = 0; hour <= 24; hour++)
    stream.push(
      text(112 + (hour / 24) * 500, 548, 4, String((18 + hour) % 24)),
    );
  if (language === "fr") {
    stream.push(text(616, 552, 4, "QUALITÉ DU"));
    stream.push(text(616, 545, 4, "SOMMEIL"));
    stream.push(text(654, 552, 4, "QUALITÉ DU"));
    stream.push(text(654, 545, 4, "RÉVEIL"));
    stream.push(text(692, 552, 4, "FORME DE LA"));
    stream.push(text(692, 545, 4, "JOURNÉE"));
    stream.push(text(728, 552, 4, "TRAITEMENT ET"));
    stream.push(text(728, 545, 4, "REMARQUES"));
  } else {
    stream.push(text(616, 552, 4, "SLEEP QUALITY"));
    stream.push(text(654, 552, 4, "WAKE QUALITY"));
    stream.push(text(692, 552, 4, "DAY FORM"));
    stream.push(text(728, 552, 4, "TREATMENT / NOTES"));
  }
  entries.forEach((entry, rowIndex) =>
    row(stream, entry, 538 - rowIndex * 40, language, sport),
  );
  const y = 520 - entries.length * 40;
  stream.push(text(30, y, 9, "OBSERVATIONS"));
  stream.push(text(30, y - 14, 6, summaryLine(snapshot, language)));
  const unvalidated = snapshot.entries.filter(
    (entry) =>
      entry.publication_status === "draft" ||
      entry.publication_status === "modified",
  ).length;
  if (unvalidated > 0)
    stream.push(
      text(
        30,
        y - 24,
        6,
        language === "fr"
          ? `AVERTISSEMENT : ${plural(unvalidated, "jour non validé", "jours non validés")} dans cet export.`
          : `WARNING: ${plural(unvalidated, "unvalidated day", "unvalidated days")} in this export.`,
      ),
    );
  entries
    .filter((entry) => entry.treatment_and_notes)
    .slice(0, 3)
    .forEach((entry, noteIndex) =>
      stream.push(
        text(
          30,
          y - 36 - noteIndex * 10,
          6,
          `${compactDate(entry.night_start_date)} : ${entry.treatment_and_notes.slice(0, 110)}`,
        ),
      ),
    );
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
): string {
  const fr = language === "fr";
  const stream = [
    "0 G 0 g\n",
    text(30, 565, 14, fr ? "SÉANCES DE SPORT EFFECTUÉES" : "COMPLETED SPORT SESSIONS"),
    text(740, 565, 7, `${index + 1}/${count}`),
    text(30, 547, 7, `${fr ? "Période des lignes" : "Row period"} : ${first} 18:00 - ${last} 18:00`),
    text(30, 532, 7, fr
      ? "Horaires enregistrés ; la durée est celle de la séance. Aucun lien médical déduit."
      : "Recorded times and session duration. No medical conclusion is inferred."),
  ];
  sessions.forEach((session, rowIndex) => {
    const y = 510 - rowIndex * 16;
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
  reportStart = snapshot.entries[0]?.night_start_date ?? "—",
  reportEnd = snapshot.entries[snapshot.entries.length - 1]?.night_start_date ?? "—",
): Blob {
  const groups = Array.from(
    { length: Math.max(1, Math.ceil(snapshot.entries.length / 9)) },
    (_, index) => snapshot.entries.slice(index * 9, (index + 1) * 9),
  );
  const sportGroups = Array.from({ length: Math.ceil(sport.length / 28) },
    (_, index) => sport.slice(index * 28, (index + 1) * 28));
  const reportEndTime = Date.parse(`${reportEnd}T12:00:00Z`);
  const reportEndExclusive = Number.isFinite(reportEndTime)
    ? new Date(reportEndTime + 86400000).toISOString().slice(0, 10)
    : reportEnd;
  const contents = [
    ...groups.map((entries, index) => page(snapshot, entries, index,
      groups.length + sportGroups.length, language, sport)),
    ...sportGroups.map((sessions, index) => sportPage(sessions, snapshot.entries,
      groups.length + index, groups.length + sportGroups.length, language,
      reportStart,
      snapshot.entries[snapshot.entries.length - 1]?.night_end_date ?? reportEndExclusive)),
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
