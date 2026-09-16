export interface TrainlogChartTheme {
  base: string
  mantle: string
  surface0: string
  surface1: string
  surface2: string
  overlay1: string
  text: string
  subtext: string
  lavender: string
  mauve: string
  blue: string
  sapphire: string
  green: string
  yellow: string
  peach: string
  red: string
  fontSans: string
  fontSize: number
}

const fallback: TrainlogChartTheme = {
  base: '#1e1e2e', mantle: '#181825', surface0: '#313244', surface1: '#45475a',
  surface2: '#585b70', overlay1: '#7f849c', text: '#cdd6f4', subtext: '#a6adc8',
  lavender: '#b4befe', mauve: '#cba6f7', blue: '#89b4fa', sapphire: '#74c7ec',
  green: '#a6e3a1', yellow: '#f9e2af', peach: '#fab387', red: '#f38ba8',
  fontSans: 'Inter, ui-sans-serif, system-ui, sans-serif', fontSize: 12,
}

/** CONTRACT: charts consume the Web design system at runtime, so a theme
 * change cannot silently leave ECharts with its default palette. */
export function trainlogChartTheme(element: Element): TrainlogChartTheme {
  const style = getComputedStyle(element)
  const value = (token: string, defaultValue: string) => style.getPropertyValue(token).trim() || defaultValue
  return {
    base: value('--base', fallback.base), mantle: value('--mantle', fallback.mantle),
    surface0: value('--surface-0', fallback.surface0), surface1: value('--surface-1', fallback.surface1),
    surface2: value('--surface-2', fallback.surface2), overlay1: value('--overlay-1', fallback.overlay1),
    text: value('--text', fallback.text), subtext: value('--subtext-0', fallback.subtext),
    lavender: value('--lavender', fallback.lavender), mauve: value('--mauve', fallback.mauve),
    blue: value('--blue', fallback.blue), sapphire: value('--sapphire', fallback.sapphire),
    green: value('--green', fallback.green), yellow: value('--yellow', fallback.yellow),
    peach: value('--peach', fallback.peach), red: value('--red', fallback.red),
    fontSans: value('--font-sans', fallback.fontSans), fontSize: 12,
  }
}
