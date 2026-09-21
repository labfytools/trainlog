import type { SleepEntry, SleepEventType, SleepSnapshot } from '../api/sleepDiary'

const ascii = (value: string) => value.normalize('NFD').replace(/[\u0300-\u036f]/g, '').replace(/[^\x20-\x7e]/g, '?')
const text = (x: number, y: number, size: number, value: string) => `0 g BT /F1 ${size} Tf ${x.toFixed(1)} ${y.toFixed(1)} Td (${ascii(value).replace(/([\\()])/g, '\\$1')}) Tj ET\n`
const minute = (iso: string, night: string) => Math.max(0, Math.min(1440,
  (new Date(iso).getTime() - new Date(`${night}T18:00:00`).getTime()) / 60000))
const labels: Record<SleepEventType, string> = { bed_time: 'v', final_get_up: '^', night_get_up: '^',
  sleep: 'SLEEP', nap: 'NAP', long_awake: 'AWAKE', half_sleep: 'HALF', daytime_sleepiness: 'S' }

function row(stream: string[], entry: SleepEntry, y: number) {
  const timelineX = 112; const width = 500
  stream.push(`0.7 G 30 ${y - 40} 782 40 re S\n`)
  stream.push(text(33, y - 16, 5, `${entry.night_start_date} > ${entry.night_end_date}`))
  for (let hour = 0; hour <= 24; hour++) { const x = timelineX + hour / 24 * width; stream.push(`0.9 G ${x} ${y - 40} m ${x} ${y} l S\n`) }
  entry.events.forEach((event) => {
    const x = timelineX + minute(event.start_at, entry.night_start_date) / 1440 * width
    if (event.end_at) {
      const eventWidth = Math.max(2, timelineX + minute(event.end_at, entry.night_start_date) / 1440 * width - x)
      const shade = event.type === 'sleep' ? 0.75 : event.type === 'half_sleep' ? 0.87 : 0.93
      stream.push(`${shade} g ${x} ${y - 29} ${eventWidth} 18 re f 0 G ${x} ${y - 29} ${eventWidth} 18 re S\n`)
      if (eventWidth > 24) stream.push(text(x + 2, y - 23, 5, labels[event.type]))
    } else stream.push(text(x, y - 25, 8, labels[event.type]))
  })
  entry.intakes.forEach((intake) => {
    const x = timelineX + minute(intake.taken_at, entry.night_start_date) / 1440 * width
    stream.push(text(x, y - 36, 6, 'M'))
  })
  stream.push(text(620, y - 16, 7, entry.sleep_quality ?? '-')); stream.push(text(658, y - 16, 7, entry.wake_quality ?? '-'))
  const medicationText = entry.intakes.map((intake) => `${intake.taken_at.slice(11, 16)} ${intake.medication_name}${intake.dose_value === null ? '' : ` ${intake.dose_value} ${intake.dose_unit}`}`).join('; ')
  const notes = [medicationText, entry.treatment_and_notes].filter(Boolean).join(' | ')
  stream.push(text(696, y - 16, 7, entry.day_form ?? '-')); stream.push(text(730, y - 13, 4, notes.slice(0, 32)))
  stream.push(text(730, y - 22, 4, notes.slice(32, 64)))
}

function page(snapshot: SleepSnapshot, entries: SleepEntry[], index: number, count: number, language: 'fr' | 'en') {
  const stream = ['0 G 0 g\n', text(30, 565, 14, language === 'fr' ? 'AGENDA TRAINLOG DE VIGILANCE ET DE SOMMEIL' : 'TRAINLOG SLEEP AND ALERTNESS DIARY'), text(740, 565, 7, `${index + 1}/${count}`), text(30, 548, 7, 'DATE')]
  for (let hour = 0; hour <= 24; hour++) stream.push(text(112 + hour / 24 * 500, 548, 4, String((18 + hour) % 24)))
  stream.push(text(618, 548, 4, 'SLEEP'), text(656, 548, 4, 'WAKE'), text(694, 548, 4, 'DAY'), text(730, 548, 4, 'TREATMENT / NOTES'))
  entries.forEach((entry, rowIndex) => row(stream, entry, 538 - rowIndex * 40))
  const y = 520 - entries.length * 40
  stream.push(text(30, y, 9, 'OBSERVATIONS'), text(30, y - 14, 6, `${snapshot.summary.nights} nights; sleep ${Math.round(snapshot.summary.sleep_duration_seconds / 60)} min; long awake ${snapshot.summary.long_awake_count}; naps ${snapshot.summary.nap_count}; sleepiness ${snapshot.summary.sleepiness_count}; intakes ${snapshot.summary.intake_count}`))
  entries.filter((entry) => entry.treatment_and_notes).slice(0, 3).forEach((entry, noteIndex) => stream.push(text(30, y - 27 - noteIndex * 10, 6, `${entry.night_start_date}: ${entry.treatment_and_notes.slice(0, 110)}`)))
  stream.push(text(30, 18, 6, 'Legend: v bedtime; ^ get-up; S sleepiness; M medication intake; SLEEP sleep; NAP nap; AWAKE long awakening; HALF half-sleep.'))
  return stream.join('')
}

/** Builds a deterministic local vector document from the Core snapshot, never from screen pixels. */
export function buildSleepDiaryPdf(snapshot: SleepSnapshot, language: 'fr' | 'en'): Blob {
  const groups = Array.from({ length: Math.max(1, Math.ceil(snapshot.entries.length / 9)) }, (_, index) => snapshot.entries.slice(index * 9, (index + 1) * 9))
  const objects = ['<< /Type /Catalog /Pages 2 0 R >>', `<< /Type /Pages /Kids [${groups.map((_, index) => `${4 + index * 2} 0 R`).join(' ')}] /Count ${groups.length} >>`, '<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
  groups.forEach((entries, index) => { const content = page(snapshot, entries, index, groups.length, language); objects.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 842 595] /Resources << /Font << /F1 3 0 R >> >> /Contents ${5 + index * 2} 0 R >>`, `<< /Length ${new TextEncoder().encode(content).length} >>\nstream\n${content}endstream`) })
  let output = '%PDF-1.4\n%Trainlog\n'; const offsets: number[] = []
  objects.forEach((object, index) => { offsets.push(new TextEncoder().encode(output).length); output += `${index + 1} 0 obj\n${object}\nendobj\n` })
  const xref = new TextEncoder().encode(output).length
  output += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n${offsets.map((value) => `${String(value).padStart(10, '0')} 00000 n \n`).join('')}trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`
  return new Blob([new TextEncoder().encode(output)], { type: 'application/pdf' })
}

export function presentSleepDiaryPdf(blob: Blob, download: boolean) {
  const url = URL.createObjectURL(blob)
  if (download) { const link = document.createElement('a'); link.href = url; link.download = 'trainlog-sleep-diary.pdf'; link.click() }
  else window.open(url, '_blank', 'noopener,noreferrer')
  window.setTimeout(() => URL.revokeObjectURL(url), 60_000)
}
