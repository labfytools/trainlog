import { useEffect, useRef } from 'react'
import * as echarts from 'echarts/core'
import { LineChart } from 'echarts/charts'
import { GridComponent, TooltipComponent } from 'echarts/components'
import { SVGRenderer } from 'echarts/renderers'
import type { ProgressionIdentity, ProgressionPoint } from '../api/dashboard'
import { formatDateTime, formatDose, formatWeight } from '../dashboard/dashboardFormat'
import { trainlogChartTheme } from './trainlogChartTheme'

echarts.use([LineChart, GridComponent, TooltipComponent, SVGRenderer])

interface ProgressionChartProps { identity: ProgressionIdentity; points: ProgressionPoint[] }

interface ChartDatum {
  value: [number, number]
  point: ProgressionPoint
  itemStyle: { color: string; borderColor: string; borderWidth: number }
  symbolSize: number
  label?: { show: boolean; formatter: string; position: 'top'; color: string; fontSize: number }
}

export interface ProgressionYAxisBounds { min: number; max: number }

export function progressionYAxisBounds(points: ProgressionPoint[]): ProgressionYAxisBounds {
  if (points.length === 0) return { min: 0, max: 1 }
  const weights = points.map((point) => point.weight_kg)
  const observedMin = Math.min(...weights)
  const observedMax = Math.max(...weights)
  const span = observedMax - observedMin
  // CONTRACT: bounds add visual breathing room only; every persisted value,
  // including a legacy zero, remains unchanged and inside the visible domain.
  const rawMargin = span === 0
    ? Math.max(1, Math.abs(observedMax) * 0.1)
    : Math.max(0.5, span * 0.1)
  const margin = Math.ceil(rawMargin * 2) / 2
  const min = Math.max(0, observedMin - margin)
  const max = observedMax + margin
  return { min, max: max > min ? max : min + 1 }
}

export function progressionCollisionMetadata(points: ProgressionPoint[]): Array<{ count: number; index: number }> {
  const keys = points.map((point) => `${point.timestamp}\u0000${point.weight_kg}`)
  const totals = new Map<string, number>()
  keys.forEach((key) => totals.set(key, (totals.get(key) ?? 0) + 1))
  const seen = new Map<string, number>()
  return keys.map((key) => {
    const index = seen.get(key) ?? 0
    seen.set(key, index + 1)
    return { count: totals.get(key) ?? 1, index }
  })
}

export function progressionTooltip(identity: ProgressionIdentity, point: ProgressionPoint): string {
  return [
    formatDateTime(point.timestamp),
    formatWeight(point.weight_kg),
    identity.exercise_name,
    identity.equipment_label,
    `Dose : ${formatDose(identity.dose)}`,
    ...(point.improved ? ['Amélioration'] : []),
  ].filter(Boolean).join('\n')
}

export function ProgressionChart({ identity, points }: ProgressionChartProps) {
  const host = useRef<HTMLDivElement>(null)

  useEffect(() => {
    if (host.current === null) return
    const element = host.current
    const theme = trainlogChartTheme(element)
    const chart = echarts.init(element, undefined, { renderer: 'svg' })
    const yBounds = progressionYAxisBounds(points)
    const collisions = progressionCollisionMetadata(points)
    const data: ChartDatum[] = points.map((point, index) => {
      const collision = collisions[index]
      const baseSize = point.improved ? 10 : 7
      return {
        value: [new Date(point.timestamp).valueOf(), point.weight_kg], point,
        itemStyle: { color: point.improved ? theme.green : theme.lavender, borderColor: theme.base, borderWidth: 2 },
        symbolSize: baseSize + (collision.count - collision.index - 1) * 4,
        ...(collision.count > 1 && collision.index === collision.count - 1 ? { label: { show: true, formatter: `×${collision.count}`, position: 'top' as const, color: theme.subtext, fontSize: 12 } } : {}),
      }
    })
    chart.setOption({
      animation: !matchMedia('(prefers-reduced-motion: reduce)').matches,
      textStyle: { color: theme.text, fontFamily: theme.fontSans, fontSize: theme.fontSize },
      grid: { left: 8, right: 10, top: 12, bottom: 4, outerBoundsMode: 'same' },
      tooltip: {
        trigger: 'item', renderMode: 'richText', confine: true,
        backgroundColor: theme.mantle, borderColor: theme.surface2, textStyle: { color: theme.text, fontFamily: theme.fontSans, fontSize: 12, lineHeight: 18 },
        formatter: (params: unknown) => {
          const candidate = Array.isArray(params) ? params[0] : params
          const datum = (candidate as { data?: ChartDatum } | undefined)?.data
          return datum === undefined ? '' : progressionTooltip(identity, datum.point)
        },
      },
      xAxis: {
        type: 'time', boundaryGap: false,
        axisLine: { lineStyle: { color: theme.surface2 } }, axisTick: { show: false },
        axisLabel: { color: theme.subtext, fontSize: 12, hideOverlap: true }, splitLine: { show: false },
      },
      yAxis: {
        type: 'value', name: 'kg', min: yBounds.min, max: yBounds.max, scale: true, nameTextStyle: { color: theme.subtext, fontSize: 12 },
        axisLabel: { color: theme.subtext, fontSize: 12, formatter: '{value} kg' },
        axisLine: { show: false }, axisTick: { show: false }, splitLine: { lineStyle: { color: theme.surface0 } },
      },
      series: [{
        type: 'line', data, smooth: false, showSymbol: true, connectNulls: false,
        lineStyle: { color: theme.lavender, width: 2 }, emphasis: { focus: 'series', scale: 1.15 },
      }],
    })
    const observer = new ResizeObserver(() => chart.resize())
    observer.observe(element)
    return () => { observer.disconnect(); chart.dispose() }
  }, [identity, points])

  return <div className="progression-chart" ref={host} data-testid="progression-chart" role="img" aria-label={`Courbe de ${identity.exercise_name} : ${points.length} ${points.length === 1 ? 'mesure réelle' : 'mesures réelles'} en kilogrammes. La ligne relie les observations sans représenter de mesures intermédiaires.`} />
}
