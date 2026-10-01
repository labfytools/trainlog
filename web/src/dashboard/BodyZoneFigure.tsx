import { useState } from 'react'
import type { WorkedZone } from '../api/dashboard'
import { count } from './dashboardFormat'
import { ANDROID_BODY_HEAD, ANDROID_BODY_NECK, ANDROID_BODY_REGIONS } from './androidBodyFigureGeometry'

export const BODY_ZONE_SVG_MAP = {
  chest: ['front-chest-left', 'front-chest-right'],
  back: ['back-trunk'],
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

interface BodyZoneFigureProps { zones: WorkedZone[]; views?: 'front' | 'both'; language?: 'fr' | 'en' }

export function BodyZoneFigure({ zones, views = 'both', language = 'fr' }: BodyZoneFigureProps) {
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
      'aria-label': language === 'fr'
        ? `${zone.label} : ${count(zone.session_count, 'séance')}, ${count(zone.occurrence_count, 'occurrence')}, ${count(zone.set_count, 'série')}.`
        : `${zone.label}: ${zone.session_count} ${zone.session_count === 1 ? 'session' : 'sessions'}, ${zone.occurrence_count} ${zone.occurrence_count === 1 ? 'exposure' : 'exposures'}, ${zone.set_count} ${zone.set_count === 1 ? 'associated set' : 'associated sets'}.`,
      onFocus: () => setFocused(zone), onBlur: () => setFocused(null),
      onMouseEnter: () => setFocused(zone), onMouseLeave: () => setFocused(null),
    }
  }
  return <div className="body-zone-figure">
    <div className="body-views">
      <BodyView title={language === 'fr' ? 'Avant' : 'Front'} side="front" zoneProps={zoneProps} language={language} />
      {views === 'both' && <BodyView title={language === 'fr' ? 'Arrière' : 'Back'} side="back" zoneProps={zoneProps} language={language} />}
    </div>
    <div className="muscle-legend" aria-label={language === 'fr' ? 'Échelle visuelle des séances sur 30 jours' : 'Visual scale of sessions over 30 days'}><span className="intensity-0" />0<span className="intensity-1" />{language === 'fr' ? 'Faible' : 'Low'}<span className="intensity-2" />{language === 'fr' ? 'Intermédiaire' : 'Medium'}<span className="intensity-3" />{language === 'fr' ? 'Maximum observé' : 'Observed maximum'}</div>
    {focused && <div className="body-tooltip" role="status"><strong>{focused.label}</strong><span>{language === 'fr' ? `${count(focused.session_count, 'séance')} · ${count(focused.occurrence_count, 'occurrence')} · ${count(focused.set_count, 'série')}` : `${focused.session_count} sessions · ${focused.occurrence_count} exposures · ${focused.set_count} associated sets`}</span></div>}
  </div>
}

type RegionProps = (zoneId: MappedBodyZoneId) => Record<string, unknown>

function BodyView({ title, side, zoneProps, language }: { title: string; side: 'front' | 'back'; zoneProps: RegionProps; language: 'fr' | 'en' }) {
  return <figure>
    <figcaption>{title}</figcaption>
    <svg
      viewBox="0 0 100 100"
      preserveAspectRatio="none"
      role="group"
      aria-label={language === 'fr' ? `Silhouette, vue ${title.toLowerCase()}` : `Silhouette, ${title.toLowerCase()} view`}
    >
      <ellipse className="body-figure-base" {...ANDROID_BODY_HEAD} />
      <path className="body-figure-base" d={ANDROID_BODY_NECK} />
      {ANDROID_BODY_REGIONS[side].map((region) =>
        <path key={region.id} id={region.id} d={region.d} {...zoneProps(region.zone)} />
      )}
    </svg>
  </figure>
}
