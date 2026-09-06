# Trainlog Android

## Current status

```text
ANDROID_SCAFFOLD=IMPLEMENTED
ANDROID_THEME_PARITY=IMPLEMENTED
ANDROID_HOME=IMPLEMENTED
ANDROID_SESSION_SHELL=IMPLEMENTED
ANDROID_INLINE_EXERCISE_ROUTE=IMPLEMENTED
ANDROID_EXERCISE_SHELL=IMPLEMENTED
ANDROID_BODY_SHELL=IMPLEMENTED

LOCAL_PERSISTENCE=NEXT
MTP_SYNC=AFTER_LOCAL_WORKFLOW
```

## Visual contract

The application follows the same Trainlog language as the TUI:

```text
dark background
monospace typography
cyan/teal accent
yellow active frame
green success
red error
blue muted/navigation
magenta graph role
```

The launcher icon is only a themed `T`.

## Navigation

```text
Accueil
├── Enregistrer une séance
│   └── Créer un nouvel exercice
│       └── retour séance
├── Enregistrer un exercice
└── Enregistrer des mensurations
```

## Exercise contract

Android must consume the same model as the desktop application:

```text
recording_mode
tracking_mode
data_fields
```

No exercise input form may be inferred from its name.

## Toolchain

```text
AGP          9.4.0
Gradle       9.6
Kotlin       2.3.21
Compose BOM  2026.08.00
compileSdk   37
targetSdk    36
minSdk       26
JDK          17
```

A Gradle wrapper is intentionally not committed by the scaffold script unless
it can be generated locally. From this directory:

```bash
gradle wrapper --gradle-version 9.6.0
./gradlew assembleDebug
```
