# TRAINLOG_SYNC_GAP_CONTRACT_V1

## Causal deletion closeout implementation note

The staged causal-deletion implementation derives deletion admission from the
complete affected business projection, not only the parent row. Session
projection includes ordered occurrences, plans, confirmed facts, MAX,
occurrence/session note revisions, immutable feedback revisions and linked
observation identities. Observation projection includes every measurement,
its session identity and note revision. Feedback projection includes its full
immutable revision history. Supported corrections also persist a non-deleted
mutation revision, so a change followed by a content revert does not erase
evidence of the intervening edit. Unknown or incomparable ancestry conflicts;
timestamps never arbitrate it.

Execution-draft deletion uses `(session_id, revision_id)` on Android active and
pending storage and on desktop storage. Serialized JSON order, whitespace,
local row IDs and unfinished Android form strings are not causal identity.
Built-in exercise/equipment authorization is identical for local and imported
operations, including aliases that resolve to a built-in exercise.

Statut : `CONTRACT_FROZEN / IMPLEMENTATION_NOT_STARTED`

Ce document définit la cible produit d'une synchronisation complète sans
modifier les formats, schémas, moteurs, interfaces ou transports de production.
Les formes JSON décrites ici sont des **spécifications non implémentées**. Elles
ne sont acceptées par aucun importeur actuel.

Les mentions normatives sont :

- **[ACTUEL DÉMONTRÉ]** : comportement observé dans le code ou un test ;
- **[CIBLE DÉCIDÉE]** : décision produit de ce contrat ;
- **[CONTRAT FUTUR — NON IMPLÉMENTÉ]** : obligation d'un lot ultérieur ;
- **[OUVERT/BLOQUÉ]** : choix ou validation qui reste à fermer.

## 1. Principe et frontières

**[CIBLE DÉCIDÉE]** Une donnée d'entraînement destinée à plusieurs clients est
synchronisable. Une préférence propre à un appareil ou une interface reste
locale. Le desktop est le hub qui orchestre un run ; il n'est pas, par ce seul
fait, l'autorité absolue de chaque domaine.

**[ACTUEL DÉMONTRÉ]** Android et desktop échangent principalement par MTP dans
`Documents/Trainlog`. `trainlog_sync_run()` orchestre le moteur commun au TUI et
à `trainlog-syncd`, sous `$XDG_DATA_HOME/trainlog/sync.lock`. Google Drive porte
le brouillon IA entrant et l'export d'analyse IA, pas les séances ordinaires.
Les formats mobiles actifs sont V3 et les companions restent indépendants.
Sources : `tui/src/sync.c`, `tui/include/trainlog/sync.h`,
`tools/trainlog_syncd.py`, `tools/sync_ai_session_draft_drive.py`,
`tools/post_sync_ai_drive.py`, `docs/sync_exchange.md`.

**[CIBLE DÉCIDÉE]** La séquence bidirectionnelle est :

```text
publication Android préalable
-> pull Android
-> validation de génération
-> merge/import desktop
-> midpoint Drive IA, état séparé
-> capture cohérente de l'état convergé
-> publication vers Android
-> consommation durable Android
-> acknowledgement corrélé
```

Un échange des fichiers MTP déjà présents n'est pas une synchronisation de
l'état courant si Android n'a pas publié après ses dernières mutations. Un run
Web doit donc pouvoir rester `waiting_android_publication`; il ne promet jamais
des données fraîches sur la seule présence du téléphone.

## 2. Matrice canonique courant/cible

`A` signifie Android, `D` desktop, `IA` pipeline Drive. `C/M/S` décrivent
création, modification et suppression. Une absence actuelle reste une absence,
pas une fonctionnalité implicite.

| Domaine et champs | Producteur / consommateur | Autorité | C/M/S et conflits | Format actuel démontré | Évolution cible et limite |
|---|---|---|---|---|---|
| Séance réalisée : `session_id`, début, type, occurrences, faits, plans, snapshots | A↔D | identité créatrice ; merge par politiques V3 | C idempotente ; corrections V3 bornées ; S absente | mobile V3 | V4 transporte lifecycle, causalité et tombstone |
| `ended_at` | A et D / A et D | valeur établie par le client qui finalise | `null→date` admis ; deux dates établies divergent explicitement | stocké localement, absent V3 | nullable, jamais fabriqué ni reconstruit sans source |
| note de séance | client éditeur / A et D | objet séance | bornée ; absent≠vide ; conflit causal | colonne locale, absente V3 | suit `session_id` |
| Occurrence : `entry_id`, ordre, exercice, équipement, targets, modes, faits, MAX | A↔D | identité créatrice ; profils immuables pour l'historique | corrections V3 bornées ; S suit séance | mobile V3 + companions | V4 conserve `entry_id`, ajoute causalité/suppression |
| note d'occurrence | client éditeur / A et D | objet occurrence | bornée ; conflit causal | colonne locale, absente V3 | suit `(session_id, entry_id)` |
| Activité continue | A↔D | occurrence | idempotence par artifact | mobile V3 | inchangé, inclus dans génération |
| Brouillon actif d'exécution | A puis futurs A↔D/Web | identité créatrice | un seul actif A ; aucun remplacement silencieux | tables Android `active_session_draft`, `draft_*`, exclues V3 | artifact lifecycle dédié ; synchronisable |
| Proposition IA | IA→D→A | proposition IA immuable | import idempotent, archive explicite | `trainlog-ai-session-drafts` | reste distincte d'un brouillon actif |
| Programme | futur | non défini | non implémenté | aucun modèle actif | dépendance future, hors lot |
| Template | futur | non défini | non implémenté | aucun modèle actif | distinct d'un programme et d'une séance préparée |
| Séance préparée | futurs D/Web→A | créateur puis lifecycle | non implémenté | proposition IA seulement | modèle futur distinct, peut devenir brouillon explicitement |
| Exercice catalogue canonique | A↔D selon identités/politiques existantes | identité stable ; métadonnées conciliées | rename sûr ; profils référencés protégés | mobile V3/catalogue/aliases | tombstone pour exercice utilisateur ; canonique non supprimable arbitrairement |
| Exercice utilisateur | A↔D | créateur | C/M causales ; S tombstone | pas de tombstone distribué | tombstone obligatoire |
| Alias d'exercice | A↔D | mapping historique canonique | union/flattening ; suppression interdite | companion aliases | conservation nécessaire à l'interprétation historique |
| Équipement canonique | D→A | catalogue canonique | non supprimable dans ce contrat | catalogue/definitions | génération, pas de tombstone arbitraire |
| Équipement personnalisé | A↔D | identité créatrice | conflit d'identité explicite ; S tombstone | definitions V1, pas de tombstone | tombstone causal |
| Association occurrence-équipement | A↔D | occurrence | `set/cleared` explicite | associations V2 | état explicite conservé dans génération |
| Relations BODY ZONES | A↔D | relation `(exercise_id, zone, role)` | trois voies ; S par état explicite/tombstone | companion V1 | opération causale explicite |
| Profils causaux | A↔D | politiques existantes | révisions causales | companion dédié | inclus dans génération sans réinterprétation |
| Observation corporelle : mesures, `observation_id` | A↔D | identité créatrice | C idempotente ; M/S non distribuées | mobile V3 | révision causale + tombstone |
| note d'observation | client éditeur / A et D | objet observation | bornée ; absent≠vide | colonne locale, absente V3 | suit `observation_id` |
| lien observation→séance | client éditeur / A et D | relation métier stable | parent manquant différé/rejet explicite | `session_row_id` local, absent V3 | transporte uniquement `session_id` |
| Feedback immédiat | A↔D | `(session_id, entry_id, exercise_id)` exact | révisions immuables, union stricte | feedback V2 | génération conserve identités/révisions |
| Feedback J+1 texte | A↔D | `session_id` exact | révisions immuables | feedback V2 | comportement actuel conservé |
| Feedback J+1 structuré | futur A↔D | séance par défaut | zones, intensité, texte ; exercice seulement explicite | absent | nouveau stockage et format nécessaires |
| Cardio physiologique | aucun | aucune | non implémenté | aucune source active | aucun contrat inventé |
| Langue, layout Web, fenêtre, état visuel, caches, champs de formulaire invalides | client local | appareil/interface | jamais fusionnés | locaux | restent locaux |
| Requête/receipt/run | A↔D | run desktop, requête Android | receipt corrélé au `request_id`, pas à une consommation complète | request/receipt V1 + historique | run/génération/ack distincts |
| Midpoint Drive IA entrant | IA↔D | flux IA | best effort observé séparément | scripts Drive | état dédié, erreur visible sans annuler le merge principal |
| Export Drive post-sync | D→Drive | desktop | best effort séparé | script post-sync | état dédié, jamais confondu avec succès principal |

## 3. États d'un run

**[CONTRAT FUTUR — NON IMPLÉMENTÉ]** Un run possède `run_id`, déclencheur,
direction, pair attendu, capacités négociées et les états monotones suivants :

```text
requested
waiting_android_publication | running
desktop_import_validated
published
waiting_acknowledgement
peer_consumed
completed
failed
```

`published` signifie seulement que le commit marker est visible.
`peer_consumed` exige un ack valide après consommation durable. `completed`
signifie que les phases obligatoires du run ont réussi ; les flux IA portent en
parallèle `not_requested|pending|succeeded|failed` et leur diagnostic. Un flux
best effort échoué n'est ni caché ni converti en échec du merge principal.

Une expiration en `waiting_acknowledgement` ne prouve pas l'annulation de
l'import pair. Un ack valide observé plus tard ferme idempotemment l'attente.

## 4. Brouillon et lifecycle

**[CIBLE DÉCIDÉE]** Ces concepts ne sont jamais fusionnés :

```text
programme != template != séance préparée != brouillon actif d'exécution
          != proposition IA != séance réalisée
```

`ai_session_drafts` ne devient pas un modèle générique. Une conversion entre
concepts est une transition explicite qui crée/conserve les identités prévues.

### 4.1 Inventaire Android v17

Sources : `TrainlogRepository.createActiveDraftTables()`, migrations v8-v17 et
`finalizeActiveSessionDraft()`.

| Table/champ | Classe cible | Justification |
|---|---|---|
| `active_session_draft.id=1` | local | clé technique singleton, jamais identité réseau |
| futur `session_id` stable | métier synchronisable | créé au brouillon, conservé à la finalisation |
| `session_type`, `source_session_id`, `updated_at` | métier synchronisable | lifecycle/provenance causale |
| `selected_exercise_row_id`, `selected_exercise_label`, `selected_equipment_id` | préférence/état UI local | sélecteur en cours, pas fait validé |
| `weight_text`, `max_weight_text`, `set_count_text`, `reps_text`, `duration_text`, `speed_text`, `distance_text` | saisie transitoire non validée | chaîne brute potentiellement invalide, reprise locale seulement |
| `draft_session_exercises.entry_id`, position, exercice/équipement, profils, targets, repos | métier synchronisable | occurrence validée et ordonnée |
| `draft_performed_sets` | métier synchronisable si série confirmée | faits ; une ligne de formulaire non confirmée reste locale |
| `draft_continuous_activity`, `draft_max_results` | métier synchronisable si confirmé | faits explicites, jamais fausses séries |
| `draft_exercise_feedback` et révisions | métier synchronisable | feedback rattaché à l'occurrence |

**[CIBLE DÉCIDÉE]** « Reprise exacte » signifie restaurer tout état métier
validé : type, ordre, occurrences, identités, équipement, targets, snapshots de
modes, faits confirmés, MAX et feedback. Elle ne promet pas de déplacer entre
appareils le focus, la sélection, un cache, une chaîne invalide ni une série non
confirmée. Ces champs locaux peuvent être perdus lors d'une reprise sur un autre
client ; cette limite doit être annoncée.

Le `session_id` est créé une seule fois à la création du brouillon et chaque
`entry_id` à la création de l'occurrence. La transition `draft -> completed`
utilise les mêmes identités dans une transaction locale. Son rejeu reconnaît la
séance déjà finalisée et ne crée ni seconde séance ni second brouillon.

### 4.2 Concurrence de lifecycle

- deux éditions du même brouillon utilisent révisions causales ; des branches
  concurrentes incompatibles deviennent un conflit, jamais du LWW général ;
- démarrage Android pendant édition Web : une seule branche peut acquérir
  l'état `started`; l'autre doit se rebaser ou demander une décision ;
- une finalisation domine une édition ancienne du brouillon, qui est rejetée
  comme lifecycle obsolète et ne recrée pas de brouillon ;
- suppression et finalisation concurrentes sont un conflit explicite ;
- reprendre un snapshot antérieur ne recule pas la causalité ;
- si le singleton Android est occupé, un brouillon entrant est conservé comme
  candidat en attente ; aucun remplacement silencieux ni fusion automatique.

## 5. Champs actuellement perdus

### 5.1 `ended_at`

**[ACTUEL DÉMONTRÉ]** Android et desktop stockent `ended_at`, mais mobile V3 ne
le transporte pas ; l'import desktop produit `NULL`. Le test
`sync_gap_characterization` le verrouille.

**[CIBLE DÉCIDÉE]** `ended_at` est nullable et synchronisable. `null -> date`
est l'établissement normal. `date A != date B` entre deux valeurs établies est
une divergence explicite nécessitant preuve causale ou décision ; aucune règle
`max(date)`. L'import ne fait jamais `now()` et ne déduit jamais la valeur de la
dernière série. Une absence historique ne signifie pas « encore en cours ».
Les dates déjà perdues ne sont pas reconstruites sans source vérifiable.

### 5.2 Notes

**[CIBLE DÉCIDÉE]** Notes de séance, occurrence et observation sont trois
champs distincts attachés à leur identité stable. Taille UTF-8 future : au plus
4096 octets chacun après validation UTF-8 ; `null` signifie jamais renseigné,
`""` effacement explicite. Une édition crée une révision causale ; deux éditions
concurrentes non identiques sont un conflit. Les notes de filtre, focus,
placeholder ou cache UI restent locales.

### 5.3 Observation corporelle → séance

Le wire ne transporte jamais `session_row_id`. La relation métier transporte
`session_id`. Si parent et enfant sont dans la même génération, le staging
résout après validation complète. Parent temporairement absent : relation en
attente bornée, jamais attachée par heuristique. Parent tombstoné : enfant
conservé selon sa politique, lien rejeté/retiré explicitement. Parent inconnu
après consommation complète : artifact invalide ou conflit rapporté.

## 6. Suppressions et non-résurrection

| Domaine | Politique cible | Motif |
|---|---|---|
| séances réalisées | tombstone | suppression possible, identité historique stable |
| brouillons | état lifecycle `deleted` avec fait causal | distingue abandon, finalisation et absence |
| exercices utilisateur | tombstone | empêche recréation par ancien snapshot |
| observations corporelles | tombstone | objet utilisateur supprimable |
| équipements personnalisés | tombstone | définitions référencées historiquement |
| feedback | état/révision de retrait, contenu historique conservé selon politique | révisions immuables |
| aliases | suppression interdite ; flattening canonique | interprétation des anciennes identités |
| relations BODY ZONES | opération d'état/tombstone ciblée | relation synchronisable à trois voies |

Un tombstone contient type, identité stable, opération causale, identité du
créateur et génération d'émission. Un timestamp est informatif, jamais arbitre
unique. Supprimer un parent traite explicitement les enfants : la séance
supprime logiquement ses occurrences ; supprimer un exercice utilisateur ne
détruit pas les faits historiques qui conservent leur snapshot ; supprimer une
observation traite son lien sans supprimer la séance.

Les tombstones sont conservés tant qu'un pair enregistré peut présenter un
snapshot antérieur. La collecte requiert un horizon causal prouvé par ack de
tous les pairs concernés ou une révocation explicite du pair ; elle n'est pas
basée sur l'âge seul. Un pair longtemps hors ligne doit consommer l'historique
causal ou effectuer une réinitialisation sûre. La réintroduction volontaire
crée une nouvelle identité ; réutiliser une identité tombstonée est une
résurrection accidentelle rejetée.

## 7. Conflits

Résolutions automatiques permises : rejeu byte-identique/idempotent ; union de
révisions immuables identiques ; application d'une opération causale qui domine
clairement l'état connu ; canonicalisation d'alias selon les règles gelées ;
correction V3 déjà démontrée dans ses limites.

Conflits explicites : même identité avec attribut immuable divergent ; éditions
concurrentes ; delete/update ; finalize/update ; génération obsolète qui tente
une résurrection ; causalité inconnue ou contradictoire ; parent supprimé ;
artifact corrompu, incomplet ou version future. Aucun last-write-wins général et
aucun écran de résolution ne sont définis dans ce lot.

## 8. Génération et manifest

**[CONTRAT FUTUR — NON IMPLÉMENTÉ]** Toute publication multi-artifact possède un
`generation_id` opaque unique (UUIDv4 préfixé `gen_` recommandé), jamais un
timestamp seul. Le manifest indépendant est conceptuellement :

```json
{
  "format": "trainlog-sync-manifest",
  "version": 1,
  "generation_id": "gen_<uuid-v4>",
  "producer": {"peer_id": "peer_<uuid-v4>", "kind": "android"},
  "generated_at": "offset-date-time",
  "artifacts": [{
    "logical_name": "mobile-history",
    "format": "trainlog-mobile-export",
    "version": 4,
    "filename": "generations/<generation_id>/mobile-v4.json",
    "size": 123,
    "sha256": "64 lowercase hex",
    "required": true
  }]
}
```

Limites : 32 artifacts ; manifest 64 KiB ; artifact 64 MiB et génération
256 MiB par défaut, sous admission du Resource Governor ; noms logiques 64
octets ASCII ; filename relatif UTF-8 ≤ 240 octets, segments non vides, aucune
racine, `..`, NUL, backslash ou symlink suivi ; profondeur ≤ 3 ; formats et
versions sur liste négociée ; identités selon préfixe + UUIDv4. Les limites sont
opérationnelles configurables, pas une limite scientifique globale de dataset.

SHA-256 porte sur les bytes immuables exacts. Le manifest peut donc relier sans
modifier les companions gelés. Le digest prouve intégrité et appartenance
déclarée ; il ne prouve pas que les fichiers furent capturés au même instant.
Chaque producteur doit capturer tous les artifacts depuis un même snapshot
logique de base (transaction de lecture SQLite ou snapshot équivalent), puis
les sérialiser déterministement.

## 9. Capture, publication, consommation et reprise

```text
capturer et valider G
-> publier les artifacts immuables de G
-> publier manifest/commit marker en dernier
-> consommer uniquement G complète, vérifiée et compatible
```

Le transport n'a besoin d'aucun renommage atomique global. Le consommateur
ignore les répertoires sans manifest final. La génération courante est la plus
récente génération **valide et causalement admissible explicitement publiée**,
pas le fichier au mtime maximal. Une nouvelle génération invalide est signalée
et bloque l'avancement ; aucun repli silencieux vers l'ancienne ne masque le
problème. La dernière génération valide est retenue avec la nouvelle jusqu'à
ack. Rétention minimale : dernière ackée + génération publiée en attente + une
précédente valide ; nettoyage seulement après absence de lecteur et preuves
d'ack, avec quota borné et diagnostic si reporté.

**[CIBLE DÉCIDÉE]** La consommation utilise staging validé puis une transaction
SQLite commune par pair et génération lorsque tous les domaines touchent la
même base. Les artifacts sont validés et transformés en staging borné avant la
transaction ; la transaction applique domaines, causalité et journal de
consommation, puis commit. Les effets externes restent séparés et idempotents.
Le manifest ne rend donc pas, à lui seul, les imports SQL atomiques.

Reprise : crash avant manifest → génération invisible/nettoyable ; après
manifest avant consommation → rejeu complet ; entre validations → aucun commit ;
pendant transaction → rollback SQLite ; après commit avant ack → journal local
reconnaît G et réémet l'ack ; pendant nettoyage → marqueurs et ordre de
rétention rendent l'opération idempotente.

## 10. Acknowledgement et rapport

L'ack versionné contient `run_id`, `generation_id`, identités producteur et
consommateur, digest du manifest, état `consumed|rejected`, garantie de
durabilité, timestamp et diagnostic borné. `consumed` n'est émis qu'après commit
et journal durable. Même génération + même manifest réémet le même résultat ;
même ID + digest différent est un conflit. Ack perdu, rejeu et observation
ultérieure sont idempotents. Un timeout laisse `waiting_acknowledgement`.

Le rapport futur sépare phases et domaines et expose : run/génération/pairs,
déclencheur, direction, timestamps, résultat, états IA, diagnostics et compteurs
`imported`, `exported`, `ignored`, `reconciled`, `deleted`, `rejected`,
`conflicts`. Une valeur non calculée est `null`/indisponible, jamais zéro.
`sessions_reconciled`, aujourd'hui produit par l'importeur mais perdu par
`TrainlogSyncReport`, doit être préservé.

## 11. Formats et compatibilité

`TRAINLOG_FORMAT_V1`, mobile V3 et companions publiés restent inchangés.
Architecture cible proportionnée : mobile V4 pour histoire complétée et champs
perdus ; artifact dédié brouillon/lifecycle ; companions spécialisés conservés ;
manifest global indépendant. Aucun monolithe universel.

Chaque pair publie `peer_id`, versions de formats, features causales, limite de
taille et niveau d'ack. Nouveau desktop/ancien Android : mode V3 explicitement
`degraded`, sans drafts, `ended_at`, notes, liens ni suppressions ; il ne peut
pas annoncer « sync complète ». Ancien desktop/nouvel Android : Android peut
émettre V3 compatible seulement si cela ne contourne aucun tombstone/révision ;
sinon refus clair. Une version future inconnue est rejetée. Une génération
nouvelle invalide n'est pas masquée. Après activation causale/tombstones, un
snapshot legacy susceptible de ressusciter est refusé ou admis uniquement via
une porte de migration prouvant sa base causale.

Portes : environnement/test ; lecture seule des capacités ; double-écriture
contrôlée sans autorité V4 ; round-trip validé ; activation par pair ; ack de
migration ; interdiction du downgrade destructeur. Aucune donnée réelle n'est
migrée dans ce lot.

## 12. Façade application et HTTP futurs

**[CONTRAT FUTUR — NON IMPLÉMENTÉ]** Une façade réutilise moteur et verrou :

```text
start_sync_job(trigger, direction) -> accepted(job_id) | already_active
get_sync_job(job_id) -> structured state
get_latest_sync_result() -> structured immutable result
```

Le serveur ne bloque pas sa boucle HTTP. L'exécution proposée est un processus
connu, argv fixe, sans shell, environnement/XDG contrôlé, stdout/stderr bornés,
timeout et arrêt du groupe de processus maîtrisés, admission Resource Governor
et verrou inter-processus existant. Une connexion SQLite indépendante ne
résout pas la contention avec les snapshots Web : lectures cohérentes,
busy-timeout borné et invalidation après commit sont requis.

API envisagée seulement : `POST /api/v1/sync` → `202` et
`GET /api/v1/sync/status`. Elle valide méthode, `Host`, `Origin`, CSRF, type et
taille du corps ; refuse un second job (`409`) ; borne réponses et fréquence.
États utilisateur : attente de publication Android, appareil absent, opération
active, import desktop validé, publication faite, attente d'ack, consommation
confirmée, terminé dégradé, échec. Aucun faux compteur. Le bouton ne dit jamais
« synchronisation complète » pour un simple upload, traitement Drive ou rejeu
de fichiers Android anciens.

## 13. Tests obligatoires des lots futurs

- round-trip complet de chaque domaine et champ, absent/vide/bornes UTF-8 ;
- drafts A↔D, singleton occupé, reprise exacte, finalisation idempotente et
  conservation `session_id`/`entry_id` ;
- transitions prepared/draft/completed/deleted et branches concurrentes ;
- update/update, delete/update, finalize/update, parents/enfants ;
- tombstones, ancien snapshot, pair longtemps déconnecté, collecte sûre et
  réintroduction volontaire ;
- générations mélangées, incomplètes, digest/taille/chemin invalides ;
- versions anciennes/futures, négociation, downgrade refusé ;
- crash à chaque frontière publication/consommation/commit/nettoyage ;
- ack perdu/tardif/rejoué, double import, même ID/digest divergent ;
- contention Web/TUI/daemon et lock ; sorties/processus/temps bornés ;
- flux IA midpoint et post-sync en succès/échec indépendants ;
- rapports : compteurs présents, `null` si indisponibles, réconciliations
  conservées.

## 14. Ordre d'implémentation futur

1. `TRAINLOG_SYNC_TEST_ENV_V1` : environnement Android/JDK/quota et harness
   isolé reproductible ;
2. `TRAINLOG_SYNC_CHARACTERIZATION_V1` : compléter les preuves actuelles sur
   toutes plateformes ;
3. `TRAINLOG_SYNC_DATA_LIFECYCLE_V1`: mobile V4, drafts and previously lost fields — `PASS/FROZEN`;
4. `TRAINLOG_SYNC_CAUSAL_DELETE_V1` : opérations causales et tombstones ;
5. `TRAINLOG_SYNC_GENERATION_ACK_V1` : manifest, staging, publication,
   consommation, reprise et ack ;
6. `TRAINLOG_SYNC_ORCHESTRATOR_REPORT_V1` : états et rapport complet ;
7. `TRAINLOG_WEB_SYNC_API_V1` : façade et endpoints ;
8. `TRAINLOG_WEB_SYNC_BUTTON_V1` : UX et rafraîchissement.

Chaque lot dépend des précédents et reste borné. Aucun n'est commencé par ce
contrat.

## 15. Statut des preuves

**[ACTUEL DÉMONTRÉ]** Desktop : test dédié des pertes V3, résurrection et
transaction par artifact ; suites existantes pour V3 strict, aliases,
équipements, BODY ZONES, profils, feedback, MAX et idempotence.

**[ACTUEL DÉMONTRÉ]** Android : le test existant
`TrainlogRepositoryDraftTest.finalizeIsAtomicAndDraftNeverExportsBeforeCompletion`
prouve l'exclusion du brouillon, la finalisation transactionnelle et
l'apparition de la séance finalisée dans l'export.

**[ACTUEL DÉMONTRÉ]** La validation de ce lot a exécuté les 202 tests Android
(198 réussis, 4 ignorés, 0 échec) et `assembleDebug` avec le JDK 17 documenté.
Le quota du tmpfs `/tmp` a nécessité un `java.io.tmpdir` ticket-spécifique sous
le home ; Robolectric est resté actif.

**[ACTUEL DÉMONTRÉ]** `TRAINLOG_SYNC_TEST_ENV_V1=PASS/FROZEN` fournit désormais
le harness reproductible : JDK 17 contrôlé, JVM Gradle et Robolectric vérifiées,
HOME/XDG/tmp/exchange/bases privés par run et aucun transport réel par défaut.
Son test d'environnement porte la suite Android à 203 tests (199 réussis,
4 ignorés, 0 échec) sans changer les contrats de synchronisation.

**[CURRENT EVIDENCE]** `TRAINLOG_SYNC_CHARACTERIZATION_V1=PASS/FROZEN` adds a
real Android-exporter → desktop-importer/exporter → fresh-Android-importer V3
round trip, plus exact current-behavior assertions for identity and ordering,
identical replay versus correction, cross-artifact partial failure,
delete-before-send publication failure, request/receipt marking, and contention
on the real private-XDG lock. The assertion-level matrix is owned by
`docs/tests.md`. This evidence does not implement V4, draft transport, general
tombstones, coherent generations, causal merge, or a peer-consumption
acknowledgement.

**[CURRENT EVIDENCE]** `TRAINLOG_SYNC_DATA_LIFECYCLE_V1=PASS/FROZEN` now has
real cross-implementation round trips for enriched mobile V4 history and the
separate execution-draft V1 artifact in both producer directions. The exact
entry points, assertions, commands, compiler comparison, test inventory, and
remaining transport boundary are recorded in `docs/tests.md`. This evidence
does not select V4 transport and does not implement causal deletion,
generation manifests, peer-consumption acknowledgement, or orchestration.

## 16. Sources propriétaires

- état/roadmap : `docs/current_state.md`, `docs/roadmap.md` ;
- architecture/run : `docs/architecture.md`, `docs/sync_exchange.md` ;
- formats : `docs/exchange_format.md`, `tools/import_mobile_export.py`,
  `tools/export_pc_mobile.py` ;
- schémas : `docs/database.md`, `tui/src/database.c`,
  `android/.../TrainlogRepository.kt` ;
- feedback : `docs/training_feedback.md` ;
- tests : `docs/tests.md`, `tests/test_session_exchange_v3.py`,
  `tests/test_sync_gap_characterization.py`,
  `tests/support/roundtrip_desktop_v3.py`, Android repository tests and
  `tui/tests/test_sync_body_zone_wiring.c`.
