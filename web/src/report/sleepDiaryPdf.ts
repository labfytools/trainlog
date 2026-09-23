import type {
  SleepEntry,
  SleepEventType,
  SleepSnapshot,
} from "../api/sleepDiary";
import { formatDose, formatDuration } from "../dashboard/dashboardFormat";

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

const minute = (iso: string, night: string) =>
  Math.max(
    0,
    Math.min(
      1440,
      (new Date(iso).getTime() - new Date(`${night}T18:00:00`).getTime()) /
        60000,
    ),
  );

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
) {
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
  entry.events.forEach((event) => {
    const x =
      timelineX +
      (minute(event.start_at, entry.night_start_date) / 1440) * width;
    if (event.end_at) {
      // INVARIANT: every interval keeps its canonical absolute start/end on
      // the shared 18:00-to-18:00 axis; long awakenings therefore interrupt
      // rather than replace or merge the adjacent sleep ranges.
      const eventWidth = Math.max(
        2,
        timelineX +
          (minute(event.end_at, entry.night_start_date) / 1440) * width -
          x,
      );
      const shade =
        event.type === "sleep"
          ? 0.75
          : event.type === "long_awake"
            ? 0.5
            : event.type === "half_sleep"
              ? 0.87
              : 0.93;
      stream.push(
        `${shade} g ${x.toFixed(2)} ${(y - 29).toFixed(2)} ${eventWidth.toFixed(2)} 18.00 re f 0 G ${x.toFixed(2)} ${(y - 29).toFixed(2)} ${eventWidth.toFixed(2)} 18.00 re S\n`,
      );
      if (eventWidth > 24)
        stream.push(text(x + 2, y - 23, 5, eventLabels[language][event.type]));
    } else {
      stream.push(text(x, y - 25, 8, pointMarker(event.type)));
    }
  });
  entry.intakes.forEach((intake) => {
    const x =
      timelineX +
      (minute(intake.taken_at, entry.night_start_date) / 1440) * width;
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
    row(stream, entry, 538 - rowIndex * 40, language),
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

/** Builds a deterministic local vector document from the Core snapshot, never from screen pixels. */
export function buildSleepDiaryPdf(
  snapshot: SleepSnapshot,
  language: Language,
): Blob {
  const groups = Array.from(
    { length: Math.max(1, Math.ceil(snapshot.entries.length / 9)) },
    (_, index) => snapshot.entries.slice(index * 9, (index + 1) * 9),
  );
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    `<< /Type /Pages /Kids [${groups.map((_, index) => `${4 + index * 2} 0 R`).join(" ")}] /Count ${groups.length} >>`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
  ];
  groups.forEach((entries, index) => {
    const content = page(snapshot, entries, index, groups.length, language);
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
