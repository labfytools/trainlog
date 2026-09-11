# APP_SHELL_V1 — éléments de revue du design

Date : 10 septembre 2026.

## Périmètre réellement exécuté

- Inspection du dépôt propre à `7cb4996` : `SESSION_GENERATOR_V1` après
  `TRAINING_KNOWLEDGE_V1`.
- Scout TUI isolé, lecture seule : structure des écrans, clavier, header/footer,
  géométrie, API terminal et Notcurses installé.
- Inspection Android par le parent : routes, state owners, formulaires,
  brouillon/génération, historique/MAX, catalogues, mensurations, synchronisation,
  thème, typographie, ressources vectorielles et dépendances.
- Advisor `gpt-5.6-terra`, high, contexte isolé, lecture seule : première revue
  de l'architecture puis vérification bornée des documents/maquettes écrits.
- Création exclusive d'artefacts dans `docs/design/app_shell_v1/`.
  Aucune modification de code de production, DB, catalogue, build, format ou
  document canonique décrivant l'implémentation actuelle.

## Conclusion de la revue architecturale

L'advisor n'a relevé aucune contradiction architecturale bloquante dans le
design final examiné. Il a confirmé notamment : propriétaire unique de route,
état Android durable / TUI limité au run, alias clavier indépendants de l'ordre
visuel, F6/F7 suffisants, rectangles header/content/footer, absence de clipping
implicite des plans enfants, ownership des widgets, routes futures masquées,
pas d'édition fictive de définition d'équipement, pas de migration métier.

Deux précisions demandées lors de sa dernière lecture ont été intégrées :

1. Le lien de fiche exercice Android devient **Voir les derniers MAX** et ouvre
   la liste existante dans Statistiques. Il ne prétend pas fournir une page
   d'analyse filtrée par exercice inexistante aujourd'hui.
2. Toute sortie d'une proposition générée existante passe par une garde
   explicite ; seule **Abandonner la proposition** l'efface. Une sortie sûre
   peut la conserver dans son contrôleur en mémoire. Cette règle couvre
   Échap/q, Retour, drawer et sortie de l'application.

Le wrapper reader précise également que Home/End, flèches, suppression et texte
restent locaux au champ. Les événements bruts Notcurses demeurent privés à
l'adaptateur. Le comportement de destruction de `ncselector`, absent du
commentaire local du header, a été établi par sa documentation officielle
3.0.17, qui couvre aussi l'échec de création.

Cette revue est une revue de **design**, pas un audit d'une implémentation du
shell ni un gel PASS/FROZEN.

## Vérifications des artefacts

| Vérification | Résultat |
|---|---|
| Android : neuf vues statiques | Présentes ; IDs HTML uniques, références SVG et liens locaux résolus |
| Android : inspection visuelle | Rendu Firefox headless consulté à 360 px/100 % ; aperçu 320 px/200 % consulté pour le reflow |
| TUI texte | Six grilles : 120×35, 100×30, 80×24, 72×20, navigation 72×20, confirmation 72×20 |
| Bornes des grilles | Exactement la hauteur annoncée ; chaque ligne ≤ largeur annoncée en cellules Unicode ; deux lignes finales de footer non vides |
| TUI couleur | Rendu HTML du 120×35 inspecté ; il reprend les mêmes grilles, pas une capture de l'application actuelle |
| Scripts des deux visionneuses | Vérification de syntaxe `node --check` réussie |
| Code suivi par Git | `git diff --exit-code HEAD` et `git diff --cached --exit-code` passent |
| Espaces du diff suivi | `git diff --check` passe ; contrôle des nouveaux fichiers textuels fait séparément |

Contrastes calculés en sRGB pour les paires opaques proposées :

| Paire | Rapport |
|---|---:|
| Text / Base | 11,34:1 |
| Subtext 1 / Base | 9,26:1 |
| Subtext 1 / Surface 0 | 7,10:1 |
| Lavender / Surface 0 | 7,03:1 |
| Crust / Lavender | 10,48:1 |
| Error / Surface 0 | 5,43:1 |
| Warning / Surface 0 | 9,89:1 |

Ces résultats ne certifient pas tous les états d'accessibilité futurs.
Ils vérifient les couleurs et les artefacts du design ; les états réellement
composés, TalkBack, IME, polices système Android et palette de terminal seront
validés pendant l'implémentation.

Aucun build/test de production n'était requis par une modification de code :
il n'y en a pas eu. Aucun lancement de la TUI sur la base réelle, installation
Android ou essai MTP matériel n'est revendiqué. Aucun commit ni push.

## Sources de l'inspection

Sources du dépôt :

- [État courant](../../current_state.md), [contrat TUI](../../tui.md),
  [contrat Android](../../android.md), [architecture](../../architecture.md).
- [TUI actuelle](../../../tui/src/tui.c),
  [adaptateur terminal](../../../tui/src/terminal.c),
  [clavier Trainlog](../../../tui/include/trainlog/terminal.h),
  [interfaces DB existantes](../../../tui/include/trainlog/database.h).
- [Routes Android](../../../android/app/src/main/java/com/labfytools/trainlog/ui/TrainlogApp.kt),
  [composants](../../../android/app/src/main/java/com/labfytools/trainlog/ui/TrainlogComponents.kt),
  [thème](../../../android/app/src/main/java/com/labfytools/trainlog/ui/theme/TrainlogTheme.kt),
  [dépendances](../../../android/app/build.gradle.kts).
- [Repository Android](../../../android/app/src/main/java/com/labfytools/trainlog/data/TrainlogRepository.kt),
  [générateur](../../../android/app/src/main/java/com/labfytools/trainlog/ui/SessionGeneratorScreen.kt),
  [historique/MAX](../../../android/app/src/main/java/com/labfytools/trainlog/ui/HistoryScreen.kt).

Provenance Notcurses : `pkg-config --modversion notcurses-core` → `3.0.17` ;
`pkg-config --libs notcurses-core` → `-lnotcurses-core` ;
`/usr/include/notcurses/version.h` et `/usr/include/notcurses/notcurses.h`.
Les signatures installées font foi, certaines synopsis HTML upstream étant
mal formées.

Références officielles consultées pour les choix de présentation/API :

- [Drawer Compose](https://developer.android.com/develop/ui/compose/components/drawer).
- [Material 3 et sa dépendance](https://developer.android.com/develop/ui/compose/designsystems/material3).
- [Accessibilité et minimum tactile 48 dp](https://developer.android.com/develop/ui/compose/accessibility/api-defaults).
- [Icônes vectorielles Android](https://developer.android.com/develop/ui/compose/graphics/images/material).
- [Palette Catppuccin](https://catppuccin.com/palette/).
- [Plans Notcurses](https://notcurses.com/notcurses_plane.3.html),
  [selector 3.0.17 et propriété du plan](https://notcurses.com/notcurses_selector.3.html),
  [reader et ses limites](https://notcurses.com/notcurses_reader.3.html).

## Arrêt demandé

`APP_SHELL_V1_DESIGN=READY_FOR_HUMAN_REVIEW`

La tranche s'arrête au design gate demandé. Une approbation explicite reste
nécessaire pour commencer l'implémentation de production.
