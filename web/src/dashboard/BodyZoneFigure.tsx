import { useState } from 'react'
import type { WorkedZone } from '../api/dashboard'
import { count } from './dashboardFormat'

export const BODY_ZONE_SVG_MAP = {
  chest: ['front-chest-left', 'front-chest-right'],
  back: ['back-upper', 'back-lat-left', 'back-lat-right'],
  shoulders: ['front-shoulder-left', 'front-shoulder-right', 'back-shoulder-left', 'back-shoulder-right'],
  arms: ['front-arm-left', 'front-arm-right', 'back-arm-left', 'back-arm-right'],
  core: ['front-core'],
  glutes: ['back-glute-left', 'back-glute-right'],
  thighs: ['front-thigh-left', 'front-thigh-right', 'back-thigh-left', 'back-thigh-right'],
  calves: ['front-calf-left', 'front-calf-right', 'back-calf-left', 'back-calf-right'],
} as const

export type MappedBodyZoneId = keyof typeof BODY_ZONE_SVG_MAP
export const UNMAPPED_BODY_ZONE_IDS = ['full_body', 'upper_body', 'lower_body'] as const

export function muscleIntensityLevel(sessionCount: number, maximum: number): 0 | 1 | 2 | 3 {
  if (sessionCount <= 0 || maximum <= 0) return 0
  const ratio = sessionCount / maximum
  if (ratio <= 1 / 3) return 1
  if (ratio <= 2 / 3) return 2
  return 3
}

interface BodyZoneFigureProps { zones: WorkedZone[]; views?: 'front' | 'both' }

export function BodyZoneFigure({ zones, views = 'both' }: BodyZoneFigureProps) {
  const [focused, setFocused] = useState<WorkedZone | null>(null)
  const byId = new Map(zones.map((zone) => [zone.zone_id, zone]))
  const maximum = Math.max(1, ...zones.map((zone) => zone.session_count))
  const zoneProps = (zoneId: MappedBodyZoneId) => {
    const zone = byId.get(zoneId)
    if (zone === undefined) return { className: 'body-region intensity-0', 'aria-hidden': true as const }
    return {
      className: `body-region intensity-${muscleIntensityLevel(zone.session_count, maximum)}`,
      tabIndex: 0,
      role: 'button',
      'aria-label': `${zone.label} : ${count(zone.session_count, 'séance')}, ${count(zone.occurrence_count, 'occurrence')}, ${count(zone.set_count, 'série')}.`,
      onFocus: () => setFocused(zone), onBlur: () => setFocused(null),
      onMouseEnter: () => setFocused(zone), onMouseLeave: () => setFocused(null),
    }
  }
  return <div className="body-zone-figure">
    <div className="body-views">
      <BodyView title="Avant" side="front" zoneProps={zoneProps} />
      {views === 'both' && <BodyView title="Arrière" side="back" zoneProps={zoneProps} />}
    </div>
    <div className="muscle-legend" aria-label="Échelle visuelle des séances sur 30 jours"><span className="intensity-0" />0<span className="intensity-1" />Faible<span className="intensity-2" />Intermédiaire<span className="intensity-3" />Maximum observé</div>
    {focused && <div className="body-tooltip" role="status"><strong>{focused.label}</strong><span>{count(focused.session_count, 'séance')} · {count(focused.occurrence_count, 'occurrence')} · {count(focused.set_count, 'série')}</span></div>}
  </div>
}

type RegionProps = (zoneId: MappedBodyZoneId) => Record<string, unknown>

function BodyView({ title, side, zoneProps }: { title: string; side: 'front' | 'back'; zoneProps: RegionProps }) {
  return <figure><figcaption>{title}</figcaption><svg viewBox="0 0 120 260" role="group" aria-label={`Silhouette, vue ${title.toLowerCase()}`}>
    <circle className="body-outline" cx="60" cy="23" r="16" />
    <path className="body-outline" d="M42 43 Q60 36 78 43 L88 116 75 150 70 244H52L45 150 32 116Z" />
    {side === 'front' ? <>
      <ellipse id="front-shoulder-left" {...zoneProps('shoulders')} cx="37" cy="57" rx="10" ry="12" /><ellipse id="front-shoulder-right" {...zoneProps('shoulders')} cx="83" cy="57" rx="10" ry="12" />
      <path id="front-chest-left" {...zoneProps('chest')} d="M43 58 Q52 53 58 59 L57 85 Q47 85 42 77Z" /><path id="front-chest-right" {...zoneProps('chest')} d="M77 58 Q68 53 62 59 L63 85 Q73 85 78 77Z" />
      <path id="front-arm-left" {...zoneProps('arms')} d="M28 61 Q35 58 41 64 L34 130 23 128Z" /><path id="front-arm-right" {...zoneProps('arms')} d="M92 61 Q85 58 79 64 L86 130 97 128Z" />
      <path id="front-core" {...zoneProps('core')} d="M46 88 Q60 94 74 88 L76 135 Q60 145 44 135Z" />
      <path id="front-thigh-left" {...zoneProps('thighs')} d="M45 139 Q54 143 59 140 L56 198 43 198Z" /><path id="front-thigh-right" {...zoneProps('thighs')} d="M75 139 Q66 143 61 140 L64 198 77 198Z" />
      <path id="front-calf-left" {...zoneProps('calves')} d="M43 201H56L54 247H46Z" /><path id="front-calf-right" {...zoneProps('calves')} d="M64 201H77L74 247H66Z" />
    </> : <>
      <ellipse id="back-shoulder-left" {...zoneProps('shoulders')} cx="37" cy="57" rx="10" ry="12" /><ellipse id="back-shoulder-right" {...zoneProps('shoulders')} cx="83" cy="57" rx="10" ry="12" />
      <path id="back-upper" {...zoneProps('back')} d="M43 57 Q60 48 77 57 L73 83 Q60 91 47 83Z" /><path id="back-lat-left" {...zoneProps('back')} d="M43 82L58 89 55 127 43 135Z" /><path id="back-lat-right" {...zoneProps('back')} d="M77 82L62 89 65 127 77 135Z" />
      <path id="back-arm-left" {...zoneProps('arms')} d="M28 61 Q35 58 41 64 L34 130 23 128Z" /><path id="back-arm-right" {...zoneProps('arms')} d="M92 61 Q85 58 79 64 L86 130 97 128Z" />
      <path id="back-glute-left" {...zoneProps('glutes')} d="M44 132 Q53 127 59 136 L57 157 Q47 160 42 151Z" /><path id="back-glute-right" {...zoneProps('glutes')} d="M76 132 Q67 127 61 136 L63 157 Q73 160 78 151Z" />
      <path id="back-thigh-left" {...zoneProps('thighs')} d="M43 159Q52 162 58 158L56 201H43Z" /><path id="back-thigh-right" {...zoneProps('thighs')} d="M77 159Q68 162 62 158L64 201H77Z" />
      <path id="back-calf-left" {...zoneProps('calves')} d="M43 203H56L54 247H46Z" /><path id="back-calf-right" {...zoneProps('calves')} d="M64 203H77L74 247H66Z" />
    </>}
  </svg></figure>
}
