export type AnalysisLanguage = 'fr' | 'en'

export const analysisMessages = {
  fr: {
    eyebrow: 'DONNÉES FACTUELLES', title: 'Analyse', loading: 'Chargement de l’analyse…', failed: 'Analyse momentanément indisponible.',
    overview: 'Vue d’ensemble', exercises: 'Exercices', distribution: 'Répartition', measurements: 'Mensurations',
    period: 'Période', periods: { '7d': '7 jours', '30d': '30 jours', '90d': '90 jours', all: 'Tout' },
    sessions: 'Séances', sets: 'Séries', duration: 'Durée', unavailable: 'Indisponible', noData: 'Aucune donnée sur cette période.',
    activeProgram: 'Programme actif', noProgram: 'Aucun programme actif', nextSession: 'Prochaine séance',
    exercise: 'Exercice', noExercise: 'Aucun exercice disponible', noExerciseData: 'Aucune donnée compatible pour cet exercice.',
    reps: 'Répétitions', volume: 'Volume externe', max: 'MAX explicite', frequency: 'Fréquence', distance: 'Distance', speed: 'Vitesse',
    exposures: 'Expositions', zonesHelp: 'Occurrences enregistrées associées à une zone primaire. Ce ne sont pas des pourcentages physiologiques.',
    metric: 'Mesure', first: 'Première', last: 'Dernière', delta: 'Évolution', onePoint: 'Une seule mesure : aucune évolution calculée.',
    language: 'Langue', partial: 'Affichage borné aux observations les plus récentes.', programProgress: 'Progression du programme',
  },
  en: {
    eyebrow: 'FACTUAL DATA', title: 'Analysis', loading: 'Loading analysis…', failed: 'Analysis is temporarily unavailable.',
    overview: 'Overview', exercises: 'Exercises', distribution: 'Distribution', measurements: 'Measurements',
    period: 'Period', periods: { '7d': '7 days', '30d': '30 days', '90d': '90 days', all: 'All' },
    sessions: 'Sessions', sets: 'Sets', duration: 'Duration', unavailable: 'Unavailable', noData: 'No data in this period.',
    activeProgram: 'Active program', noProgram: 'No active program', nextSession: 'Next session',
    exercise: 'Exercise', noExercise: 'No exercise available', noExerciseData: 'No compatible data for this exercise.',
    reps: 'Repetitions', volume: 'External volume', max: 'Explicit MAX', frequency: 'Frequency', distance: 'Distance', speed: 'Speed',
    exposures: 'Exposures', zonesHelp: 'Recorded occurrences associated with a primary zone. These are not physiological percentages.',
    metric: 'Measurement', first: 'First', last: 'Last', delta: 'Change', onePoint: 'Only one measurement: no change calculated.',
    language: 'Language', partial: 'Display is bounded to the most recent observations.', programProgress: 'Program progress',
  },
} as const
