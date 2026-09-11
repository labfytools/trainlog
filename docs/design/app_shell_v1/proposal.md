# APP_SHELL_V1 — proposition soumise au design gate

Date : 10 septembre 2026. Baseline inspectée : `7cb4996`, après
`TRAINING_KNOWLEDGE_V1=PASS` et `SESSION_GENERATOR_V1=PASS`.
Le dépôt était propre à l'ouverture. **Design seulement ; aucune autorisation
d'implémentation n'est déduite de ce document.** Ce dossier est une proposition,
pas une description de fonctionnalités déjà livrées ni un contrat FROZEN.

Les [maquettes Android](android_mockups.html) sont consultables hors ligne,
avec un [aperçu PNG](android_overview.png). Les [maquettes TUI en couleur](tui_mockups.html)
et leur [version texte](tui_mockups.txt) donnent les quatre grilles exactes et les
overlays compacts. Les données affichées sont fictives, uniquement dans ces
documents. Aucun catalogue ni aucune base utilisateur n'est modifié.

## 1. Décision proposée

Un même classement de l'information, deux interactions natives :

- Android : drawer modal de premier niveau, pages de section et navigation
  vers les détails. Touches d'action de 48 dp minimum, contenu compact et défilant.
- TUI : shell persistant Notcurses ; navigation latérale quand la surface est
  suffisante, panneau de navigation temporaire dans les petits terminaux.
- Un titre et un emplacement stables pour chaque fonction. La sélection de la
  rubrique est dérivée de la destination ; elle n'est pas un second état.
- Mocha/Lavender, typographie système sur Android, hiérarchie par poids et
  alignement dans le terminal. Les surfaces regroupent ; les cadres ne décorent pas.

L'ancien **Historique** devient **Séances → Séances effectuées**.
**Programmer une séance** héberge le générateur existant : préparer une séance
unique, modifier sa proposition puis l'accepter dans l'éditeur ordinaire.
Ce libellé n'introduit ni calendrier, ni modèles réutilisables, ni programme
multi-séances, ni seconde séance planifiée persistante.

## 2. Ce que l'inspection a établi

| Sujet | Implémentation constatée | Conséquence pour le shell |
|---|---|---|
| Android navigation | `TrainlogApp.kt:21` : enum de huit écrans ; `remember`, cible de retour particulière pour la création d'exercice, ID de séance sélectionné ; `BackHandler` revient souvent à Accueil | Introduire un propriétaire unique de routes et des retours hiérarchiques/caller-aware |
| Android cadre | `TrainlogComponents.kt:39` : colonne entièrement défilante, bannière comprise ; composants Foundation faits maison | Extraire un vrai shell fixe ; une seule zone défilante par écran |
| Android thème | `TrainlogTheme.kt` : accent Teal `#94e2d5`, muted Blue, toute la police monospace | Passage explicite à Lavender et à une hiérarchie sans-serif ; conserver les rôles métier |
| Android dépendances | BOM Compose `2026.08.00`, activity-compose `1.13.0`, Foundation/UI ; pas Material 3 ni navigation-compose ni bibliothèque d'icônes | L'ajout de Material 3 est un choix de la future implémentation, pas une dépendance supposée présente |
| TUI structure | `tui.c` contient environ 13 000 lignes, des boucles d'écran imbriquées et des états locaux | Extraction progressive des contrôleurs ; une seule boucle d'événements finale |
| TUI surfaces | `terminal.c:14` possède le contexte et le plan standard ; `TrainlogPanel` est une vue de coordonnées et son commit est vide | Les panneaux actuels ne peuvent pas servir de shell persistant sans changer l'adaptateur |
| TUI état terminal | `tui.c:63` utilise un pointeur file-static pendant `trainlog_tui_run()` | Remplacer cet accès implicite par un contexte d'application passé explicitement |
| Notcurses | `pkg-config notcurses-core` et `/usr/include/notcurses/version.h` : **3.0.17**, liaison `-lnotcurses-core` | Sélection d'API fondée sur ce header, sans mise à niveau nécessaire |
| Équipements | Android : choix/création dans la saisie ; TUI : catalogue autonome. Création/liste/résolution disponibles, modification de définition absente | Ajouter le point d'entrée Android en réutilisant les opérations existantes ; ne pas inventer un éditeur de définition |
| Persistance | Desktop et Android v11 dans le code et `docs/current_state.md` ; brouillon Android durable, brouillon TUI en mémoire | Le shell ne crée aucune migration ; ne pas promettre une reprise TUI après arrêt du processus |

Le texte desktop v9 de `AGENTS.md` §8 est un état ancien par rapport au code
et aux documents actuels v11. La proposition ne résout pas cet écart par une
modification de schéma. La formule « un plan standard » de `docs/tui.md`
décrit le backend existant ; les plans enfants proposés conservent un seul
contexte et un seul plan standard. Les contrats produit et de format restent
intacts. La nouvelle séquence demandée ici sera synchronisée dans la roadmap
lors de l'implémentation approuvée, sans annoncer le shell comme déjà livré.

## 3. Architecture de l'information finale

Légende : **V1** = exposé par la future implémentation APP_SHELL_V1 ;
**existant PC** = fonction déjà disponible seulement sur desktop ;
**futur** = emplacement réservé, absent des menus V1 tant que non implémenté.

```text
Accueil                                             V1

Séances                                             V1 : page de section
  Séance en cours                                   V1 : état/reprise
  Programmer une séance                             V1 : générateur existant
  Nouvelle séance manuelle                          V1 : action
  Séances effectuées                                V1 : ancien Historique
    Détail d'une séance
      Exercices / occurrences / séries / activité / MAX / plan
      Modifier                                      selon capacité actuelle
      Reprendre ce Test max                         Android, stable ID

Exercices                                           V1 : ouvre Catalogue
  Catalogue                                         V1
    Fiche exercice                                  V1
      Zones du corps / connaissances et sources      V1
      Équipements compatibles / utilisés             selon données disponibles
      Performances / MAX                            liens vers vues existantes
      Modifier                                      V1, action contextuelle
  Créer                                             V1, action de catalogue

Équipements                                         V1 : ouvre Catalogue
  Catalogue                                         V1, autonome aussi Android
    Fiche équipement                                V1
      Fourni / Personnel / Référence inconnue        V1, selon résolution réelle
      Modifier la définition                        futur, API métier absente
  Créer                                             V1, capacités actuelles

Statistiques                                        V1 : page de section utile
  Vue d'ensemble                                    futur STATS_V1
  Fréquence                                         futur STATS_V1
  Progression                                       futur STATS_V1
  Par exercice                                      existant PC ; enrichi STATS_V1
  Zones du corps                                    futur STATS_V1
  Mensurations                                      V1
    Relevés / Ajouter / Détail
    Modifier / Graphiques / Analyse corporelle        existant PC
    Vue sur 12 mois                                  existant PC, déplacée d'Accueil
  Capacités / MAX                                    V1, vues actuelles seulement
    Derniers MAX locaux                             Android
    Mesures / historique / graphe / charges de travail existant PC

Synchronisation                                     V1
  État / Lancer / Résultat / Diagnostic               capacités actuelles
  Journal / Détail d'une synchronisation              existant PC
  Dossier d'échange / Récupération                    Android

Paramètres                                          V1, contenu par plateforme
  Profil d'estimation corporelle                     existant PC
  Dossier d'échange                                  Android, grant SAF existant
```

**Créer et Modifier sont des actions, pas trois catalogues parallèles.**
On ouvre Exercices directement sur son catalogue ; « Créer » est visible en
tête. « Modifier » concerne l'exercice sélectionné ou sa fiche. Même logique
pour Équipements lorsque l'opération existe. « Modifier l'équipement de cette
occurrence » reste une action de séance : ce n'est pas la modification d'une
définition du catalogue.

Les sous-rubriques futures de Statistiques ne sont ni des boutons grisés, ni
des pages « bientôt disponible ». Le hub V1 montre uniquement Mensurations,
Capacités / MAX et, sur PC, Par exercice. STATS_V1 ajoutera des destinations
en utilisant les mêmes routes de section et les mêmes composants. Les zones
actuelles restent pleinement visibles dans Exercices et Programmer ; aucune
analyse par zone n'est anticipée. Android conserve sa mission de capture et de
consultation locale ; réserver une route n'autorise pas une copie de toutes
les analyses desktop.

Sur Android, « Voir les derniers MAX » depuis une fiche exercice ouvre la
liste agrégée existante sous Statistiques → Capacités / MAX. Ce lien ne promet
pas une page MAX filtrée par exercice, qui reste du ressort de STATS_V1.
Sur TUI, le contexte d'exercice continue à ouvrir ses vues de performance/MAX
existantes ; depuis Statistiques, un sélecteur d'exercice fournit ce contexte.

Paramètres a une véritable première destination sur chaque plateforme.
Pas d'interrupteur de thème, notifications, unités ou police sans fonction
réelle. Le profil d'estimation et le grant SAF gardent leurs propriétaires
actuels ; les anciens accès contextuels pointent vers le même éditeur.

## 4. Inventaire exhaustif des capacités visibles et destination

Cette matrice distingue les plateformes : préserver n'exige pas d'inventer
sur Android une opération réservée aujourd'hui au PC.

| Capacité actuelle | Android | TUI | Destination / conservation |
|---|---|---|---|
| Reprise séance en cours | Brouillon durable, résumé, avertissements de récupération | Éditeur courant en mémoire | Accueil → Reprendre ; Séances → Séance en cours |
| Nouvelle séance, normal/Test max | Oui | Oui | Séances → Nouvelle séance manuelle → type |
| Ajouter/éditer/retirer une occurrence ; même exercice plusieurs fois | Oui | Oui | Séance en cours → occurrences distinctes par `entry_id` |
| Créer un exercice pendant la saisie | Oui, avec retour au formulaire | Oui, via sélecteur | Action « Créer un exercice » dans la sélection ; retour au même appelant |
| Séries réelles hétérogènes, charge par série, valeur absente/0, virgule décimale Android | Oui | Oui | Éditeur d'occurrence, valeurs réelles séparées du plan |
| SETS + DURATION ; CONTINUOUS + DURATION ; vitesse/distance si configurées | Oui | Oui | Même éditeur piloté par métadonnées, aucun set fictif |
| MAX explicite sans séries | Oui | Oui | Type Test max ; résultat d'occurrence |
| Abandon explicite / sauvegarde / erreurs de validation | Brouillon durable, abandon confirmé, finalisation atomique | Édition en mémoire, écriture finale transactionnelle | Actions contextuelles, diagnostic persistant et retour sans écrasement |
| Génération par zone, objectif, durée personnalisée/préréglée | Oui | Oui | Séances → Programmer une séance |
| Exposition récente, avertissement, couverture partielle/vide, raisons et provenance de charge | Oui | Oui | Paramètres/proposition/détail du générateur ; toutes les explications consultables |
| Proposition modifiable, supprimer/réordonner/régénérer/accepter/annuler | Oui ; édition dans l'aperçu | Aperçu puis édition ordinaire après acceptation | Même point d'entrée ; respecter les étapes réellement disponibles, pas de réécriture de politique |
| Liste des séances terminées, date/type, détail des actuals et du plan | Oui, historique local | Oui, historique canonique | Séances → Séances effectuées → Détail |
| Correction complète d'une séance persistée, retrait d'exercice, rollback | Non, pas d'éditeur général actuel | Oui | Détail → Modifier sur PC |
| Reprendre le même Test max terminé en gardant ID/date | Oui | Édition persistée existante | Détail du Test max ; respect du conflit de brouillon existant |
| Changer/retirer l'équipement d'une occurrence terminée | Oui | Oui via édition | Détail → occurrence → Équipement |
| Catalogue d'exercices, recherche préfixe normalisé | Oui | Oui | Exercices → Catalogue |
| Créer/éditer nom et zones d'exercice ; profil protégé si référencé | Oui | Nom/zones et création selon capacités actuelles | Créer / Fiche → Modifier ; IDs inchangés ; aucun renommage en masse |
| Zone primaire, secondaires, groupes descendants, Non renseignés | Oui | Oui | Catalogue : filtre et résumé ; Fiche : détail ; Éditeur : sélecteur existant |
| Connaissances exercice, confiance, références scientifiques, cas non résolu | Oui | Oui, vue défilante | Fiche → Connaissances ; affichage existant sans nouvelle inférence |
| Équipements compatibles et usages historiques d'un exercice | Connaissances / contexte existants | Relations du manifeste et usages historiques séparés | Fiche → Équipements ; ne pas présenter un usage passé comme une preuve de compatibilité |
| Meilleures performances ordinaires | Pas de page analytique dédiée | Oui | Statistiques → Par exercice ; lien `p` depuis fiche/catalogue |
| Derniers MAX explicites | Dans Historique | Vue MAX | Statistiques → Capacités / MAX ; lien dans Séances effectuées |
| MAX : historique, graphe, résultat récent/meilleur, arrondi 0,5/1/2,5/5 kg | Consultation locale simple | Oui | Capacités / MAX → exercice ; assistance inverse et absence de pourcentage conservées |
| Catalogue équipements fournis, recherche par nom/étiquette/alias, détail | Pendant saisie | Autonome | Équipements → Catalogue ; sélecteur partagé dans l'éditeur |
| Création équipement personnel | Nom simple, sémantique actuelle | Nom/étiquette/type/mode de charge | Catalogue → Créer et action dans sélecteur, sans élargir les contrats des formulaires |
| Équipement historique introuvable | Résolution selon données reçues | Référence inconnue explicite | Fiche/occurrence : « Référence inconnue » et identifiant lisible |
| Saisie de toutes les mensurations actuelles | Oui | Oui | Statistiques → Mensurations → Ajouter ; raccourci Accueil |
| Relevés corporels récents | Oui, résumés | Oui, sélection/détail/édition | Mensurations → Relevés |
| Correction relevé : identité/date/lien séance préservés | Non | Oui | Mensurations → Détail → Modifier |
| Tendances, normalisation multi-mesures, distinction gauche/droite | Non | Oui | Mensurations → Graphiques |
| Graphique mensuel 12 mois aujourd'hui sur Accueil | Non | Oui | Mensurations → Vue sur 12 mois, mêmes mois vides et même sélection du dernier relevé |
| Composition, tendances, proportions, symétrie et libellés d'estimation | Non | Oui | Mensurations → Analyse corporelle, deux pages actuelles |
| Taille et branche de formule du profil d'estimation | Non | Oui | Paramètres → Profil d'estimation ; raccourci `p` conservé dans l'analyse |
| USB/MTP appareil, stockage, rafraîchir | Côté PC | Oui | Synchronisation → État |
| Android→PC, PC→Android, bidirectionnel et confirmation exacte | Requête Android bidirectionnelle | Trois actions directes | Synchronisation ; aucun nouveau protocole ni menu de mode intermédiaire |
| Snapshot automatique, import catalogue automatique, effets après sauvegarde | Oui | Import/export par moteur | Mêmes services et mêmes déclencheurs ; pas de relance à chaque navigation |
| Autoriser/changer dossier SAF, relire catalogue PC | Oui | Sans objet | Paramètres → Dossier d'échange ; liens directs dans Synchronisation |
| Requête en attente, reçu, diagnostic de résultat | Oui | Moteur/daemon | Synchronisation ; badge global seulement s'il reflète un état connu |
| Journal structuré, direction, détail, erreurs/conflits | Reçu courant | Oui | Synchronisation → Journal → Détail |
| UTF-8, clavier, petits terminaux, resize, aide et raccourcis | Touch/TalkBack/clavier matériel | Oui | Contrats transversaux du shell |

L'audit d'implémentation devra parcourir cette matrice ligne par ligne. Les
anciens écrans n'ont pas tous la même profondeur de fonctions : la proposition
ne promet pas une parité métier qui n'existe pas.

## 5. Android : drawer, pages de section, retour

Retenir **ModalNavigationDrawer + ModalDrawerSheet + NavigationDrawerItem**,
avec `Scaffold` et barre supérieure Material 3. La dépendance `material3`
sera ajoutée en s'alignant sur le BOM déjà utilisé. Les composants officiels
prennent en charge le drawer modal ; le choix des hubs est une décision UX
de Trainlog. [Documentation Android du drawer](https://developer.android.com/develop/ui/compose/components/drawer).

Le drawer contient sept lignes : Accueil, Séances, Exercices, Équipements,
Statistiques, Synchronisation, Paramètres. Pas de sous-arbre déroulant : les
longs libellés et les rubriques futures allongeraient inutilement le parcours
sur téléphone. Chaque ligne a un pictogramme, un libellé et une zone tactile
complète ; la rubrique active porte un fond Surface 0, une marque latérale
Lavender et l'état sémantique sélectionné. Sous Séances, un petit texte peut
indiquer « Séance en cours » si elle existe ; ce texte n'est pas un second lien.

La sélection d'une rubrique ouvre sa racine et ferme le drawer. Réappuyer sur
la rubrique courante revient à sa racine, avec le même garde de formulaire si
nécessaire. Les listes mémorisent recherche, filtre et position. Pas de piles
de navigation indépendantes par item de drawer.

Aux racines : `☰ TRAINLOG` et un titre de page normal dans le contenu.
Dans un détail : flèche Retour et titre court dans la barre, contexte de
rubrique en sous-texte ; le menu secondaire contient une action « Navigation »
pour changer de rubrique sans empiler des retours. Le drawer demeure le même.
Pas de hamburger et flèche Retour concurrents dans le même emplacement.

Ordre de Retour : IME si ouvert → dialogue/feuille → drawer → route appelante
→ hub/racine → Accueil → comportement Android normal. L'éditeur de brouillon
Android conserve ses données via le repository ; Retour ne supprime rien.
Les formulaires non durables et la proposition non acceptée demandent de
confirmer une sortie qui perdrait les modifications. L'ouverture du drawer
seule ne quitte pas le formulaire.

La création inline transporte une intention de retour avec l'ID de l'occurrence
ou du formulaire appelant. Après création, l'exercice reste sélectionnable et
le même éditeur reprend ; aucun second brouillon et aucun retour forcé Accueil.

## 6. Android : cadre et composants

```text
TrainlogTheme (MaterialTheme + rôles complémentaires)
  AndroidAppShell
    ModalNavigationDrawer
    Scaffold
      TopAppBar                 fixe, contexte/navigation/actions courtes
      ScreenHost                une seule destination, insets appliqués une fois
        SectionHeading          titre 22 sp, contexte court
        SearchAndFilters        si liste
        LazyColumn / Form       contenu défilant
      ContextActionBar          seulement si formulaire/aperçu, au-dessus IME
      SnackbarHost              feedback ponctuel non critique
    Dialog / ModalBottomSheet   garde de sortie, confirmation, choix borné
```

Un écran fournit titre, filiation et liste d'actions sémantiques. Il ne
redessine ni bannière globale ni bouton Retour géant dans son contenu.
Un seul bouton principal rempli par contexte : Reprendre, Générer, Accepter
la proposition ou Terminer la séance. Les actions secondaires sont des
boutons texte ; les actions par ligne ont un menu accessible « Actions pour… ».
Les suppressions ne dominent pas Accueil : elles vivent dans l'éditeur/menu,
avec confirmation. Les avertissements importants restent dans le contenu et
près de l'action concernée ; un snackbar ne suffit pas pour une erreur de
persistance ou un avertissement du générateur.

Le formulaire de séance distingue clairement : **Exercice**, **Équipement**,
**Objectif prévu**, **Séries réalisées**. Une proposition acceptée affiche zéro
série réelle tant que l'utilisateur n'en a pas enregistré. Les nombres et les
unités sont alignés ; la cible n'est pas un placeholder qui pourrait être
enregistré comme une valeur réelle.

Les lignes du catalogue utilisent titre + contexte court, pas une grande
carte par exercice. Les connaissances détaillées sont une route lisible ou
une feuille défilante ; la confiance et les sources restent présentes.

## 7. Android : typographie, icônes, tailles

| Rôle | Proposition Android | Usage |
|---|---|---|
| Titre application | Sans-serif système 18 sp / 24, semibold | Barre TRAINLOG |
| Titre écran | 22 sp / 28, semibold | Séances, Exercices ; pas de titre géant |
| Titre section | 16 sp / 22, semibold | Dernière séance, Séries réalisées |
| Texte courant | 16 sp / 24 | Libellés, explications et champs |
| Texte secondaire | 14 sp / 20 | Équipement, date, contexte ; jamais erreur essentielle en 12 sp |
| Valeur importante | 24 sp / 30, medium | Un MAX isolé, durée estimée |
| Valeurs alignées | 16 sp / 24, chiffres tabulaires si pris en charge | kg, répétitions, durée, colonnes |

Utiliser la famille Android par défaut ; tenter `fontFeatureSettings="tnum"`
dans les valeurs, puis réserver `FontFamily.Monospace` aux cellules numériques
si la police système ne fournit pas l'alignement attendu. Pas de téléchargement
de police ni nouvelle dépendance typographique. Le texte suit `fontScale` ;
une maquette HTML système ne prétend pas reproduire exactement le moteur Compose.

Cibles tactiles **48 × 48 dp minimum**, ce qui dépasse la demande d'environ
44 dp et suit le minimum Android. Ne pas compacter les cibles pour faire entrer
une ligne ; empiler les actions. Vérifier TalkBack, ordre de lecture, intitulés
de suppression par série/occurrence, annonces d'erreur et absence de doubles
descriptions des icônes décoratives. [Accessibilité Compose](https://developer.android.com/develop/ui/compose/accessibility/api-defaults).

Il n'existe actuellement que les vecteurs du lanceur. Ajouter une petite
sélection de **Material Symbols en VectorDrawable XML**, rendus par `Icon` /
`painterResource`, même épaisseur et taille optique 24 dp. Pas de police
d'icônes ni de gros paquet `material-icons-extended`. Cette forme de ressource
est recommandée par la documentation Android actuelle.
[Icônes Compose](https://developer.android.com/develop/ui/compose/graphics/images/material).

## 8. Android : adaptation aux téléphones

| Largeur utile | Comportement proposé |
|---|---|
| 320 dp | Marge 16 dp ; toutes les actions principales sur une colonne ; valeurs d'une série sur deux lignes si nécessaire ; drawer `min(360 dp, largeur - 56 dp)` = 264 dp |
| 360 dp | Marge 16 dp ; lignes de catalogue sur deux lignes ; éditeur rep/charge compact avec actions de ligne séparées ; drawer 304 dp |
| 393–412 dp | Même architecture, davantage de texte visible ; pas de nouvelle colonne de navigation ; drawer 337–356 dp |
| Paysage / fenêtre réduite | Hauteur défilante, barre d'action au-dessus de l'IME ; pas de cartes à hauteur fixe |
| ≥600 dp | V1 garde le drawer modal ; formulaire centré avec largeur max utile, listes utilisant la surface. Sidebar/rail persistant possible plus tard, pas requis V1 |

Les largeurs sont celles de la fenêtre disponible, pas des modèles de téléphone.
À 200 % de police, le drawer peut réduire sa marge de fond visible à 24 dp
pour laisser davantage de place aux libellés. Titres/actions peuvent passer
sur plusieurs lignes avec césure lisible des mots longs ; on ne
réduit pas le texte. La barre d'action grandit avec son contenu et sa hauteur
est déduite du viewport. Un seul propriétaire applique les insets du système
et de l'IME, pour éviter leur double ajout.

## 9. Accueil et hub Séances

Accueil répond dans cet ordre :

1. Séance en cours : type, nombre d'occurrences, action **Reprendre**.
2. Sans séance : action principale **Programmer une séance**, puis Nouvelle
   séance manuelle. Avec séance active, la reprise est prioritaire.
3. Dernière séance terminée : date, type, nombre d'exercices, accès au détail.
4. Dernier MAX explicite s'il existe et si la lecture actuelle le permet ;
   libellé « Dernier MAX enregistré », avec exercice/date/équipement, sans flèche
   de progression ni qualification de record inventée.
5. Accès rapide Mensurations et état de synchronisation connu.

Le compteur hebdomadaire et une synthèse de progression sont différés à STATS_V1 :
ils impliqueraient de figer de nouveaux calculs/calendriers sans nécessité
pour le shell. Le graphe corporel 12 mois actuel du PC reste accessible sous
Mensurations avec un lien depuis Accueil. Il ne disparaît pas du produit.

Le hub Séances présente quatre lignes/action groupées. Sans séance active,
« Aucune séance en cours » est du texte d'état, suivi de Programmer et Nouvelle
séance manuelle réellement actives. Avec une séance, « Nouvelle séance manuelle »
ouvre la reprise existante avec une explication ; aucune action n'écrase le
brouillon. Programmer peut ouvrir ses paramètres/aperçu ; l'acceptation
rencontre le conflit existant et propose Reprendre ou revenir à l'aperçu.
Un abandon du brouillon se fait explicitement dans son éditeur.

Pas de vignette « séance planifiée » fabriquée. La proposition non acceptée est
temporaire ; après acceptation, il s'agit de la séance en cours ordinaire.

## 10. TUI : architecture AppShell

```text
trainlog_tui_run(database empruntée)
  TrainlogAppContext                         durée d'un run, pas global
    NavigationState                         route + pile de retour bornée
    SessionController                       brouillon actif en mémoire
    ScreenController actif                  données / sélection / formulaire
    FocusManager                            une cible logique
    ActionModel                             actions disponibles du contexte
    OverlayStack                            transactions UI temporaires
    TrainlogTerminal                        seul propriétaire Notcurses
      stdplane                              racine empruntée au contexte
        header                              plan persistant
        sidebar                             plan persistant si layout large
        contentHost                         plan persistant
          viewport(s) du contrôleur          plans bornés aux rectangles alloués
        footer                              plan persistant, deux lignes
        overlayRoot(s)                      plans temporaires + widgets dédiés
```

Les plans persistants survivent aux changements d'écran. Un passage compact
peut désallouer la sidebar seule ; son état logique de navigation n'est pas
perdu. Le contentHost survit ; les vues filles sont démontées/remontées ou
redimensionnées. Un overlay ne détruit pas le contrôleur ou le contenu dessous.

Séparer les futurs fichiers par responsabilité : `app_shell.c`,
`navigation.c`, `focus.c`, `actions.c`, `layout.c`, `overlays.c`,
`components/list_view.c`, `components/search_field.c`, `components/form.c`,
`screens/{home,sessions,exercises,equipment,body,max,sync}.c`.
Ces noms sont une proposition interne, pas une nouvelle API publique.
`terminal.c` demeure la frontière Notcurses ; core/repository/sync ne voient
aucun plan, widget ni constante NCKEY.

Le contrôleur d'écran expose conceptuellement `enter`, `handle_action`,
`layout`, `render`, `leave`, `destroy` ; il retourne une intention sémantique
(`OpenRoute`, `OpenOverlay`, `Save`, `Back`), jamais un appel récursif à la
boucle d'un autre écran. Une seule boucle lit l'entrée et déclenche le rendu
composé. `render` ne fait pas de SQL et ne mute pas le domaine. Les lectures
sont effectuées par le contrôleur/data adapter avant rendu ; un résultat
immuable ou une erreur explicite est transmis à la vue.

## 11. API Notcurses sélectionnées et limites réelles

Source de signatures : `/usr/include/notcurses/notcurses.h` **3.0.17**.
Les pages web du projet sont complémentaires ; certaines synopsis HTML sont
mal formées, donc ne servent pas de déclaration C à recopier.

| Besoin | API disponible retenue | Discipline |
|---|---|---|
| Contexte | `notcurses_core_init`, `notcurses_stdplane`, `notcurses_render`, `notcurses_stop` | Un seul contexte, seul thread UI fait du rendu |
| Plans | `ncplane_create`, `ncplane_options` (`name`, `userptr`, `resizecb`), `ncplane_move_yx`, `ncplane_resize_simple` | Propriétaires explicites, rectangles contrôlés |
| Taille | `ncplane_dim_yx`, événement `NCKEY_RESIZE`, `notcurses_refresh` | Mesurer la géométrie courante, recalculer tout le layout avant rendu ; ne pas redimensionner stdplane manuellement |
| Resize callback | `ncplane_set_resizecb` | Marque le layout invalide seulement ; aucun SQL, mutation ou rendu récursif dans callback |
| Z-order | `ncplane_move_above/below`, `ncplane_move_family_top` | Empiler une famille d'overlay entière, footer au-dessus de la zone de contenu |
| Libération | `ncplane_destroy`, `ncplane_family_destroy` | Libérer les widgets avant leur famille restante ; aucune double destruction de leur plan |
| Couleurs/styles | `ncchannels_set_fg_rgb8`, `ncchannels_set_bg_rgb8`, `ncplane_set_channels`, `ncplane_set_styles`, `ncplane_set_base` | Rôles sémantiques centralisés ; bases opaques pour footer/overlay |
| Texte Unicode | Fonctions de sortie UTF-8 `ncplane_putstr_yx`, largeur en cellules via utilitaires existants/utf8proc | Couper/envelopper aux graphèmes, pas par octets ; unité de layout = cellule |
| Champ de recherche | `ncreader_create`, `ncreader_offer_input`, `ncreader_contents`, `ncreader_clear`, `ncreader_destroy` | Plan dédié, une seule ligne, contenu borné avant insertion ; contrat détaillé ci-dessous |
| Choix court | `ncselector_create`, `ncselector_offer_input`, `ncselector_selected`, `ncselector_destroy` | Type de séance/objectif/tri/actions courts ; `maxdisplay` calculé ; pas de catalogue complet dans le widget |
| Défilement | Viewport logique de liste + rendu des lignes visibles ; `ncplane_scrollup`/`ncplane_set_scrolling` seulement pour une zone contrôlée si utile | Le scrolling physique n'est pas la pagination des données |
| Souris | `notcurses_mice_enable(..., NCMICE_BUTTON_EVENT)`, `ncplane_translate_abs`, `notcurses_mice_disable` | Click/roue utiles, pas de mouvement continu nécessaire ; mêmes actions que clavier |
| Progression | `ncprogbar_create`, `ncprogbar_set_progress`, `ncprogbar_destroy` disponibles | Utiliser seulement si un vrai total/progrès est fourni ; sinon étape textuelle réelle, aucun pourcentage simulé |

`ncselector` est adapté aux petits choix, avec son titre et footer facultatifs
désactivés pour économiser des lignes. Il peut redimensionner son plan. Le
wrapper vérifie les longueurs/hauteurs autorisées ; si le choix ne tient pas,
le composant liste plat prend le relais. Le widget prend son plan en charge,
y compris en cas d'échec de création.
[Contrat officiel ncselector 3.0.17](https://notcurses.com/notcurses_selector.3.html).

`ncreader` n'est pas un éditeur de formulaire métier. Le wrapper intercepte
Entrée, Tab, Échap, F6 et F7 avant `offer_input`. Home/End, flèches, suppression
et caractères imprimables restent des entrées locales du reader focalisé.
Le `ncinput` brut reste privé à l'adaptateur. Le wrapper refuse les
contrôles/sauts de ligne et les insertions dépassant la capacité du champ,
assemble correctement les caractères composés, restitue la sélection/caret
et libère la copie allouée par `ncreader_contents`. La recherche catalogue
réutilise le plafond actuel de 200 octets UTF-8, avec message explicite au
dépassement. Défilement horizontal permis **seulement sous cette borne** ;
aucune croissance illimitée verticale. `NCREADER_OPTION_NOCMDKEYS` évite les
raccourcis implicites incompatibles. Le curseur n'est visible que tant que
le champ possède le focus. La saisie UTF-8 complexe fait l'objet d'un test
précoce ; si le reader ne tient pas ce contrat, le champ Trainlog existant
est conservé et rendu dans un plan dédié. Ce repli est défini, pas une promesse
d'API inexistante. [Limites documentées du reader](https://notcurses.com/notcurses_reader.3.html).

`ncmultiselector`, `ncmenu`, `ncreel`, `nctree` sont disponibles mais non retenus
pour le shell V1 : le modèle de zones primaire/secondaires existant ne doit pas
être remplacé par de simples cases indépendantes ; menu/reel/tree ajouteraient
une navigation ou des surfaces inutiles. Il n'existe pas de DataGrid métier
prêt à l'emploi dans les widgets inspectés. La table interactive Trainlog
compose de vrais viewports/headers avec sélection et événements, plutôt qu'un
texte statique ou un plan par ligne de toute la base.

**Le rattachement d'un plan enfant n'est pas un masque de clipping.** Chaque
plan de contenu doit rester dans son rectangle alloué ; les lignes sont
rendus dans le viewport visible, sans enfant déporté sous le footer. La hauteur
de liste ne devient jamais la hauteur totale du jeu de données. L'application
gère son OverlayStack : ce n'est pas une classe Notcurses inventée.
[Primitives de plans](https://notcurses.com/notcurses_plane.3.html).

## 12. Focus et dispatch TUI

Le focus est l'une des cibles : Navigation, Recherche, Liste/Table, Éditeur,
Actions du footer ou Overlay actif. Le header n'est pas un arrêt supplémentaire
sauf son contrôle Navigation en compact. Une seule cible reçoit une entrée.

Le focus visuel : `>` + libellé/ligne en Lavender et fond Surface 0. La
rubrique active porte aussi des crochets dans les maquettes texte et sa
sous-rubrique un point, distincts du `>` de focus. Une
sélection gardée dans une liste non focalisée conserve sa marque et un fond
neutre, mais pas le même accent que le composant actif. Un champ focalisé a
curseur et indication de champ ; une rubrique active du menu reste repérable
indépendamment du focus. Aucune couleur seule ne porte ces états.

Ordre du dispatch : resize/événements système → overlay supérieur → saisie
locale → actions du composant → raccourcis de route disponibles → action de
shell. PRESS/UNKNOWN et REPEAT volontaire restent une action logique ; RELEASE
est ignoré. Une entrée consommée n'est jamais réémise. Les lettres/chiffres
saisis dans un champ ne déclenchent pas de navigation.

Tab/Shift-Tab parcourt les composants visibles : navigation → recherche/filtre
→ contenu → actions du footer → navigation. En compact, le bouton Navigation
remplace la sidebar dans cette boucle. Dans un formulaire, Tab parcourt ses
champs puis ses actions et ressort ; aucun piège. Dans la table réelle existante,
Tab conserve sa fonction de cellule, et F6/F7 permettent de sortir directement.
Les cibles non disponibles sont absentes du parcours.

Après fermeture d'overlay, restaurer la cible logique et l'ID sélectionné,
pas un pointeur de plan détruit. Après resize, garder le même élément visible
et convertir le focus sidebar en contrôle Navigation si la sidebar disparaît.

## 13. Clavier et migration des raccourcis

Les anciens chiffres sont des **alias de destination**, pas les indices du
nouveau menu. Ils gardent donc leur sens même si Statistiques s'insère avant
Synchronisation. Les libellés du menu n'ont pas une numérotation trompeuse.

| Contexte / ancien raccourci | Nouveau comportement | Motif / compatibilité |
|---|---|---|
| `0` / Home : Accueil | Inchangé hors champ/overlay | Home dans un champ continue à déplacer le caret |
| `1` / F1 : nouvelle séance | Reprendre si active, sinon nouvelle manuelle | Évite de perdre le travail ; ne devient pas l'index de Séances |
| `2` / F2 : Historique | Séances → Séances effectuées | Même données, nouveau classement |
| `3` / F3 : Exercices | Catalogue Exercices | Inchangé |
| `4` / F4 : Équipements | Catalogue Équipements | Inchangé |
| `5` / F5 : Corps | Statistiques → Mensurations | Même données et actions |
| `6` : Sync | Synchronisation | Inchangé, F6 n'est pas un alias de 6 |
| `g` depuis Accueil | Programmer une séance | Même générateur ; ajouté au hub Séances seulement |
| Nouveau F6 | Navigation : focus sidebar ou ouvre panneau compact | Aucun ancien F6 ; aussi accessible comme contrôle textuel avec Tab |
| Nouveau F7 | Toutes les actions du contexte, aide comprise | Garantit accès aux raccourcis qui ne tiennent pas dans le footer |
| Nouveau `?` hors saisie | Aide contextuelle + alias globaux | Pas F1, déjà utilisé pour une séance |
| Tab/Shift-Tab : navbar/contenu | Cycle de composants, sens inverse conservé | Extension ; table de séries conserve son Tab-cellule |
| Flèches / PgUp / PgDown | Sélection, déplacement cellule, défilement/page du composant focalisé | Aucun changement implicite de rubrique en lisant une liste |
| Entrée liste exercice | Fiche exercice | Aligné sur `screen_exercise_detail`, malgré le raccourci descriptif ancien de docs/tui |
| `p` catalogue/fiche exercice | Performances de cet exercice | Vue existante réutilisée dans Statistiques |
| `m` catalogue/fiche exercice | MAX de cet exercice | Distinct de la performance ordinaire |
| `k` fiche/connaissances | Ouvrir/fermer connaissances | Inchangé |
| `e` fiche exercice | Modifier nom/zones | Stable ID ; pas EXERCISE_NAMING en masse |
| `a` catalogue exercice | Créer exercice | Inchangé |
| `n` catalogue équipement | Créer équipement | Conservé ; `n` n'est jamais raccourci Navigation global |
| `/` exercices/équipements | Recherche éditable, filtre en direct | Entrée rend le focus aux résultats ; aucun enregistrement |
| `/` séances effectuées | Même champ de recherche | Ajout ; filtre explicite de la liste, pas moteur de recherche global |
| `z` catalogue exercices | Cycle de zones actuel | Filtre interactif visuel ajouté sans changer descendants/Non renseignés |
| `x` catalogue exercices | Effacer recherche + filtre de zone | Inchangé ; ne pas généraliser à un contexte destructeur |
| `x` sélecteur équipement | Aucun équipement pour l'occurrence | Inchangé, ne devient pas effacement global de filtre |
| `e` / Entrée séance en cours | Éditer occurrence/séries | Inchangé |
| `r` séance en cours | Remplacer occurrence sélectionnée | Inchangé ; identité/ordre selon service existant |
| `a`, `d` séance | Ajouter / retirer occurrence | Retrait confirmé, catalogue intact |
| `f` séance | Enregistrer/terminer via garde actuelle | Inchangé ; action libellée dans F7 et le footer |
| `q` éditeur courant | Abandon explicite avec confirmation | Alias préservé ; ne quitte pas tout le processus |
| Échap éditeur global, auparavant abandon | Retour au hub en conservant l'édition en mémoire | Changement explicite requis pour une navigation sûre ; abandon via q/action |
| `a`, `d`/Delete, Entrée dans table de séries | Ajouter, supprimer, éditer cellule | Inchangé, jamais de valeurs cibles copiées |
| `f`/`b`, Échap dans table | Quitter la table vers l'occurrence ; Échap cellule annule sa saisie | Priorité locale conservée |
| Générateur : Entrée / `a` accepte l'aperçu ; `q`/Échap annule | Même actions et gardes ; détail via action dédiée | L'acceptation ne devient pas silencieusement « ouvrir détail » |
| Avertissement génération `c`/Entrée, `z`, `q` | Continuer, autre zone, annuler | Inchangé |
| Zones : `p`, Espace, `n`, Entrée, Échap | Primaire, secondaire, Non renseigné, valider, annuler | Modèle métier existant conservé |
| Détail séance : `e`, `i`, flèches/PgUp/PgDown | Éditer, fiche équipement, parcourir occurrences/lignes | Inchangé |
| Corps : `a`, `e`, `v`, `g` | Ajouter, modifier, analyse, superposition globale | Déplacé sous Mensurations ; `g` n'y génère pas une séance |
| Analyse corporelle : `p`, gauche/droite | Profil d'estimation, changer page | Même éditeur via Paramètres et même page analytique |
| Vue MAX : `r` | Cycle d'arrondi actuel | Pas de nouveau calcul |
| Sync : `a`, `p`, `b`, `r` | Android→PC, PC→Android, bidirectionnel, actualiser | Priorité locale ; `b` ne signifie pas Retour sur Sync |
| Sync : `s` anciennement retiré | Reste sans action | Ne pas réactiver un contrat obsolète |
| Sync confirmation : Entrée / Échap | Lancer une fois / annuler sans opération | Inchangé |
| Enfant : `b`/Échap Retour | Retour appelant, sauf conflits locaux ci-dessus | Aide affiche le sens exact du contexte |
| `q` racine application | Quitter, avec garde si état en mémoire | Aucun arrêt depuis un champ par lettre q |
| Confirmation historique `1`/`0` | Alias confirmer/annuler dans la confirmation de retrait concernée | Navigation globale suspendue ; focus initial sur Annuler |

Pas de Ctrl+S obligatoire : les terminaux peuvent l'interpréter comme contrôle
de flux. Les actions explicites et `f` restent la voie fiable. Les touches de
fonction ajoutées seront traduites dans `TrainlogKey`, sans fuite de NCKEY.

Garde de sortie du générateur : avant production d'un aperçu, Échap revient
au hub Séances, avec la garde de formulaire si une configuration modifiée
serait perdue. Une fois l'aperçu produit, `q`/Échap, Retour Android, navigation
par drawer et sortie de l'application passent par la même garde. Seule l'action
explicite « Abandonner la proposition » l'efface ; « Conserver et revenir »
la suspend dans le contrôleur en mémoire. Cela inclut ses modifications et
l'état d'acquittement de l'avertissement. Aucune restauration après mort du
processus n'est promise pour cet aperçu.

## 14. Listes, tables et recherche

Un modèle commun de composant transporte : ID stable de sélection, ordre,
requête, filtres, début de viewport, nombres connus, état chargement/erreur.
La liste ne prend pas l'index courant comme identité. Après édition, filtrage
ou sync, conserver l'ID si visible, sinon choisir le voisin déterministe et
annoncer le changement.

Les tables ont un en-tête fixe, des colonnes numériques alignées, une ligne
sélectionnée, le compteur de position (`3 / 24` si total connu), des marqueurs
`↑ autres` / `↓ autres`. Le mode compact replie les colonnes secondaires sous
la ligne sélectionnée ou dans le détail, sans masquer une valeur métier.
Les noms longs sont élidés en liste avec `…` ; la fiche révèle tout le texte.
Pas de défilement horizontal indispensable à 72 colonnes.

Recherche : `/` focalise un vrai champ de la liste, créé si nécessaire. Le
filtrage se met à jour à chaque modification validée du texte ; les lectures
sont coalescées et aucun résultat ancien ne remplace une requête plus récente.
Entrée revient à la liste en gardant le filtre. **Échap avec texte efface la
requête et reste dans le champ ; Échap à vide ferme le champ.** Un petit `×`
Android fait la même chose. Le filtre de zone ne disparaît pas sur Échap ;
`x` garde sa fonction complète dans le catalogue d'exercices.

Sémantique conservée : exercices = préfixe normalisé + filtre de zone ;
équipements = noms/étiquettes/alias via recherche existante. Pour les séances,
V1 filtre les libellés effectivement affichés (date et type) ; recherche par
exercice se fait depuis sa fiche/performances existantes. Ne pas promettre une
recherche plein texte historique sans définition. Les tris V1 restent ceux
des vues existantes ; ne pas ajouter une flèche de tri sans ordre réellement
implémenté. Un choix de tri futur aura toujours un tie-break par identité et
n'altérera pas les contrats chronologiques scientifiques.

Ressources : seuls les éléments visibles sont matérialisés graphiquement.
Les pages de lecture sont bornées, avec indicateur explicite quand le total
n'est pas connu (`lignes 1–12 · suite disponible`, pas un faux total). Certaines
API actuelles n'offrent que capacité + nombre et certaines vues capent leur
chargement (équipements 256, sync 64). La migration doit conserver un diagnostic
de résultat partiel ; elle ne doit pas annoncer une pagination exhaustive
automatique fournie par ces APIs. Ajouter au besoin des accesseurs de lecture
paginée **étroitement bornés**, avec le même ordre/filtres et sans schéma ni
écriture, fait partie du travail de support UI à spécifier avant codage.
L'absence de page suivante ne doit pas être confondue avec un plafond local.
Les données scientifiques ne reçoivent aucun plafond global nouveau.

## 15. Overlays et modalités

Confirmation courte : plan flottant centré, une surface et une bordure discrète,
texte explicite, boutons Annuler/Confirmer. Un seul chemin de validation évite
un double appel. Aide, détails longs, recherche contextuelle et récupération :
overlay défilant ou route de détail selon le besoin ; pas de succession de
boîtes minuscules pour éditer une séance entière.

L'OverlayStack possède : type, état temporaire, plan(s)/widget(s), action de
retour, cible de focus à restaurer. La profondeur est bornée par les parcours
autorisés (trois niveaux suffisent : formulaire → confirmation → aide), jamais
un empilement illimité. Un quatrième niveau est remplacé par une navigation
dans le panneau courant ou refusé avec diagnostic ; aucun état n'est perdu.

Tant qu'un overlay est ouvert, sa saisie est exclusive. Échap ferme le niveau
supérieur ou demande confirmation si sa fermeture perd une édition. Un clic
sur le fond ne valide ni ne déclenche l'écran du dessous. La navigation
globale est suspendue, notamment `1`/`0` dans une confirmation. Le footer
montre **les actions de cet overlay** ; il reste physiquement visible.

Le dimming est limité à la surface centrale, pas une couche opaque sur le
footer. À 72×20, l'overlay peut occuper tout le rectangle central 72×16 avec
une ligne de titre, contenu défilant et actions accessibles en footer ; ce
n'est pas un nouveau plein écran qui détruit le précédent.

Sync déjà confirmée : afficher la direction et l'étape réellement connue.
Ne pas ajouter un bouton Annuler si le moteur ne garantit pas l'annulation.
Conserver son exécution/exclusion actuelles ; pas de worker pool ni de nouveau
scheduler pour animer une barre. Les événements de navigation ne déclenchent
aucun second run ; à la fin, relire la taille et afficher le résultat.

## 16. Responsive TUI et rectangles exacts

Toutes les coordonnées ci-dessous sont zéro-based. Header : lignes `0..1`.
Footer : les deux dernières lignes. Le rectangle central est toujours
`(y=2, x=0, h=H-4, w=W)`. Les marges sont internes, pas des cadres externes.

| Terminal | Mode | Navigation | Contenu utile | Overlay |
|---|---|---|---|---|
| **120×35** | Étendu | Sidebar 22 colonnes, x0..21 ; séparateur x22 ; section active développée | x23..119, 97×31 ; tableau + détail inférieur ou deux colonnes si chaque bloc tient | Centré, largeur ≤80 et hauteur ≤29, dans rectangle central ; fond intact |
| **100×30** | Standard | Sidebar 22 colonnes, premier niveau seulement | x23..99, 77×26 ; sous-navigation dans hub/breadcrumb ; détail pleine largeur | Largeur ≤74, hauteur ≤24 ; champs sur une colonne si nécessaire |
| **80×24** | Compact | Aucun espace réservé à gauche ; `F6 Navigation` ouvre panneau de 32 colonnes | 80×20 ; listes sans panneau détail latéral | Confirmation ~64×10 ; formulaire jusqu'à 78×20 ; contenu défilant |
| **72×20** | Minimum | Aucun rail d'icônes ; Navigation devient overlay central complet | 72×16 ; titre/contexte compact et lignes de liste ; footer de 2 lignes intact | Jusqu'à 72×16, titre et viewport ; actions dans footer partagé |

Règle déterministe : sidebar si **W≥100 et H≥26** ; sous-section développée si
**W≥120 et H≥32**. Entre ces seuils, conserver le mode moins chargé. Un
terminal très large mais bas utilise donc le compact. Pas de mise à l'échelle
aveugle d'un rectangle unique. La sidebar étendue ne développe que la rubrique
active ; ses items futurs sont absents et son propre viewport peut défiler.

En compact, les sept rubriques sont toutes nommées dans Navigation. Séances
ouvre son hub avec les quatre actions ; pas besoin de deviner une icône.
Le header affiche « Séances / Effectuées » et « F6 Navigation » ; un chemin
très long est abrégé au parent + titre, le détail restant accessible.

Sous 72×20 : état petit terminal, dimensions actuelles/minimum lisibles,
action Quitter accessible et garde d'état en mémoire si nécessaire. Aucun
write, abandon ou changement de route sur resize. Les surfaces non adaptées
ne sont pas rendues ; le contrôleur reste vivant. Dès retour à une taille
valide, reconstituer la géométrie, le focus, le défilement et les overlays.

## 17. Footer contextuel : contrat anti-régression

Le footer appartient **uniquement au shell**, dans un plan opaque et réservé.
Ni écran, ni scrolling, ni modal ne peut peindre sur ses deux lignes. Il est
recomposé après chaque changement de focus/route/overlay et à chaque resize.
Le rendu de contenu reçoit un rectangle qui l'exclut. Éviter les chaînes de
raccourcis tronquées par `%.*s` comme méthode de layout.

Ligne 1 : déplacement/validation et actions les plus utiles au focus.
Ligne 2 : Navigation, Actions, Aide et Retour/Quitter quand disponibles. Les
alias redondants peuvent rester dans F7/Aide ; l'action n'est jamais supprimée
car la ligne est courte. Les intitulés viennent du même registre que le
dispatch ; toute action affichée possède un handler et toute action disponible
apparaît au moins dans F7. Un contexte vide expose Retour/Navigation/Aide.

Exemples tenant à 72 colonnes :

```text
↑↓ Choisir  Entrée Détail  / Rechercher  e Modifier
F6 Navigation  F7 Actions  ? Aide  Échap Retour

↑↓ Série  ←→ Cellule  Entrée Modifier  a Ajouter  d Supprimer
f Terminer  F6 Navigation  F7 Actions  Échap Retour

a Android→PC  p PC→Android  b Bidirectionnel
r Actualiser  F6 Navigation  F7 Actions  Échap Retour

Tab Choisir  Entrée Confirmer  Échap Annuler
Confirmation : Retirer cet exercice de la séance
```

Les avertissements ne remplacent pas le footer : une ligne/bannière défilante
au-dessus les porte, avec un marqueur permanent et un accès au texte complet.
Tester le footer sur liste vide, nom long, erreur, IME/reader, confirmation,
retour d'overlay, changement de focus, et resize aller-retour aux quatre tailles.

## 18. Tokens Catppuccin et stratégie d'icônes

**Recommandation : tokens plateforme synchronisés et documentés**, avec une
table canonique de rôles très courte. V1 n'ajoute pas un moteur de thème ni un
parseur JSON au démarrage, et n'emploie pas les catalogues scientifiques pour
les couleurs. Les adaptateurs C et Kotlin traduisent la même table. La revue
de changement de palette et une vérification légère de parité empêchent la
dérive observée aujourd'hui. Si plusieurs thèmes apparaissent plus tard,
la génération depuis un petit manifeste pourra devenir utile.

Palette de référence : [Catppuccin Mocha](https://catppuccin.com/palette/).
L'affectation sémantique suivante est la proposition Trainlog :

| Token | Couleur | Rôle |
|---|---|---|
| `background` | Base `#1e1e2e` | Zone principale |
| `chrome_surface` | Mantle `#181825` | Header, sidebar, footer |
| `backdrop` | Crust `#11111b` | Fond extérieur/atténuation d'overlay |
| `surface` | Surface 0 `#313244` | Groupe utile, ligne sélectionnée |
| `elevated_surface` | Surface 0 `#313244` | Dialogue, avec contour si séparation nécessaire |
| `separator` | Surface 1 `#45475a` | Séparation discrète, pas seule marque de focus |
| `text` | Text `#cdd6f4` | Contenu principal |
| `muted_text` | Subtext 1 `#bac2de` | Contexte, dates, équipement |
| `accent` / `focus` | Lavender `#b4befe` | Interaction, état actif |
| `on_accent` | Crust `#11111b` | Texte sur bouton rempli Lavender |
| `success` | Green `#a6e3a1` | Succès réel + libellé « Enregistré » |
| `notice` | Peach `#fab387` | Information demandant attention sans erreur |
| `warning` | Yellow `#f9e2af` | Avertissement explicite + texte |
| `error` | Red `#f38ba8` | Échec, erreur de saisie, suppression |
| `information` | Blue `#89b4fa` | Information utile + libellé, pas texte secondaire systématique |

Pas de nouvelle palette scientifique pour les graphes existants : conserver
leurs séries/semantiques, labels et différenciation, puis harmoniser leur
présentation dans UI_POLISH_V1 si nécessaire. Lavender ne signifie ni succès,
ni zone du corps, ni charge élevée.

Espacement Android : échelle 4/8/12/16/24 dp ; marge écran 16, séparations
internes 8/12, entre groupes 16/24. Rayon discret 8–12 dp sur une surface
groupée et sur les contrôles ; pas de capsule autour de chaque texte.
TUI : 1 cellule de séparation minimale, padding horizontal 1–2 cellules,
1 ligne entre groupes quand la hauteur le permet, sans conversion dp→cellule.
Les degrés d'emphase (normal/secondaire/actif/critique) sont communs, pas les
unités physiques ni les tailles de police.

| Concept | Android, vecteur suggéré | TUI, sens équivalent | Fallback terminal |
|---|---|---|---|
| Accueil | home | maison, si mode icônes choisi | Accueil |
| Séances | event_note | carnet/calendrier | Séances |
| Exercices | exercise | mouvement/exercice | Exercices |
| Équipements | fitness_center | haltère/matériel | Équipements |
| Statistiques | bar_chart | graphique | Statistiques |
| Synchronisation | sync | flèches de transfert | Synchronisation |
| Paramètres | settings | roue dentée | Paramètres |
| Connaissances | menu_book | livre | Connaissances |

TUI V1 fonctionne **par défaut avec les libellés et marqueurs Unicode simples**.
Les Nerd Font sont une amélioration opt-in, jamais détectées à tort à partir
de `TERM` ou de la seule largeur d'un glyphe : la présence réelle du dessin ne
se prouve pas ainsi. Si un mode Nerd est ajouté, il doit être réellement
actionnable dans les préférences de présentation ou une option locale explicite,
avec aperçu et retour immédiat au mode texte ; aucune modification de police
du terminal n'est tentée. En l'absence de cette option implémentée, aucun menu
ne la promet et le fallback textuel est la livraison V1.

Les glyphes ont une colonne fixe, mesurée en cellules ; s'ils ne tiennent pas,
le libellé est conservé et l'icône retirée. `>` marque la sélection sans
Nerd Font. Variante ASCII de flèches/traits si nécessaire ; accent/UTF-8 dans
les données reste conservé. Android n'emploie aucun glyphe Nerd.

## 19. Propriétaires d'état, durées de vie, reprise

| État | Propriétaire | Durée / restauration |
|---|---|---|
| Section active | Dérivée de la route canonique | Aucun booléen concurrent dans drawer/sidebar |
| Route, pile de retour, appelant inline | NavigationState plateforme | Pile bornée ; pas d'IDs d'objet dupliqués dans plusieurs états incohérents |
| Requête, filtre, sélection ID, scroll | État de destination | Conservé sur détail/retour et changement de section, cache borné |
| Android brouillon + raw partial fields | Repository/SQLite existants | Durable ; rechargé, jamais copié comme source de vérité dans le shell |
| TUI brouillon / correction de séance | SessionController d'un run | Conservé lors d'une navigation UI ; aucun engagement après sortie/crash |
| Proposition générée non acceptée | Contrôleur générateur | En mémoire ; aucune écriture à l'ouverture/render/resize/annulation |
| Correction non durable d'exercice/mensuration | Contrôleur formulaire | Sauvegarder explicitement ou confirmer perte ; erreur garde les valeurs |
| État de sync/reçu | Services actuels | Shell lit un résumé ; aucun deuxième moteur ni deuxième propriétaire d'opération |
| Focus et pile d'overlays | AppShell | Sauvegarde de cible logique, invalidation des handles de plan détruits |
| Plans/widgets | Adaptateur terminal / owner du composant | Libération explicite ; chaînes empruntées copiées avant destruction si nécessaires |
| Thème | Adaptateur plateforme, table de tokens | Données de présentation, hors DB métier et échange |

La route distingue la destination de son appelant : `sessions.completed.detail`
porte le session_id ; `stats.exercise.performance` porte l'exercise_id ; la
création inline reste une sous-route du workflow de séance et réutilise le
même composant de formulaire que la création standalone. Drawer/sidebar
dérivent leur sélection de cette route contextualisée ; le retour à l'appelant
n'exige pas un second état de rubrique. Un seul aperçu générateur et une seule
édition active sont retenus à la fois ; commencer un workflow qui remplacerait
un état non durable exige la garde de sortie.

Android : introduire un petit `AppRoute` typé (destination + IDs) et un
`AppState`/state holder à la racine. Une seule pile bornée suffit ; pas de
framework de navigation supplémentaire obligatoire pour V1. Des sauvegardes
Compose peuvent garder routes/requêtes/positions modestes, jamais sérialiser
une séance complète dans un Bundle. Les contrôleurs transitoires peuvent
survivre à une recréation d'Activity via state holder/ViewModel ciblé si utile ;
après mort du processus, l'Accueil propose explicitement la reprise du brouillon
durable, comme aujourd'hui. Une proposition non acceptée n'est pas restaurée
comme une séance. Ne pas ouvrir un écran de confirmation destructive à la
recréation sans revalider sa cible.

Les imports/exports racine et callbacks de sauvegarde actuels restent au niveau
de leur cycle de vie/service. Naviguer ne déclenche pas un `LaunchedEffect`
réinstallé par écran qui importerait plusieurs fois. Les résultats de génération
ou lectures asynchrones portent une identité de requête et ne mettent plus à
jour un écran détruit. Ne pas changer la réutilisation du repository ni partager
une connexion SQLite entre threads sans son contrat actuel.

TUI : une DB empruntée à `trainlog_tui_run(TrainlogDatabase *)`, fermée par son
propriétaire actuel. Aucun pointeur Notcurses process-global. Libérer : widgets
→ plans de contenu/overlays → plans du shell → contexte Notcurses. Sur échec
d'allocation, libérer uniquement ce qui a été créé et afficher une erreur
explicite ; ne pas perdre silencieusement un formulaire.

## 20. Plan d'implémentation après approbation explicite

1. **Figer le design approuvé et la matrice de parité.** Formaliser routes,
   action IDs, propriété des états, géométrie et exceptions clavier. Confirmer
   les requêtes paginées de support nécessaires sans élargir les règles métier.
2. **Socle de présentation.** Tokens C/Kotlin synchronisés ; Material 3 ajouté
   au BOM, vecteurs locaux ; `AppRoute`/ActionModel. Contrôle C17 précoce pour
   toute interface interne exposée via header, sans casser l'API/ABI publique.
   Les commentaires WHY / CONTRACT / INVARIANT accompagnent chaque changement
   de durée de vie, action, propriété, borne ou persistance dans le même patch.
3. **Shell TUI et banc de géométrie.** Contexte explicite, vrais plans, header,
   footer, layout, focus, overlay, input adapter ; tests de footer à 72×20 avant
   migration des longues listes. Aucun widget sur un plan possédé par le shell.
4. **Shell Android.** Extraire contenu de TrainlogScreen, drawer et scaffold,
   retour hiérarchique, insets, actions standard ; aucun deuxième scroll root.
5. **Listes et pages en lecture.** Accueil, hubs, séances effectuées, catalogues,
   connaissances, mensurations/MAX existants, sync journal. Déplacer le graphe
   12 mois sans changer son calcul. Accès autonome équipements Android.
6. **Formulaires et flux sensibles.** SessionController TUI, éditeurs, MAX,
   génération et acceptation, création inline, confirmations, SAF/sync. Migrer
   les boucles TUI écran par écran vers le dispatch unique ; aucun ancien
   `draw_shell` plein écran ne subsiste sur le parcours livré.
7. **Validation et revue.** Exécuter la matrice ci-dessous, revue ciblée puis
   un audit final de tranche ; réparer les régressions et synchroniser les
   documents canoniques après comportement stabilisé.

Les commits intermédiaires ne sont pas implicitement autorisés par ce plan.
Le présent design gate s'arrête avant l'étape 1 exécutable.

## 21. Validation prévue et risques à protéger

| Risque | Preuve exigée à l'implémentation |
|---|---|
| Footer effacé/coupé | Rectangles disjoints et capture de rendu aux quatre dimensions, après modals/focus/erreurs/resize ; F7 énumère toutes les actions |
| Contenu qui dépasse son parent | Test avec nom UTF-8 long, nombreux sets, overlay proche du bas ; aucune cellule peinte dans footer |
| Double destruction de widget/plan | Tests création/échec/destruction, ASan/UBSan sur allocation/resize et fermeture imbriquée |
| State perdu en naviguant | Brouillon Android brut `32,`, retour depuis création inline, rotation/background/force-stop ; TUI aller-retour de rubrique puis reprise |
| Faux engagement de durabilité TUI | Message explicite à la sortie avec séance en mémoire ; aucune base/brouillon sidecar ajouté |
| Générateur transforme cible en réel | Cas zéro actual, aperçu vide/partiel, avertissement non acquitté, accepter avec brouillon existant ; mêmes garde/rollback |
| Données/IDs perdus | Snapshot DB temporaire avant/après navigation seule ; aucune écriture, migration, renommage ; sauvegarde réelle contrôlée transactionnellement |
| Raccourcis cassés | Matrice ancien/nouveau ; collisions q/b/g/n/p/r/Home ; RELEASE ignoré ; widget ne redéclenche pas l'action |
| Dead navigation | Chaque destination visible et chaque action F7 mène à une capacité réelle, futures routes absentes |
| Analyse cachée/perdue | Parcours body, overlay, profil, MAX, pourcentages/arrondis et performance ordinaire ; aucun zéro artificiel ni changement de science |
| Sync répétée/altérée | Chaque direction conserve une confirmation et un appel moteur ; s inactif ; erreurs/reçus/SAF/manual recovery toujours accessibles |
| Unicode/search incomplets | Accents précomposés/décomposés, CJK, emojis/graphèmes, dépassement capacité, collage, noms longs ; aucune coupure d'octet |
| Liste incomplète présentée comme complète | Jeu au-delà des anciennes capacités ; indication/page suivante réelle, filtrage avant pagination |
| Petits téléphones/IME | 320/360/393/412 dp, portrait/paysage, police 100/130/200 %, TalkBack, cible 48 dp, boutons visibles au-dessus IME |
| Thème illisible | Contrastes mesurés, focus sans couleur, fallback sans Nerd Font, terminal sans true color testé |

Commandes futures : `meson compile -C build`, `meson test -C build
--print-errorlogs`, validateurs JSON/import, vérifications scientifiques
existantes si leurs consommateurs sont touchés, build Android Java 17
`assembleDebug`, tests UI/draft ciblés, C17 et ASan/UBSan au checkpoint C,
`git diff --check`, `git status --short`. Le hardware MTP et les captures
Android/TUI réelles demeurent des validations manuelles explicitement nommées.
Dans ce design pass, seule la cohérence des artefacts et l'inspection statique
sont validées ; aucun build de production ni test de dispositif n'est revendiqué.

## 22. Report explicite des tranches suivantes

| Tranche | Travail différé |
|---|---|
| EXERCISE_NAMING_V1 | Noms exercice distincts des noms de machine ; migration explicite de noms/affichage sans toucher exercise_id, entry_id, session_id, actuals, MAX, zones ou sync ; aucun exemple de maquette ne déclenche une migration |
| STATS_V1 | Vue d'ensemble, fréquence, progression, analyse par zones et enrichissement par exercice/capacités ; définition des calculs, périodes et périmètre Android ultérieure ; pas de graphiques fictifs pour remplir le menu |
| UI_POLISH_V1 | Ajustements visuels après usage réel : transitions, détails de densité, raffinements de graphes, icônes Nerd opt-in si non livrées, éventuelle navigation permanente grands écrans Android |
| Capacité distincte à définir | Modification des définitions d'équipement : règles d'identité, propriété fourni/personnel, synchronisation et mutation absentes aujourd'hui. Ni promise par APP_SHELL_V1 ni assimilée à EXERCISE_NAMING_V1 |

APP_SHELL_V1 livre l'organisation, le shell et la lisibilité fondamentales.
Le focus, le footer, l'accessibilité de base, la reprise et les fonctions
existantes ne sont pas différés sous prétexte de polish.

## 23. État du design gate

La proposition et ses maquettes sont destinées à la revue humaine. L'approbation
du design, lorsqu'elle sera donnée, précédera toute implémentation de production.

`APP_SHELL_V1_DESIGN=READY_FOR_HUMAN_REVIEW`

Aucune implémentation de production. Aucun renommage d'exercice. Aucun STATS_V1.
Aucun commit. Aucun push.
