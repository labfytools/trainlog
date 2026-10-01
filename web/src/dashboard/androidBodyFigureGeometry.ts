/*
 * WHY: Android's HomeScreen.kt bodyRegions() Canvas is the visual source of
 * truth for Trainlog's BODY ZONES silhouette. SVG cannot reuse Compose Path
 * objects directly, so these paths transcribe its normalized coordinates
 * exactly: Android fractions are multiplied by 100 for this viewBox.
 * CONTRACT: preserve the Android front/back region order and zone IDs. The
 * Web's session intensity and Catppuccin colors remain separate semantics.
 * INVARIANT: each face has the same 11 region paths as Android; head and neck
 * are Android's drawOval() and neckPath(), behind the colored regions.
 */
export const ANDROID_BODY_HEAD = { cx: 50, cy: 8.25, rx: 9.5, ry: 6.75 } as const
export const ANDROID_BODY_NECK = "M44.5 13 L43 19 C46 20.5 54 20.5 57 19 L55.5 13 Z"

export const ANDROID_BODY_REGIONS = {
  front: [
    { id: "front-shoulder-left", zone: "shoulders", d: "M48 17.5 C40 17 30 16.5 22 21.5 C18 24 18 28.5 23 30 C30 27.5 38 25 48 24.5 Z" },
    { id: "front-shoulder-right", zone: "shoulders", d: "M52 17.5 C60 17 70 16.5 78 21.5 C82 24 82 28.5 77 30 C70 27.5 62 25 52 24.5 Z" },
    { id: "front-chest-left", zone: "chest", d: "M27 28.5 C32 24.5 40 24 49 26.5 L49 38 C42 39.5 34 38 29 34.5 Z" },
    { id: "front-chest-right", zone: "chest", d: "M73 28.5 C68 24.5 60 24 51 26.5 L51 38 C58 39.5 66 38 71 34.5 Z" },
    { id: "front-arm-left", zone: "arms", d: "M21.5 25.5 C16 28.5 14.5 36.5 13 43 L10.5 56 C12 59 17 59.5 19 55.5 L25.5 30.5 Z" },
    { id: "front-arm-right", zone: "arms", d: "M78.5 25.5 C84 28.5 85.5 36.5 87 43 L89.5 56 C88 59 83 59.5 81 55.5 L74.5 30.5 Z" },
    { id: "front-core", zone: "core", d: "M30 36 C35 40 38 43 37 55 C41 58 59 58 63 55 C62 43 65 40 70 36 C60 39 40 39 30 36 Z" },
    { id: "front-thigh-left", zone: "thighs", d: "M35 56.5 C31 63 30 71 31.5 80.5 C34 83 40.5 82.5 43.5 79.5 L47.5 57 Z" },
    { id: "front-thigh-right", zone: "thighs", d: "M65 56.5 C69 63 70 71 68.5 80.5 C66 83 59.5 82.5 56.5 79.5 L52.5 57 Z" },
    { id: "front-calf-left", zone: "calves", d: "M31.5 80.5 C28.5 86 31.5 91 33.5 98 L40.5 98 C41 91 45 85.5 43.5 79.5 Z" },
    { id: "front-calf-right", zone: "calves", d: "M68.5 80.5 C71.5 86 68.5 91 66.5 98 L59.5 98 C59 91 55 85.5 56.5 79.5 Z" },
  ],
  back: [
    { id: "back-shoulder-left", zone: "shoulders", d: "M48 17.5 C40 17 30 16.5 22 21.5 C18 24 18 28.5 23 30 C30 27.5 38 25 48 24.5 Z" },
    { id: "back-shoulder-right", zone: "shoulders", d: "M52 17.5 C60 17 70 16.5 78 21.5 C82 24 82 28.5 77 30 C70 27.5 62 25 52 24.5 Z" },
    { id: "back-trunk", zone: "back", d: "M28 28.5 C35 25 43 24.5 50 26.5 C57 24.5 65 25 72 28.5 C68 38 64 47 62 55 C56 57 44 57 38 55 C36 47 32 38 28 28.5 Z" },
    { id: "back-arm-left", zone: "arms", d: "M21.5 25.5 C16 28.5 14.5 36.5 13 43 L10.5 56 C12 59 17 59.5 19 55.5 L25.5 30.5 Z" },
    { id: "back-arm-right", zone: "arms", d: "M78.5 25.5 C84 28.5 85.5 36.5 87 43 L89.5 56 C88 59 83 59.5 81 55.5 L74.5 30.5 Z" },
    { id: "back-glute-left", zone: "glutes", d: "M37 53.5 C31 57 32 64 39 66.5 C43 67.5 47.5 64.5 49 59.5 L49 55 Z" },
    { id: "back-glute-right", zone: "glutes", d: "M63 53.5 C69 57 68 64 61 66.5 C57 67.5 52.5 64.5 51 59.5 L51 55 Z" },
    { id: "back-thigh-left", zone: "thighs", d: "M35 56.5 C31 63 30 71 31.5 80.5 C34 83 40.5 82.5 43.5 79.5 L47.5 57 Z" },
    { id: "back-thigh-right", zone: "thighs", d: "M65 56.5 C69 63 70 71 68.5 80.5 C66 83 59.5 82.5 56.5 79.5 L52.5 57 Z" },
    { id: "back-calf-left", zone: "calves", d: "M31.5 80.5 C28.5 86 31.5 91 33.5 98 L40.5 98 C41 91 45 85.5 43.5 79.5 Z" },
    { id: "back-calf-right", zone: "calves", d: "M68.5 80.5 C71.5 86 68.5 91 66.5 98 L59.5 98 C59 91 55 85.5 56.5 79.5 Z" },
  ],
} as const
