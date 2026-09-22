import { useEffect, useRef } from 'react'
import * as echarts from 'echarts/core'
import { LineChart } from 'echarts/charts'
import { GridComponent, LegendComponent, TooltipComponent } from 'echarts/components'
import { SVGRenderer } from 'echarts/renderers'
import type { MeasurementMetric, MeasurementSeries } from '../api/analysis'
import { trainlogChartTheme } from './trainlogChartTheme'
import { useDatePreferences } from '../presentation/DatePreferences'
import { formatDateTime } from '../dashboard/dashboardFormat'

echarts.use([LineChart, GridComponent, LegendComponent, TooltipComponent, SVGRenderer])

const labels: Record<MeasurementMetric, string> = {
  weight: 'Poids',
  neck: 'Cou',
  shoulders: 'Épaules',
  chest: 'Poitrine',
  waist: 'Taille',
  hips: 'Hanches',
  left_arm: 'Bras G',
  right_arm: 'Bras D',
  left_forearm: 'Avant-bras G',
  right_forearm: 'Avant-bras D',
  left_thigh: 'Cuisse G',
  right_thigh: 'Cuisse D',
  left_calf: 'Mollet G',
  right_calf: 'Mollet D',
}

interface MeasurementsEvolutionChartProps {
  series: MeasurementSeries[]
}

interface EChartsTooltipParam {
  seriesName?: string
  value?: [number, number]
  marker?: string
}

export function MeasurementsEvolutionChart({ series }: MeasurementsEvolutionChartProps) {
  const host = useRef<HTMLDivElement>(null)
  const { dateFormat } = useDatePreferences()

  useEffect(() => {
    if (host.current === null || series.length === 0) return
    const element = host.current
    const theme = trainlogChartTheme(element)
    const chart = echarts.init(element, undefined, { renderer: 'svg' })
    const hasCentimeters = series.some((item) => item.unit === 'cm')
    const hasKilograms = series.some((item) => item.unit === 'kg')
    const colors = [
      theme.lavender,
      theme.mauve,
      theme.blue,
      theme.sapphire,
      theme.green,
      theme.yellow,
      theme.peach,
      theme.red,
      theme.overlay1,
      theme.surface2,
    ]

    const yAxis = [
      ...(hasCentimeters ? [{
        type: 'value' as const,
        name: 'cm',
        scale: true,
        position: 'left' as const,
        nameTextStyle: { color: theme.subtext, fontSize: 11 },
        axisLabel: { color: theme.subtext, fontSize: 11, formatter: '{value} cm' },
        axisLine: { show: false },
        axisTick: { show: false },
        splitLine: { lineStyle: { color: theme.surface0 } },
      }] : []),
      ...(hasKilograms ? [{
        type: 'value' as const,
        name: 'kg',
        scale: true,
        position: hasCentimeters ? 'right' as const : 'left' as const,
        nameTextStyle: { color: theme.subtext, fontSize: 11 },
        axisLabel: { color: theme.subtext, fontSize: 11, formatter: '{value} kg' },
        axisLine: { show: false },
        axisTick: { show: false },
        splitLine: { show: !hasCentimeters, lineStyle: { color: theme.surface0 } },
      }] : []),
    ]

    chart.setOption({
      animation: !matchMedia('(prefers-reduced-motion: reduce)').matches,
      color: colors,
      textStyle: { color: theme.text, fontFamily: theme.fontSans, fontSize: theme.fontSize },
      grid: {
        left: hasCentimeters ? 56 : 58,
        right: hasCentimeters && hasKilograms ? 58 : 16,
        top: 14,
        bottom: series.length > 4 ? 66 : 48,
      },
      legend: {
        type: 'scroll',
        bottom: 0,
        left: 0,
        right: 0,
        textStyle: { color: theme.subtext, fontFamily: theme.fontSans, fontSize: 11 },
        pageTextStyle: { color: theme.subtext },
        pageIconColor: theme.lavender,
        pageIconInactiveColor: theme.surface2,
      },
      tooltip: {
        trigger: 'axis',
        renderMode: 'richText',
        confine: true,
        backgroundColor: theme.mantle,
        borderColor: theme.surface2,
        textStyle: { color: theme.text, fontFamily: theme.fontSans, fontSize: 12, lineHeight: 18 },
        formatter: (raw: unknown) => {
          const params = (Array.isArray(raw) ? raw : [raw]) as EChartsTooltipParam[]
          const firstValue = params.find((item) => Array.isArray(item.value))?.value
          const timestamp = firstValue?.[0]
          const heading = typeof timestamp === 'number'
            ? formatDateTime(new Date(timestamp).toISOString(), dateFormat)
            : ''
          const rows = params.flatMap((item) => {
            const value = item.value
            if (!Array.isArray(value) || typeof value[1] !== 'number') return []
            const source = series.find((candidate) => labels[candidate.metric] === item.seriesName)
            const unit = source?.unit ?? ''
            return [`${item.marker ?? ''}${item.seriesName ?? ''} : ${value[1].toLocaleString('fr-FR', { maximumFractionDigits: 2 })} ${unit}`]
          })
          return [heading, ...rows].filter(Boolean).join('\n')
        },
      },
      xAxis: {
        type: 'time',
        boundaryGap: false,
        axisLine: { lineStyle: { color: theme.surface2 } },
        axisTick: { show: false },
        axisLabel: { color: theme.subtext, fontSize: 11, hideOverlap: true },
        splitLine: { show: false },
      },
      yAxis,
      series: series.map((item, index) => ({
        name: labels[item.metric],
        type: 'line',
        yAxisIndex: item.unit === 'kg' && hasCentimeters ? 1 : 0,
        data: item.points.map((point) => [new Date(point.timestamp).valueOf(), point.value]),
        smooth: false,
        showSymbol: true,
        symbolSize: 6,
        connectNulls: false,
        lineStyle: { width: 2 },
        itemStyle: { color: colors[index % colors.length] },
        emphasis: { focus: 'series', scale: 1.15 },
      })),
    })

    const observer = new ResizeObserver(() => chart.resize())
    observer.observe(element)
    return () => {
      observer.disconnect()
      chart.dispose()
    }
  }, [dateFormat, series])

  const pointCount = series.reduce((total, item) => total + item.points.length, 0)
  return (
    <div
      className="measurements-evolution-chart"
      ref={host}
      data-testid="measurements-evolution-chart"
      role="img"
      aria-label={`Évolution de ${series.length} mensuration${series.length > 1 ? 's' : ''} disposant d’au moins deux relevés, ${pointCount} mesures réelles au total.`}
    />
  )
}
