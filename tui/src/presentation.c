/**
 * @file presentation.c
 * @brief Locale-independent French/English TUI presentation catalog.
 */

#include "trainlog/presentation.h"

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct TrainlogTranslation {
    const char *key;
    const char *fr;
    const char *en;
} TrainlogTranslation;

static TrainlogPresentationLanguage current_language =
    TRAINLOG_PRESENTATION_FRENCH;

static bool presentation_float_conversion(char conversion)
{
    return strchr("aAeEfFgG", conversion) != NULL;
}

static size_t presentation_conversion_end(const char *format, size_t start,
    bool *is_float)
{
    size_t index = start + 1U;

    *is_float = false;
    if (format[index] == '%') return index + 1U;
    while (format[index] != '\0' &&
           strchr("diouxXfFeEgGaAcspn", format[index]) == NULL)
        ++index;
    if (format[index] == '\0') return index;
    *is_float = presentation_float_conversion(format[index]);
    return index + 1U;
}

static int presentation_c_vsnprintf(char *output, size_t capacity,
    const char *format, va_list arguments)
    __attribute__((format(printf, 3, 0)));

static int presentation_c_vsnprintf(char *output, size_t capacity,
    const char *format, va_list arguments)
{
    locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    locale_t previous = (locale_t)0;
    int result;
    if (c_locale != (locale_t)0) previous = uselocale(c_locale);
    result = vsnprintf(output, capacity, format, arguments);
    if (c_locale != (locale_t)0) {
        (void)uselocale(previous);
        freelocale(c_locale);
    }
    return result;
}

static int presentation_vsnprintf(char *output, size_t capacity,
    const char *format, va_list arguments)
    __attribute__((format(printf, 3, 0)));

static int presentation_vsnprintf(char *output, size_t capacity,
    const char *format, va_list arguments)
{
    int result;
    va_list copy;
    /* WHY: setlocale is required for terminal text handling, but numeric
     * presentation is an application-language decision.
     * CONTRACT: formatting never changes the process locale or stored/input
     * values; English uses '.', French uses ',' even under a contrary host.
     * INVARIANT: every translated renderer crosses this one boundary. */
    va_copy(copy, arguments);
    result = presentation_c_vsnprintf(NULL, 0U, format, copy);
    va_end(copy);
    if (result < 0 || output == NULL || capacity == 0U) return result;
    if (current_language != TRAINLOG_PRESENTATION_FRENCH) {
        va_copy(copy, arguments);
        (void)presentation_c_vsnprintf(output, capacity, format, copy);
        va_end(copy);
        return result;
    }
    {
        char *raw = malloc((size_t)result + 1U);
        char *marked_format;
        char *marked;
        size_t format_length = strlen(format);
        size_t float_count = 0U;
        size_t index;
        unsigned char begin = 1U, end = 2U;
        size_t out = 0U;
        bool in_float = false;
        if (raw == NULL) return -1;
        va_copy(copy, arguments);
        (void)presentation_c_vsnprintf(raw, (size_t)result + 1U, format, copy);
        va_end(copy);
        for (; begin < 32U; ++begin) {
            if (memchr(raw, (int)begin, (size_t)result) == NULL) break;
        }
        for (; end < 32U; ++end) {
            if (end != begin && memchr(raw, (int)end, (size_t)result) == NULL) break;
        }
        if (begin == 32U || end == 32U) {
            /* All control bytes are user data: retain exact bytes rather than
             * risking a broad decimal rewrite. */
            (void)snprintf(output, capacity, "%s", raw);
            free(raw); return result;
        }
        for (index = 0U; index < format_length;) {
            bool is_float = false;
            if (format[index] != '%') { ++index; continue; }
            index = presentation_conversion_end(format, index, &is_float);
            if (is_float) ++float_count;
        }
        if (float_count == 0U) { (void)snprintf(output, capacity, "%s", raw); free(raw); return result; }
        marked_format = malloc(format_length + float_count * 2U + 1U);
        if (marked_format == NULL) { free(raw); return -1; }
        {
            size_t source = 0U, target = 0U;
            while (format[source] != '\0') {
                size_t directive_end;
                bool is_float = false;
                if (format[source] != '%') {
                    marked_format[target++] = format[source++];
                    continue;
                }
                directive_end = presentation_conversion_end(format, source,
                    &is_float);
                if (is_float) marked_format[target++] = (char)begin;
                while (source < directive_end)
                    marked_format[target++] = format[source++];
                if (is_float) marked_format[target++] = (char)end;
            }
            marked_format[target] = '\0';
        }
        va_copy(copy, arguments);
        marked = malloc((size_t)result + float_count * 2U + 1U);
        if (marked == NULL) { va_end(copy); free(marked_format); free(raw); return -1; }
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
        (void)presentation_c_vsnprintf(marked, (size_t)result + float_count * 2U + 1U, marked_format, copy);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        va_end(copy); free(marked_format); free(raw);
        for (index = 0U; marked[index] != '\0' && out + 1U < capacity; ++index) {
            if ((unsigned char)marked[index] == begin) { in_float = true; continue; }
            if ((unsigned char)marked[index] == end) { in_float = false; continue; }
            output[out++] = in_float && marked[index] == '.' ? ',' : marked[index];
        }
        output[out] = '\0'; free(marked);
    }
    return result;
}

/* CONTRACT: keys are serialization-stable presentation identifiers. French is
 * the product default; English strings are embedded and never depend on an
 * installed OS locale or gettext catalog. */
static const TrainlogTranslation translations[] = {
    {"nav.home", "Accueil", "Home"},
    {"nav.sessions", "Séances", "Sessions"},
    {"nav.exercises", "Exercices", "Exercises"},
    {"nav.statistics", "Statistiques", "Statistics"},
    {"nav.sync", "Synchronisation", "Synchronization"},
    {"nav.settings", "Paramètres", "Settings"},
    {"route.session.current", "Séances / Séance en cours",
        "Sessions / Current session"},
    {"route.session.generator", "Séances / Programmer",
        "Sessions / Plan"},
    {"route.session.manual", "Séances / Nouvelle séance",
        "Sessions / New session"},
    {"route.session.completed", "Séances / Effectuées",
        "Sessions / Completed"},
    {"route.session.detail", "Séances / Détail", "Sessions / Detail"},
    {"route.exercise.catalog", "Exercices / Catalogue",
        "Exercises / Catalog"},
    {"route.exercise.detail", "Exercices / Fiche", "Exercises / Details"},
    {"route.exercise.knowledge", "Exercices / Connaissances",
        "Exercises / Knowledge"},
    {"route.exercise.performance", "Statistiques / Performance",
        "Statistics / Performance"},
    {"route.exercise.max", "Statistiques / MAX mesuré",
        "Statistics / Measured MAX"},
    {"route.equipment.catalog", "Équipements / Catalogue",
        "Equipment / Catalog"},
    {"route.equipment.detail", "Équipements / Fiche", "Equipment / Details"},
    {"route.stats.training", "Statistiques / Entraînement",
        "Statistics / Training"},
    {"route.stats.exercise", "Statistiques / Par exercice",
        "Statistics / By exercise"},
    {"route.stats.exercise.detail", "Statistiques / Exercice",
        "Statistics / Exercise"},
    {"route.stats.zones", "Statistiques / Zones", "Statistics / Zones"},
    {"route.body", "Statistiques / Mensurations",
        "Statistics / Body measurements"},
    {"route.body.detail", "Mensurations / Relevé",
        "Body measurements / Observation"},
    {"route.body.metric", "Mensurations / Historique",
        "Body measurements / History"},
    {"route.body.trends", "Mensurations / 12 mois",
        "Body measurements / 12 months"},
    {"route.body.global", "Mensurations / Vue globale",
        "Body measurements / Overview"},
    {"route.body.analytics", "Mensurations / Analyse",
        "Body measurements / Analysis"},
    {"route.max", "Statistiques / Capacités MAX",
        "Statistics / MAX capacities"},
    {"settings.language", "Langue", "Language"},
    {"settings.language.help", "Entrée pour changer de langue.",
        "Press Enter to change language."},
    {"settings.language.save_failed",
        "Échec de l’enregistrement de la langue ; choix précédent conservé.",
        "Could not save language; previous choice kept."},
    {"settings.language.durability_uncertain",
        "Langue enregistrée, mais sa durabilité après incident est incertaine.",
        "Language saved, but crash durability is uncertain."},
    {"settings.body_profile", "Profil d’estimation corporelle",
        "Body-estimation profile"},
    {"settings.body_profile.help",
        "Entrée pour consulter ou modifier le profil.",
        "Press Enter to view or edit the profile."},
    {"language.fr", "Français", "Français"},
    {"language.en", "English", "English"},
    {"action.open", "Entrée Ouvrir", "Enter Open"},
    {"action.back", "Échap Retour", "Esc Back"},
    {"action.actions", "F7 Actions", "F7 Actions"},
    {"action.navigation", "F6 Navigation", "F6 Navigation"},
    {"action.help", "? Aide", "? Help"},
    {"empty.no_data", "Aucune donnée disponible.", "No data available."},
    {"chart.timeline", "ÉVOLUTION DANS LE TEMPS", "CHANGE OVER TIME"},
    {"chart.totals", "TOTAUX PAR PÉRIODE", "TOTALS BY PERIOD"},
    {"shell.too_small", "Terminal trop petit", "Terminal too small"},
    {"shell.minimum", "Minimum : 72×20", "Minimum: 72×20"},
    {"sync.confirm.both",
        "Synchroniser Android et PC dans les deux sens maintenant ?",
        "Synchronize Android and PC in both directions now?"},
    {"sync.confirm.import", "Importer Android vers le PC maintenant ?",
        "Import from Android to PC now?"},
    {"sync.confirm.publish",
        "Publier le catalogue du PC vers Android maintenant ?",
        "Publish the PC catalog to Android now?"},
    {"sync.footer.wide",
        "s Synchroniser maintenant (PC↔Android)  r Actualiser appareil  Échap retour",
        "s Synchronize now (PC↔Android)  r Refresh device  Esc back"},
    {"sync.footer.compact",
        "s Synchroniser PC↔Android  r Actualiser  Échap retour",
        "s Synchronize PC↔Android  r Refresh  Esc back"},
    {"msg.body.prepare", "Impossible de préparer un identifiant ou une date de relevé.", "Unable to prepare an observation ID or date."},
    {"msg.body.positive", "Enregistrement impossible : au moins une mesure positive est requise.", "Unable to save: at least one positive measurement is required."},
    {"msg.body.corrected", "Relevé corrigé; identité, date et séance conservées.", "Observation corrected; identity, date and session preserved."},
    {"msg.body.saved", "Relevé corporel enregistré.", "Body observation saved."},
    {"msg.positive.invalid", "Nombre positif invalide; le texte saisi est conservé.", "Invalid positive number; entered text is preserved."},
    {"msg.profile.formula", "Formule attendue : 1 homme ou 2 femme.", "Expected formula: 1 male or 2 female."},
    {"msg.profile.height", "Taille attendue entre 100 et 250 cm; texte conservé.", "Expected height between 100 and 250 cm; text preserved."},
    {"msg.profile.save.fail", "Impossible d’enregistrer le profil local; champs conservés.", "Unable to save the local profile; fields preserved."},
    {"msg.profile.saved", "Profil d’estimation corporelle enregistré localement.", "Body-estimation profile saved locally."},
    {"msg.catalog.read", "Impossible de lire le catalogue d’exercices.", "Unable to read the exercise catalog."},
    {"msg.catalog.empty", "Ajoutez d’abord au moins un exercice au catalogue.", "First add at least one exercise to the catalog."},
    {"msg.session.empty", "Ajoutez au moins un exercice avant d’enregistrer.", "Add at least one exercise before saving."},
    {"msg.actual.required", "Saisissez un résultat réel pour %.120s.", "Enter an actual result for %.120s."},
    {"msg.generation.rejected", "Génération refusée (%d).", "Generation rejected (%d)."},
    {"msg.generation.empty", "Pas assez d’exercices résolus et compatibles; rien n’a été inventé.", "Not enough resolved compatible exercises; nothing was invented."},
    {"msg.session.exists.preview", "Une séance est déjà en cours. Reprenez-la ou gardez cet aperçu.", "A session is already in progress. Resume it or keep this preview."},
    {"msg.equipment.conflict", "Conflit de définition; les champs sont conservés.", "Definition conflict; fields are preserved."},
    {"msg.equipment.save.fail", "Équipement non enregistré; tous les champs sont conservés.", "Equipment not saved; all fields are preserved."},
    {"msg.equipment.invalid", "Équipement non créé : définition invalide ou conflit.", "Equipment not created: invalid or conflicting definition."},
    {"msg.equipment.created", "Équipement personnel créé : %.120s", "Custom equipment created: %.120s"},
    {"msg.field.required", "Ce champ est obligatoire; la saisie est conservée.", "This field is required; entered text is preserved."},
    {"msg.zones.read", "Impossible de lire les zones actuelles.", "Unable to read current zones."},
    {"msg.merge.targets", "Impossible de charger les cibles de fusion.", "Unable to load merge targets."},
    {"msg.zone.required", "Une zone principale est requise pour un exercice par séries.", "A primary zone is required for a set-based exercise."},
    {"msg.exercise.conflict", "Conflit de nom ou de profil; tous les champs sont conservés.", "Name or profile conflict; all fields are preserved."},
    {"msg.exercise.save.fail", "Exercice non enregistré; tous les champs sont conservés.", "Exercise not saved; all fields are preserved."},
    {"msg.exercise.reload", "Exercice créé, mais le sélecteur n’a pas pu être rechargé.", "Exercise created, but the picker could not be reloaded."},
    {"msg.exercise.result", "Exercice %s : %.120s", "Exercise %s: %.120s"},
    {"msg.exercise.edited", "modifié", "edited"},
    {"msg.exercise.created", "créé", "created"},
    {"msg.name.required", "Le nom est obligatoire; la saisie est conservée.", "The name is required; entered text is preserved."},
    {"msg.route.active.field", "Terminez ou annulez le champ actif avant de changer de rubrique.", "Finish or cancel the active field before changing section."},
    {"msg.merge.preview.fail", "Prévisualisation impossible; aucune fusion effectuée.", "Unable to preview; no merge performed."},
    {"msg.merge.id.bound", "Fusion terminée; identifiant UI hors borne.", "Merge completed; UI identifier out of bounds."},
    {"msg.merge.completed", "Fusion terminée. Cible sélectionnée : %.112s", "Merge completed. Selected target: %.112s"},
    {"msg.merge.conflict", "Fusion refusée : profil ou zone principale incompatible. Aucun changement.", "Merge rejected: incompatible profile or primary zone. No change."},
    {"msg.merge.fail", "Fusion impossible. Aucun changement confirmé.", "Merge failed. No change confirmed."},
    {"msg.action.active.field", "Terminez ou annulez le champ actif avant une autre action.", "Finish or cancel the active field before another action."},
    {"msg.equipment.display.required", "Le nom convivial est requis et doit tenir dans le champ.", "The display name is required and must fit in the field."},
    {"msg.equipment.label.long", "Le nom d’étiquette est trop long.", "The label name is too long."},
    {"msg.equipment.type.required", "Le type d’équipement est requis.", "Equipment type is required."},
    {"msg.equipment.create.fail", "Équipement non créé : nom ou identifiant invalide/conflit.", "Equipment not created: invalid/conflicting name or identifier."},
    {"msg.dose.rejected", "Dose refusée; la proposition précédente est conservée.", "Dose rejected; the previous proposal is preserved."},
    {"msg.max.unavailable", "MAX compatible indisponible : exercice et équipement externe exacts requis.", "Compatible MAX unavailable: exact exercise and external equipment required."},
    {"msg.value.invalid", "Valeur invalide; saisissez un nombre positif.", "Invalid value; enter a positive number."},
    {"msg.percent.unavailable", "%%MAX indisponible : choisissez une résistance externe compatible.", "%%MAX unavailable: choose compatible external resistance."},
    {"msg.field.not.applicable", "Ce champ ne s’applique pas au profil de cet exercice.", "This field does not apply to this exercise profile."},
    {"msg.draft.save.fail", "Échec de l’enregistrement; le brouillon est conservé.", "Save failed; the draft is preserved."},
    {"msg.session.exists", "Une séance est déjà en cours; elle n’a pas été remplacée.", "A session is already in progress; it was not replaced."},
    {"sync.history.legacy", "Synchronisation historique PC↔Android\nHorodatage : %s\nÉtat : %s\nDétail hérité masqué (entrée historique)", "Historical PC↔Android synchronization\nTimestamp: %s\nStatus: %s\nLegacy detail hidden (historical entry)"},
    {"sync.history.entry", "Synchronisation PC↔Android\nHorodatage : %s\nÉtat : %s\nDétail diagnostique conservé hors présentation", "PC↔Android synchronization\nTimestamp: %s\nStatus: %s\nDiagnostic detail retained outside presentation"},
    {"sync.state.success", "réussie", "successful"},
    {"sync.state.failure", "échouée", "failed"},
    {"sync.result.success.counts", "Synchronisation réussie · %s · exercices +%zu, réconciliés %zu, présents %zu · séances +%zu, présentes %zu · relevés +%zu, présents %zu · équipements +%zu, présents %zu · catalogue publié %zu", "Synchronization successful · %s · exercises +%zu, reconciled %zu, present %zu · sessions +%zu, present %zu · observations +%zu, present %zu · equipment +%zu, present %zu · catalog published %zu"},
    {"sync.result.failure.conflict", "Synchronisation refusée : une autre exécution est active.", "Synchronization rejected: another run is active."},
    {"sync.result.failure.not_found", "Synchronisation impossible : aucun appareil MTP Trainlog détecté.", "Synchronization unavailable: no Trainlog MTP device detected."},
    {"sync.result.failure.invalid", "Synchronisation refusée : données ou demande invalides.", "Synchronization rejected: invalid data or request."},
    {"sync.result.failure.database", "Synchronisation interrompue : erreur de base de données locale.", "Synchronization stopped: local database error."},
    {"sync.result.failure.version", "Synchronisation refusée : version de données non prise en charge.", "Synchronization rejected: unsupported data version."},
    {"sync.result.failure.system", "Synchronisation interrompue : erreur d’accès appareil ou fichier.", "Synchronization stopped: device or file access error."},
    {"sync.result.failure.no_report", "Synchronisation interrompue sans rapport de réussite.", "Synchronization stopped without a success report."},
    {"summary.performance.none", "aucune série réussie", "no successful set"},
    {"summary.assisted.duration", "%.1f kg aide × %s", "%.1f kg assistance × %s"},
    {"summary.assisted.reps", "%.1f kg aide × %d reps", "%.1f kg assistance × %d reps"},
    {"summary.continuous", "Continu · %s", "Continuous · %s"},
    {"summary.plan.weight", "Plan %d×%d · %.2f kg · repos %d s · 0 réalisée", "Plan %d×%d · %.2f kg · rest %d s · 0 completed"},
    {"summary.plan.no_load", "Plan %d×%d · charge absente · repos %d s · 0 réalisée", "Plan %d×%d · no load · rest %d s · 0 completed"},
    {"summary.sets.prefix", "%zu séries · ", "%zu sets · "},
    {"summary.assistance.suffix", " aide", " assistance"},
    {"summary.waist.value", "taille %.1f", "waist %.1f"},
    {"summary.waist.none", "taille —", "waist —"},
    {"summary.chest.value", "poitrine %.1f", "chest %.1f"},
    {"summary.chest.none", "poitrine —", "chest —"},
    {"summary.body", "%-9s  %-9s  %-14s  %2zu valeur(s)", "%-9s  %-9s  %-14s  %2zu value(s)"},
    {"chart.body.metric", "ÉVOLUTION DANS LE TEMPS · ←/→ %s", "CHANGE OVER TIME · ←/→ %s"},
    {"chart.statistics", "←/→ %s · %s · %zu/5", "←/→ %s · %s · %zu/5"},
    {"chart.exercise", "←/→ %s · %d/5", "←/→ %s · %d/5"},
    {"list.exercise.count", "%zu exercice(s)", "%zu exercise(s)"},
    {"list.occurrences", "%zu fois · %s", "%zu times · %s"},
    {"value.not_applicable", "non applicable", "not applicable"},
    {"goal.general", "Général", "General"},
    {"goal.strength", "Force", "Strength"},
    {"goal.hypertrophy", "Hypertrophie", "Hypertrophy"},
    {"goal.endurance", "Endurance", "Endurance"},
    {"dashboard.current", "actuel", "current"},
    {"label.rest.seconds", "Repos (s)", "Rest (s)"},
    {"label.duration", "durée", "duration"},
    {"label.training.upper", "[ENTRAINEMENT]", "[TRAINING]"},
    {"label.confidence.high", "élevée", "high"},
    {"label.confidence.moderate", "modérée", "moderate"},
    {"label.confidence.uncertain", "incertaine", "uncertain"},
    {"ui.target.load.suffix", " · charge/assistance cible définie", " · target load/assistance defined"},
    {"ui.training.choice", "%c Entraînement", "%c Training"},
    {"ui.quit.lose", "%c Quitter et perdre cet état", "%c Quit and lose this state"},
    {"ui.frequency.heading", "%s Fréquence", "%s Frequency"},
    {"ui.catalog.distribution", "%s Répartition du catalogue", "%s Catalog distribution"},
    {"ui.one.reading", "%s · %s %s · 1 relevé", "%s · %s %s · 1 reading"},
    {"ui.one.reading.no.trend", "%s · %s %s · 1 relevé, tendance indisponible", "%s · %s %s · 1 reading, trend unavailable"},
    {"ui.current.sessions", "%s · actuel %zu séance%s · MAX %zu", "%s · current %zu session%s · MAX %zu"},
    {"ui.sessions.exercises", "%s · séances %zu · exercices distincts %zu", "%s · sessions %zu · distinct exercises %zu"},
    {"ui.24h.zones", "24 h : %zu séries principales, %zu secondaires (%zu séances)", "24 h: %zu primary sets, %zu secondary (%zu sessions)"},
    {"ui.72h.zones", "72 h : %zu séries principales, %zu secondaires (%zu séances)", "72 h: %zu primary sets, %zu secondary (%zu sessions)"},
    {"ui.knowledge.relation", "> %.38s · confiance %s · sources vérifiées", "> %.38s · confidence %s · verified sources"},
    {"ui.selected", "> [SÉLECTION]", "> [SELECTED]"},
    {"ui.discard.definition", "Abandonner cette définition non enregistrée ?", "Discard this unsaved definition?"},
    {"ui.discard.session", "Abandonner la séance en cours ?", "Discard the current session?"},
    {"ui.discard.changes", "Abandonner les modifications non enregistrées ?", "Discard unsaved changes?"},
    {"ui.continuous.no.fake", "Activité continue : %d s · aucune série fictive", "Continuous activity: %d s · no fake set"},
    {"ui.assistance.measured", "Assistance mesurée (kg) — moins = mieux", "Measured assistance (kg) — lower is better"},
    {"ui.associations", "Associations : %zu équipement(s), %zu zone(s).", "Associations: %zu equipment item(s), %zu zone(s)."},
    {"ui.asym.forearms", "Asymétrie avant-bras", "Forearm asymmetry"},
    {"ui.asym.arms", "Asymétrie bras", "Arm asymmetry"},
    {"ui.asym.thighs", "Asymétrie cuisses", "Thigh asymmetry"},
    {"ui.asym.calves", "Asymétrie mollets", "Calf asymmetry"},
    {"ui.session.type.choose", "Choisissez le type de séance :", "Choose the session type:"},
    {"ui.equipment.compatible", "Compatibles (manifeste) : %zu · utilisés historiquement : %zu", "Compatible (manifest): %zu · used historically: %zu"},
    {"ui.knowledge.validated", "Connaissances validées", "Validated knowledge"},
    {"ui.equipment.create.return", "Créer un équipement personnel — retour à l’occurrence", "Create custom equipment — return to occurrence"},
    {"ui.equipment.create", "Créer un équipement personnel", "Create custom equipment"},
    {"ui.create", "Créer", "Create"},
    {"ui.last.days", "Dernière : J-%zu", "Latest: D-%zu"},
    {"ui.relative.difference", "Différence relative : %+.1f%%", "Relative difference: %+.1f%%"},
    {"ui.started", "Début : %s", "Started: %s"},
    {"ui.detail", "Détail %s", "%s details"},
    {"ui.estimate.notice", "Estimation anthropométrique : tendance, pas mesure directe.", "Anthropometric estimate: trend, not a direct measurement."},
    {"ui.estimated.fat", "Graisse estimée", "Estimated fat"},
    {"ui.id.preserved", "Identifiant stable conservé : %s", "Stable identifier preserved: %s"},
    {"ui.feedback.count", "Immédiats %zu · J+1 séance %zu", "Immediate %zu · next-day session %zu"},
    {"ui.merge.impact", "Impact source : %zu occurrence(s), %zu série(s), %zu continu, %zu MAX.", "Source impact: %zu occurrence(s), %zu set(s), %zu continuous, %zu MAX."},
    {"ui.session.load.fail", "Impossible de charger la séance.", "Unable to load the session."},
    {"ui.interpretation.conditional", "Interprétation conditionnelle — à confirmer", "Conditional interpretation — confirm"},
    {"ui.interpretation.unresolved", "Interprétation scientifique non résolue.", "Scientific interpretation unresolved."},
    {"ui.old.sessions.unchanged", "Les anciennes séances resteront inchangées.", "Existing sessions will remain unchanged."},
    {"ui.series.partial", "Limite locale atteinte : certaines séries peuvent être partielles.", "Local limit reached: some series may be partial."},
    {"ui.stats.max.feedback", "MAX %zu · ressentis immédiats %zu · prévu/réalisé séries %llu/%llu", "MAX %zu · immediate feedback %zu · planned/actual sets %llu/%llu"},
    {"ui.max.measured.kg", "MAX mesuré (kg)", "Measured MAX (kg)"},
    {"ui.max.actual", "MAX réellement mesuré : %.2f kg", "Actually measured MAX: %.2f kg"},
    {"ui.fat.mass", "Masse grasse estimée", "Estimated fat mass"},
    {"ui.lean.mass", "Masse maigre estimée", "Estimated lean mass"},
    {"ui.best.set", "Meilleur set enregistré : %s", "Best recorded set: %s"},
    {"ui.best.duration", "Meilleure durée", "Best duration"},
    {"ui.best.repetitions", "Meilleures répétitions", "Best repetitions"},
    {"ui.label.optional", "Nom d’étiquette (optionnel)", "Label name (optional)"},
    {"ui.label.optional.short", "Nom étiquette (optionnel)", "Label name (optional)"},
    {"ui.unclassified", "Non classés", "Unclassified"},
    {"ui.not.entered", "Non renseignée", "Not entered"},
    {"ui.new.observation", "Nouveau relevé", "New observation"},
    {"ui.plan.heading", "Objectif prévu — séparé des valeurs réalisées", "Planned target — separate from actual values"},
    {"ui.stats.occurrences", "Occurrences %zu · séances %zu · séries %zu · répétitions %llu", "Occurrences %zu · sessions %zu · sets %zu · repetitions %llu"},
    {"ui.stats.linked", "Occurrences liées %zu · séries %llu/%llu", "Linked occurrences %zu · sets %llu/%llu"},
    {"ui.body.current", "PROFIL ACTUEL · relevé %zu/%zu · %s", "CURRENT PROFILE · observation %zu/%zu · %s"},
    {"ui.body.circumferences", "PROFIL CIRCONFÉRENCES · échelle au plus grand cm présent", "CIRCUMFERENCE PROFILE · scale to largest present cm"},
    {"ui.body.page", "Page %zu/2 · relevé %s", "Page %zu/2 · observation %s"},
    {"ui.plan.summary", "Plan : %d série(s) · repos %d s%s", "Plan: %d set(s) · rest %d s%s"},
    {"ui.real.points", "Points réels du contexte :", "Actual context points:"},
    {"ui.range.frequency", "Première %s · dernière %s · fréquence 7j %zu · 30j %zu", "First %s · latest %s · frequency 7d %zu · 30d %zu"},
    {"ui.proposal.kept", "Proposition non acceptée conservée en mémoire.", "Unaccepted proposal kept in memory."},
    {"ui.previous.difference", "Précédent %.1f kg · différence %+.1f kg%s", "Previous %.1f kg · difference %+.1f kg%s"},
    {"ui.planned.actual.same", "Prévu/réalisé mêmes occurrences : %zu · séries %llu/%llu · reps %llu/%llu", "Planned/actual same occurrences: %zu · sets %llu/%llu · reps %llu/%llu"},
    {"ui.period.selector", "Période  %s  %s  %s  %s  %s", "Period  %s  %s  %s  %s  %s"},
    {"ui.period.selector3", "Période : %s · 1 7j · 2 30j · 3 Tout", "Period: %s · 1 7d · 2 30d · 3 All"},
    {"ui.feedback.summary", "Ressentis : immédiats %zu · J+1 séance %zu · texte libre non analysé", "Feedback: immediate %zu · next-day session %zu · free text not analyzed"},
    {"ui.feedback.frequent", "Ressentis immédiats %zu · plus fréquent %s (%zu) · J+1 séance %zu · texte libre non analysé", "Immediate feedback %zu · most frequent %s (%zu) · next-day session %zu · free text not analyzed"},
    {"ui.remove.occurrence", "Retirer cette occurrence de la séance ?", "Remove this occurrence from the session?"},
    {"ui.actual.sets.count", "Réalisé : %zu série(s)", "Actual: %zu set(s)"},
    {"ui.actual.continuous", "Réalisé : activité continue · %s", "Actual: continuous activity · %s"},
    {"ui.followup", "SUIVI APRÈS SÉANCE", "POST-SESSION FOLLOW-UP"},
    {"ui.sync.busy.service", "Service de synchronisation occupé.", "Synchronization service busy."},
    {"ui.max.tests", "Tests enregistrés : %zu · dernier %.1f kg · meilleur %.1f kg", "Recorded tests: %zu · latest %.1f kg · best %.1f kg"},
    {"ui.free.text", "Texte libre non analysé", "Free text not analyzed"},
    {"ui.period.all", "Tout · 12 périodes max.", "All · max 12 periods."},
    {"ui.sync.one.run", "Une seule exécution peut être active.", "Only one run can be active."},
    {"ui.fat.change", "Variation graisse estimée %+.2f point(s)", "Estimated fat change %+.2f point(s)"},
    {"ui.volume.activity", "Volume %.1f kg · activité %s", "Volume %.1f kg · activity %s"},
    {"ui.volume.na", "Volume chargé non applicable à ce profil.", "Loaded volume not applicable to this profile."},
    {"ui.volume.valid", "Volume chargé valide : %.1f kg", "Valid loaded volume: %.1f kg"},
    {"ui.history.bounded", "Vue bornée : historique récent partiel.", "Bounded view: recent history is partial."},
    {"ui.create.action", "a Créer", "a Create"},
    {"ui.max.filter", "a MAX mesurés/tous", "a Measured/all MAX"},
    {"ui.auto.load", "charge automatique observée", "observed automatic load"},
    {"ui.continuous.actions", "e durée · s vitesse · k distance", "e duration · s speed · k distance"},
    {"ui.historical.entry", "entrée historique", "historical entry"},
    {"ui.finish.edit", "f Terminer édition", "f Finish editing"},
    {"ui.generate", "g Générer", "g Generate"},
    {"ui.regenerate", "g Régénérer", "g Regenerate"},
    {"ui.equipment.action", "i Équipement", "i Equipment"},
    {"ui.movement.unclassified", "mouvement non classé", "unclassified movement"},
    {"ui.exercise.create.action", "n Créer un exercice", "n Create exercise"},
    {"ui.not.configured", "non configuré", "not configured"},
    {"ui.plan.action", "p Objectif prévu", "p Planned target"},
    {"ui.repetitions.lower", "répétitions", "repetitions"},
    {"ui.no.equipment.lower", "sans équipement", "no equipment"},
    {"ui.session.open", "séance ouverte", "open session"},
    {"ui.sessions.lower", "séances", "sessions"},
    {"ui.sets.lower", "séries", "sets"},
    {"ui.generator.actions", "z zone · o objectif · t durée · g générer", "z zone · o goal · t duration · g generate"},
    {"ui.body.no.metric", "ÉVOLUTION DANS LE TEMPS · aucune métrique disponible", "CHANGE OVER TIME · no metric available"},
    {"ui.warmup.notice", "Échauffement et retour au calme absents en V1.", "Warm-up and cool-down absent in V1."},
    {"ui.shoulders.waist", "Épaules / taille", "Shoulders / waist"},
    {"ui.occurrence.equipment", "Équipement : %s  (i pour la fiche)", "Equipment: %s  (i for details)"},
    {"ui.occurrence.equipment.heading", "Équipement de l’occurrence — ID stable de l’occurrence inchangé", "Occurrence equipment — stable occurrence ID unchanged"},
    {"ui.device.not.refreshed", "État appareil non actualisé · r pour rechercher.", "Device status not refreshed · r to search."},
    {"ui.label", "Étiquette : %s", "Label: %s"},
    {"ui.to.enter", "à saisir · ", "to enter · "},
    {"ui.more.circumferences", "… %zu autre(s) circonférence(s) · ↑↓ change le relevé", "… %zu other circumference(s) · ↑↓ changes observation"},
    {"ui.previous.graph", "← Graphe précédent", "← Previous chart"},
    {"ui.previous.measure", "← Mesure précédente", "← Previous measurement"},
    {"ui.previous.metric", "← Métrique précédente", "← Previous metric"},
    {"ui.next.metric", "→ Métrique suivante", "→ Next metric"},
    {"ui.percent.available", " · pourcentage calculable", " · percentage available"},
    {"ui.no.yes.discard", "%s Non    %s Oui, abandonner", "%s No    %s Yes, discard"},
    {"ui.no.yes.delete", "%s Non    %s Oui, supprimer", "%s No    %s Yes, delete"},
    {"ui.catalog.zones", "+ %zu zones dans le catalogue", "+ %zu zones in catalog"},
    {"ui.no.zero", "0 Non", "0 No"},
    {"ui.yes.edit", "1 Oui, modifier", "1 Yes, edit"},
    {"ui.date", "Date : %s", "Date: %s"},
    {"ui.latest.date", "Date du dernier : %s", "Latest date: %s"},
    {"ui.name", "Nom : %s", "Name: %s"},
    {"ui.display.name", "Nom convivial", "Display name"},
    {"ui.display.name.alt", "Nom d’affichage", "Display name"},
    {"ui.profile", "Profil : %s", "Profile: %s"},
    {"ui.secondary", "Secondaires : %s", "Secondary: %s"},
    {"ui.secondary.heading", "Secondaires :", "Secondary:"},
    {"ui.sources", "Sources :", "Sources:"},
    {"ui.tracking.organization", "Suivi : %s · organisation : %s", "Tracking: %s · organization: %s"},
    {"ui.tracking", "Suivi : %s", "Tracking: %s"},
    {"ui.all", "Tout", "All"},
    {"ui.type", "Type : %s", "Type: %s"},
    {"ui.type.heading", "Type", "Type"},
    {"ui.all.selected", "[Tout]", "[All]"},
    {"ui.profile.action", "p Profil estimation", "p Estimation profile"},
    {"ui.percent.selected", "pourcentage MAX choisi", "selected MAX percentage"},
    {"ui.merge.action", "u Fusionner avec…", "u Merge with…"},
    {"ui.percent.help", "%% sur la charge : calcul utilisateur 1..100%% du dernier MAX compatible", "%% of load: user calculation 1..100%% of latest compatible MAX"},
    {"ui.compatible.item", "%c %.48s [compatible]", "%c %.48s [compatible]"},
    {"ui.feedback.exercise", "%s · %zu retour%s", "%s · %zu feedback item%s"},
    {"ui.one.point", "%s · 1 point · tendance indisponible", "%s · 1 point · trend unavailable"},
    {"ui.field.position", "%s — champ %zu/14", "%s — field %zu/14"},
    {"ui.choose.exercise", "%s — choisir un exercice", "%s — choose an exercise"},
    {"ui.change.measure", "%s — ←/→ change la mesure", "%s — ←/→ changes measurement"},
    {"ui.zone.periods", "7j principal %zu · secondaire %zu · 30j principal %zu · secondaire %zu", "7d primary %zu · secondary %zu · 30d primary %zu · secondary %zu"},
    {"ui.latest.primary", "Dernier principal : %s", "Latest primary: %s"},
    {"ui.latest.secondary", "Dernier secondaire : %s", "Latest secondary: %s"},
    {"ui.exercise.stats.fail", "Impossible de calculer les statistiques de cet exercice.", "Unable to calculate statistics for this exercise."},
    {"ui.zone.stats.fail", "Impossible de calculer les statistiques par zone.", "Unable to calculate statistics by zone."},
    {"ui.list.load.fail", "Impossible de charger cette liste.", "Unable to load this list."},
    {"ui.history.read.fail", "Impossible de lire cet historique.", "Unable to read this history."},
    {"ui.history.read.fail.alt", "Impossible de lire l’historique.", "Unable to read history."},
    {"ui.history.partial", "Limite locale atteinte : historique partiel.", "Local limit reached: partial history."},
    {"ui.max.compatible.none", "MAX compatible indisponible", "Compatible MAX unavailable"},
    {"ui.distance.measure", "Mesurer la distance : %s · 0 non · 1 oui · ↑↓ choisir", "Measure distance: %s · 0 no · 1 yes · ↑↓ choose"},
    {"ui.speed.measure", "Mesurer la vitesse : %s · 0 non · 1 oui · ↑↓ choisir", "Measure speed: %s · 0 no · 1 yes · ↑↓ choose"},
    {"ui.view.filter", "Vue : %s · a afficher %s", "View: %s · a show %s"},
    {"ui.none.lower", "aucune", "none"},
    {"ui.load.ratio", "charge %.1f/%.1f kg", "load %.1f/%.1f kg"},
    {"ui.continuous.lower", "continue", "continuous"},
    {"ui.body.history.action", "m Historique mesure", "m Measurement history"},
    {"ui.only.max", "seulement les MAX", "MAX only"},
    {"ui.all.compatible", "tous les compatibles", "all compatible"},
    {"ui.all.compatible.exercises", "tous les exercices compatibles", "all compatible exercises"},
    {"ui.blank.clear", "vide conserve · - efface · valeur positive remplace", "blank keeps · - clears · positive value replaces"},
    {"ui.blank.not.measured", "vide signifie mesure non faite · valeur positive ajoute", "blank means not measured · positive value adds"},
    {"ui.no.load.action", "x Sans charge", "x No load"},
    {"ui.external.lower", "externe", "external"},
};

static const TrainlogTranslation source_translations[] = {
    {"feedback.during.session", "Pendant la séance", "During session"},
    /* COMPLETENESS INVARIANT: every active Trainlog-owned French renderer
     * literal/format in the TUI source directory has an exact FR->EN entry here or in
     * translations[]. The deterministic source audit in test_presentation
     * enforces this. Excluded source literals are only stable/internal keys,
     * user or catalog data, protocol values, units, or punctuation glyphs;
     * those must pass through byte-exact. */
    {"audit.set.duration", "%.1f kg × %s", "%.1f kg × %s"},
    {"audit.set.reps", "%.1f kg × %d reps", "%.1f kg × %d reps"},
    {"audit.set.compact", "%s%s×%s%s", "%s%s×%s%s"},
    {"audit.set.weight", "%s%s×%.2fkg%s", "%s%s×%.2fkg%s"},
    {"audit.period.year", "1 an · pas de 31 j", "1 year · not 31 days"},
    {"audit.chart.next", "→ Graphe suivant", "→ Next chart"},
    {"audit.quit.discard", "q Abandonner", "q Discard"},
    {"audit.generator.other.zone", "z Autre zone", "z Other zone"},
    {"audit.generator.discard", "q Abandonner la proposition", "q Discard proposal"},
    {"audit.measure.next", "→ Mesure suivante", "→ Next measurement"},
    {"audit.occurrence.replace", "Remplacer l’occurrence", "Replace occurrence"},
    {"audit.goal", "Objectif : %s", "Goal: %s"},
    {"audit.generator.estimate", "%zu exercice(s) · cible %d min · estimation %d min", "%zu exercise(s) · target %d min · estimate %d min"},
    {"audit.generator.short", "Proposition plus courte : aucun remplissage artificiel.", "Shorter proposal: no artificial filler."},
    {"audit.plan.item", "%d×%d · repos %d s · %.30s", "%d×%d · rest %d s · %.30s"},
    {"audit.plan.load", "%.2f kg · %s", "%.2f kg · %s"},
    {"audit.exercise.article", "%s un exercice", "%s an exercise"},
    {"audit.profile.future", "Les prochaines utilisations suivront le nouveau profil.", "Future uses will follow the new profile."},
    {"audit.assistance", "Assistance (kg) — moins = mieux", "Assistance (kg) — lower is better"},
    {"audit.body.read.fail", "Impossible de lire les mensurations.", "Unable to read body measurements."},
    {"audit.body.discard", "Abandonner toutes les modifications ?", "Discard all changes?"},
    {"audit.profile.discard", "Abandonner les modifications du profil ?", "Discard profile changes?"},
    {"audit.profile.formula.choice", "Formule : 1 homme · 2 femme", "Formula: 1 male · 2 female"},
    {"audit.dashboard.work", "%s · travail %zu · MAX %zu%s", "%s · work %zu · MAX %zu%s"},
    {"audit.dashboard.work.simple", "%s · travail %zu · MAX %zu", "%s · work %zu · MAX %zu"},
    {"audit.body.delta", "%s · %s %s · Δ %c%s", "%s · %s %s · Δ %c%s"},
    {"audit.body.formula.female", "formule femme", "female formula"},
    {"audit.body.formula.male", "formule homme", "male formula"},
    {"audit.waist.trend", "Tour de taille %.1f cm · variation %+.1f cm", "Waist circumference %.1f cm · change %+.1f cm"},
    {"audit.chest.waist", "Poitrine / taille", "Chest / waist"},
    {"audit.sync.history", "Historique des synchronisations", "Synchronization history"},
    {"audit.performance", "Performance — %s", "Performance — %s"},
    {"audit.performance.context", "Contexte : %s · %s · %zu point%s", "Context: %s · %s · %zu point%s"},
    {"audit.max.read.fail", "Impossible de lire les tests de MAX.", "Unable to read MAX tests."},
    {"audit.max.history", "Historique des MAX", "MAX history"},
    {"audit.search", "Recherche : %s%s", "Search: %s%s"},
    {"audit.profile.summary", "%s · %.1f cm", "%s · %.1f cm"},
    {"audit.merge.search", "Recherche cible : %s_", "Search target: %s_"},
    {"audit.merge.source", "Fusionner la source : %.48s", "Merge source: %.48s"},
    {"audit.merge.confirm", "Confirmer la fusion", "Confirm merge"},
    {"audit.merge.source.heading", "SOURCE : %.54s", "SOURCE: %.54s"},
    {"audit.merge.target.heading", "   vers CANONIQUE : %.45s", "   to CANONICAL: %.45s"},
    {"audit.merge.cancel", "%c Annuler", "%c Cancel"},
    {"audit.merge.commit", "%c Fusionner source vers canonique", "%c Merge source into canonical"},
    {"audit.merge.result", "Résultat de la fusion", "Merge result"},
    {"audit.merge.close", "Entrée fermer", "Enter close"},
    {"legacy.home.tagline", "Votre entraînement, au même endroit.",
        "Your training, all in one place."},
    {"legacy.home.session", "1  Reprendre ou commencer une séance",
        "1  Resume or start a session"},
    {"legacy.home.body", "5  Ajouter ou consulter des mensurations",
        "5  Add or view body measurements"},
    {"legacy.sessions.current", "Séance en cours", "Current session"},
    {"legacy.sessions.new", "Nouvelle séance manuelle", "New manual session"},
    {"legacy.sessions.completed", "Séances effectuées", "Completed sessions"},
    {"legacy.selection", "▶ [SÉLECTION]", "▶ [SELECTED]"},
    {"legacy.profile.none", "Profil non configuré.", "Profile not configured."},
    {"legacy.navigation", "Navigation", "Navigation"},
    {"legacy.actions", "Actions", "Actions"},
    {"legacy.help", "Aide", "Help"},
    {"legacy.footer.choose", "↑↓ choisir   Entrée valider",
        "↑↓ choose   Enter confirm"},
    {"legacy.footer.close", "Échap fermer   F7 Actions   ? Aide",
        "Esc close   F7 Actions   ? Help"},
    {"legacy.footer.global", "F6 Navigation   F7 Actions   ? Aide   Échap Retour",
        "F6 Navigation   F7 Actions   ? Help   Esc Back"},
    {"legacy.search", "/ Rechercher", "/ Search"},
    {"legacy.add", "a Ajouter", "a Add"},
    {"legacy.edit", "e Modifier", "e Edit"},
    {"legacy.create", "n Créer", "n Create"},
    {"legacy.save", "Entrée Enregistrer", "Enter Save"},
    {"legacy.next", "Entrée Champ suivant", "Enter Next field"},
    {"legacy.cancel", "Échap Annuler", "Esc Cancel"},
    {"legacy.return", "Entrée Retour", "Enter Back"},
    {"legacy.resume", "0 Reprendre", "0 Resume"},
    {"legacy.discard", "1 Abandonner", "1 Discard"},
    {"legacy.close", "k Fermer", "k Close"},
    {"legacy.refresh", "r Actualiser appareil", "r Refresh device"},
    {"legacy.sync", "s Synchroniser maintenant", "s Synchronize now"},
    {"legacy.position", "Position %zu/%zu", "Position %zu/%zu"},
    {"legacy.position.partial", "Position %zu/%zu+ · limite locale atteinte",
        "Position %zu/%zu+ · local limit reached"},
    {"legacy.partial", "Vue partielle : capacité locale de présentation atteinte.",
        "Partial view: local presentation capacity reached."},
    {"legacy.compact", "Mode compact · F6 ouvre Navigation",
        "Compact mode · F6 opens Navigation"},
    {"legacy.open.full", "Entrée ouvre la vue complète existante dans ce contexte.",
        "Enter opens the full view available in this context."},
    {"legacy.periods", "%zu périodes · %.2f %s", "%zu periods · %.2f %s"},
    {"legacy.readings", "◆ %zu relevés · %.2f %s", "◆ %zu readings · %.2f %s"},
    {"legacy.training", "Entraînement", "Training"},
    {"legacy.max_test", "Test de max", "Max test"},
    {"legacy.supplied", "fourni", "supplied"},
    {"legacy.custom", "personnalisé", "custom"},
    {"legacy.unknown", "inconnu", "unknown"},
    {"legacy.no_load", "sans charge", "no load"},
    {"legacy.external_load", "charge externe", "external load"},
    {"legacy.assistance", "assistance", "assistance"},
    {"legacy.female", "Formule femme", "Female formula"},
    {"legacy.male", "Formule homme", "Male formula"},
    {"legacy.no_data", "Aucune donnée disponible.", "No data available."},
    {"legacy.timeline", "ÉVOLUTION DANS LE TEMPS", "CHANGE OVER TIME"},
    {"legacy.totals", "TOTAUX PAR PÉRIODE", "TOTALS BY PERIOD"},
    {"legacy.quit", "q Quitter", "q Quit"},
    {"legacy.esc.back", "Échap Retour", "Esc Back"},
    {"legacy.profile.title", "Profil d’estimation corporelle",
        "Body-estimation profile"},
    {"legacy.profile.help", "Entrée pour consulter ou modifier le profil.",
        "Press Enter to view or edit the profile."},
    {"legacy.too.small", "Terminal trop petit — %dx%d, minimum 72x20.",
        "Terminal too small — %dx%d, minimum 72x20."},
    {"legacy.quit.small", "q pour quitter", "q to quit"},
    {"legacy.footer.global.format", "F6 Navigation   F7 Actions   ? Aide   %s",
        "F6 Navigation   F7 Actions   ? Help   %s"},
    {"legacy.cancel.keep", "Annuler conserve l’état de cette exécution",
        "Cancel keeps this run's state"},
    {"legacy.help.context", "Aide contextuelle", "Contextual help"},
    {"legacy.help.home", "0/Home Accueil · 1/F1 Séance · 2/F2 Effectuées",
        "0/Home Home · 1/F1 Session · 2/F2 Completed"},
    {"legacy.help.catalogs", "3/F3 Exercices · 4/F4 Équipements · 5/F5 Mensurations",
        "3/F3 Exercises · 4/F4 Equipment · 5/F5 Body measurements"},
    {"legacy.help.sync", "6 Synchronisation · F6 Navigation · F7 Actions",
        "6 Synchronization · F6 Navigation · F7 Actions"},
    {"legacy.help.focus", "Tab change le focus · Échap revient",
        "Tab changes focus · Esc goes back"},
    {"legacy.session.alias", "1 Séance", "1 Session"},
    {"legacy.completed.alias", "2 Effectuées", "2 Completed"},
    {"legacy.exercises.alias", "3 Exercices", "3 Exercises"},
    {"legacy.body.alias", "5 Mensurations", "5 Body measurements"},
    {"legacy.sync.alias", "6 Synchronisation", "6 Synchronization"},
    {"legacy.home.alias", "0 Accueil", "0 Home"},
    {"legacy.enter.continue", "Entrée Continuer", "Enter Continue"},
    {"legacy.esc.discard", "Échap Abandonner…", "Esc Discard…"},
    {"legacy.enter.catalog", "Entrée Catalogue", "Enter Catalog"},
    {"legacy.sets.result", "e Séries/résultat", "e Sets/result"},
    {"legacy.save.action", "f Enregistrer", "f Save"},
    {"legacy.add.set", "a Ajouter série", "a Add set"},
    {"legacy.remove.set", "d Supprimer série", "d Remove set"},
    {"legacy.speed.action", "s Vitesse", "s Speed"},
    {"legacy.distance.action", "k Distance", "k Distance"},
    {"legacy.dose.action", "e Modifier dose", "e Edit dose"},
    {"legacy.load.auto", "u Charge auto", "u Auto load"},
    {"legacy.body.12", "g Vue 12 mois", "g 12-month view"},
    {"legacy.sync.action", "s Synchroniser maintenant · PC↔Android",
        "s Synchronize now · PC↔Android"},
    {"legacy.stats.7", "7 jours · quotidien", "7 days · daily"},
    {"legacy.stats.30", "30 jours · pas de 5 j", "30 days · 5-day steps"},
    {"legacy.stats.90", "90 jours · pas de 15 j", "90 days · 15-day steps"},
    {"legacy.period.week", "semaine courante", "current week"},
    {"legacy.period.month", "mois courant", "current month"},
    {"legacy.no.sessions.period", "Aucune séance dans cette période.",
        "No sessions in this period."},
    {"legacy.stats.summary",
        "Séances %zu   Séries %zu   Exercices pratiqués %zu   MAX %zu",
        "Sessions %zu   Sets %zu   Exercises performed %zu   MAX %zu"},
    {"legacy.stats.read.error", "Certaines données n’ont pas pu être lues.",
        "Some data could not be read."},
    {"legacy.stats.invalid", "Données historiques invalides ignorées.",
        "Invalid historical data ignored."},
    {"legacy.stats.insufficient", "Données insuffisantes.",
        "Insufficient data."},
    {"legacy.no.active.exercises", "Aucun exercice actif à répartir.",
        "No active exercises to distribute."},
    {"legacy.stats.exercises", "Exercices · faits et courbes par identité",
        "Exercises · occurrences and charts by identity"},
    {"legacy.stats.zones", "Zones · travail principal et secondaire",
        "Zones · primary and secondary work"},
    {"legacy.stats.center", "Centre d’analyse · données enregistrées uniquement",
        "Analysis center · recorded data only"},
    {"legacy.no.body", "Aucune mensuration.", "No body measurement."},
    {"legacy.no.body.entered", "Aucune mensuration saisie.",
        "No body measurements entered."},
    {"legacy.no.body.series", "Aucune série corporelle disponible.",
        "No body series available."},
    {"legacy.body.trend", "Évolution corporelle normalisée — 12 mois",
        "Normalized body change — 12 months"},
    {"legacy.month.failure", "Impossible de déterminer le mois courant.",
        "Unable to determine the current month."},
    {"legacy.body.two.months",
        "Premières courbes après 2 mois relevés pour une même mesure.",
        "Charts start after 2 recorded months for the same measurement."},
    {"legacy.body.partial",
        "Limite locale atteinte : vue 12 mois potentiellement partielle.",
        "Local limit reached: 12-month view may be partial."},
    {"legacy.no.body.observation", "Aucun relevé corporel.",
        "No body observation."},
    {"legacy.weight", "Poids", "Weight"},
    {"legacy.height", "Taille", "Height"},
    {"body.neck", "Cou", "Neck"},
    {"body.shoulders", "Épaules", "Shoulders"},
    {"body.chest", "Poitrine", "Chest"},
    {"body.waist", "Tour de taille", "Waist"},
    {"body.hips", "Hanches", "Hips"},
    {"body.left_arm", "Bras gauche", "Left arm"},
    {"body.right_arm", "Bras droit", "Right arm"},
    {"body.left_forearm", "Avant-bras gauche", "Left forearm"},
    {"body.right_forearm", "Avant-bras droit", "Right forearm"},
    {"body.left_thigh", "Cuisse gauche", "Left thigh"},
    {"body.right_thigh", "Cuisse droite", "Right thigh"},
    {"body.left_calf", "Mollet gauche", "Left calf"},
    {"body.right_calf", "Mollet droit", "Right calf"},
    {"legacy.weight.value", "Poids : %.2f kg", "Weight: %.2f kg"},
    {"legacy.weight.none", "Poids : non renseigné", "Weight: not entered"},
    {"legacy.no.circumference", "Aucune circonférence dans ce relevé.",
        "No circumference in this observation."},
    {"legacy.device", "Appareil : %s %s", "Device: %s %s"},
    {"legacy.device.none", "Aucun appareil MTP Trainlog détecté.",
        "No Trainlog MTP device detected."},
    {"legacy.sync.running", "Synchronisation en cours… PC↔Android",
        "Synchronization in progress… PC↔Android"},
    {"legacy.sync.confirm.help", "Entrée confirme une exécution · Échap annule",
        "Enter confirms · Esc cancels"},
    {"legacy.sync.failed", "Synchronisation échouée.", "Synchronization failed."},
    {"legacy.sync.none", "Aucune synchronisation enregistrée.",
        "No synchronization recorded."},
    {"legacy.no.points", "Aucun point exploitable pour ce contexte.",
        "No usable data points for this context."},
    {"legacy.no.body.add", "Aucun relevé corporel. a pour ajouter un relevé.",
        "No body observation. Press a to add one."},
    {"legacy.no.set", "Aucune série réelle. a pour saisir la première.",
        "No actual set. Press a to enter the first."},
    {"legacy.no.occurrence", "Aucune occurrence. a pour choisir un exercice.",
        "No occurrence. Press a to choose an exercise."},
    {"legacy.enter.continue.simple", "Entrée pour continuer", "Enter to continue"},
    {"legacy.enter.back", "Entrée pour revenir", "Enter to go back"},
    {"legacy.discard.resume", "1 abandonner · 0/Échap reprendre",
        "1 discard · 0/Esc resume"},
    {"legacy.actions.available", "Actions disponibles", "Available actions"},
    {"legacy.action.position", "Action %zu/%zu%s%s", "Action %zu/%zu%s%s"},
    {"legacy.merge.none", "Aucune cible correspondante.", "No matching target."},
    {"legacy.merge.preview", "Entrée prévisualiser · Échap annuler",
        "Enter preview · Esc cancel"},
    {"legacy.merge.confirm", "Confirmer la fusion", "Confirm merge"},
    {"legacy.stats.body", "Corps · poids et mensurations",
        "Body · weight and measurements"},
    {"legacy.stats.training", "Entraînement · vue globale et régularité",
        "Training · overview and consistency"},
    {"legacy.stats.max", "MAX · historique des tests enregistrés",
        "MAX · recorded test history"},
    {"legacy.stats.open", "Entrée ouvrir · aucune recommandation ni score global",
        "Enter open · no recommendation or overall score"},
    {"legacy.history.all", "Tout l’historique", "All history"},
    {"legacy.period", "Période", "Period"},
    {"legacy.volume", "Volume chargé", "Loaded volume"},
    {"legacy.sessions", "Séances", "Sessions"},
    {"legacy.sets", "Séries", "Sets"},
    {"legacy.repetitions", "Répétitions", "Repetitions"},
    {"legacy.feedback", "Ressentis immédiats", "Immediate feedback"},
    {"legacy.feedback.unit", "retours", "feedback"},
    {"legacy.calendar.weeks", "semaines calendaires", "calendar weeks"},
    {"legacy.calendar.months", "mois calendaires", "calendar months"},
    {"legacy.history.months", "mois · historique complet",
        "months · full history"},
    {"legacy.history.weeks", "semaines · historique complet",
        "weeks · full history"},
    {"legacy.stats.failure", "Impossible de calculer les statistiques enregistrées.",
        "Unable to calculate recorded statistics."},
    {"legacy.activity", "ACTIVITÉ", "ACTIVITY"},
    {"legacy.work", "TRAVAIL", "WORK"},
    {"legacy.feedback.heading", "RESSENTIS", "FEEDBACK"},
    {"legacy.planned.actual", "PRÉVU / RÉALISÉ", "PLANNED / ACTUAL"},
    {"legacy.observed.load", "Charge observée", "Observed load"},
    {"legacy.max.recorded", "MAX enregistrés", "Recorded MAX"},
    {"legacy.measure", "Mesure", "Measurement"},
    {"ui.insufficient.suffix", " · données insuffisantes", " · insufficient data"},
    {"ui.cancel.choice", "%c Annuler", "%c Cancel"},
    {"ui.no.equipment", "%c Aucun équipement", "%c No equipment"},
    {"ui.body.heading", "%s Mensurations", "%s Body measurements"},
    {"ui.no.yes.edit", "0/Échap/q Non · 1 Oui, modifier", "0/Esc/q No · 1 Yes, edit"},
    {"ui.add.occurrence", "Ajouter une occurrence", "Add an occurrence"},
    {"ui.no.max", "Aucun MAX enregistré.", "No recorded MAX."},
    {"ui.no.session.exercise", "Aucun exercice dans cette séance.", "No exercise in this session."},
    {"ui.no.result", "Aucun résultat.", "No result."},
    {"ui.none", "Aucun", "None"},
    {"ui.no.verified.anatomy", "Aucune association vérifiée; aucune anatomie déduite du nom.", "No verified association; no anatomy inferred from the name."},
    {"ui.no.numeric.load", "Aucune charge numérique qualifiée", "No qualified numeric load"},
    {"ui.no.science", "Aucune fiche scientifique pour cet identifiant.", "No scientific record for this identifier."},
    {"ui.no.zone.history", "Aucune zone associée à un entraînement enregistré.", "No zone associated with recorded training."},
    {"ui.no.feminine", "Aucune", "None"},
    {"ui.load", "Charge : %s", "Load: %s"},
    {"ui.load.help", "Charge : 1 aucune · 2 externe · 3 assistance", "Load: 1 none · 2 external · 3 assistance"},
    {"ui.best.set.load", "Charge du meilleur set (kg)", "Best-set load (kg)"},
    {"ui.load.summary", "Charge max %.1f kg · volume total %.1f kg · moyen/occ. %.1f kg · /séance %.1f kg", "Max load %.1f kg · total volume %.1f kg · avg/occ. %.1f kg · /session %.1f kg"},
    {"ui.target.assistance", "Charge/assistance cible (kg)", "Target load/assistance (kg)"},
    {"ui.distance", "Distance : %.2f km", "Distance: %.2f km"},
    {"ui.distance.state", "Distance : %s%.2f km", "Distance: %s%.2f km"},
    {"ui.duration.minutes", "Durée : %d min", "Duration: %d min"},
    {"ui.max.duration", "Durée MAX mesurée", "Measured MAX duration"},
    {"ui.target.duration", "Durée cible (s)", "Target duration (s)"},
    {"ui.duration.recorded", "Durée enregistrée : %llu s · volume chargé non applicable", "Recorded duration: %llu s · loaded volume not applicable"},
    {"ui.average.duration.none", "Durée moyenne : indisponible (fin de séance absente)", "Average duration: unavailable (session end missing)"},
    {"ui.average.duration.unavailable", "Durée moyenne indisponible", "Average duration unavailable"},
    {"ui.session.duration", "Durée séances horodatées %s · moyenne %.0f min (%zu séance%s)", "Timed sessions duration %s · average %.0f min (%zu session%s)"},
    {"ui.equipment.choose", "Entrée choisir · x aucun · n créer puis reprendre (phase catalogue)", "Enter choose · x none · n create then resume (catalog phase)"},
    {"ui.keep.proposal", "Entrée conserver et revenir · d Abandonner la proposition", "Enter keep and return · d Discard proposal"},
    {"ui.enter.close", "Entrée fermer", "Enter close"},
    {"ui.exercise.actions", "Entrée fiche équipement · p Performances · m MAX · k Connaissances · e Modifier", "Enter equipment details · p Performance · m MAX · k Knowledge · e Edit"},
    {"ui.back.catalog", "Entrée pour revenir au catalogue", "Enter to return to catalog"},
    {"ui.exercise.result", "Exercice %s : %.120s", "Exercise %s: %.120s"},
    {"ui.exercise.position", "Exercice %zu/%zu — %s", "Exercise %zu/%zu — %s"},
    {"ui.exercise.reload.fail", "Exercice créé, mais le sélecteur n’a pas pu être rechargé.", "Exercise created, but the picker could not be reloaded."},
    {"ui.exercise.save.fail", "Exercice non enregistré; tous les champs sont conservés.", "Exercise not saved; all fields are preserved."},
    {"ui.exercise.recency", "Exercices %zu · occurrences %zu · dernière J-%zu", "Exercises %zu · occurrences %zu · latest D-%zu"},
    {"ui.exercise.none", "Exercices 0 · occurrences 0 · aucune séance", "Exercises 0 · occurrences 0 · no session"},
    {"ui.exercise.possible", "Exercices possibles", "Possible exercises"},
    {"ui.merge.fail", "Fusion impossible. Aucun changement confirmé.", "Merge failed. No change confirmed."},
    {"ui.merge.conflict", "Fusion refusée : profil ou zone principale incompatible. Aucun changement.", "Merge rejected: incompatible profile or primary zone. No change."},
    {"ui.quit.warning", "Le brouillon et les propositions gardés seulement pour ce run seront perdus.", "The draft and proposals kept only for this run will be lost."},
    {"ui.tracking.edit", "Modifier le mode de suivi ?", "Edit tracking mode?"},
    {"ui.observation.edit", "Modifier le relevé", "Edit observation"},
    {"ui.edit", "Modifier", "Edit"},
    {"ui.metadata.partial", "Métadonnées de zones/usage partiellement indisponibles.", "Zone/usage metadata partially unavailable."},
    {"ui.recording.help", "Organisation : 1 séries · 2 continu · ↑↓ choisir · Entrée continuer", "Organization: 1 sets · 2 continuous · ↑↓ choose · Enter continue"},
    {"ui.proposal.settings", "Paramètres de la proposition", "Proposal settings"},
    {"ui.weight.delta", "Poids %.1f kg · variation %+.1f kg", "Weight %.1f kg · change %+.1f kg"},
    {"ui.repetitions.ratio", "Répétitions %llu/%llu", "Repetitions %llu/%llu"},
    {"ui.max.repetitions", "Répétitions MAX mesurées", "Measured MAX repetitions"},
    {"ui.target.repetitions", "Répétitions cibles", "Target repetitions"},
    {"ui.merge.result", "Résultat de la fusion", "Merge result"},
    {"ui.tracking.help", "Suivi : 1 répétitions · 2 durée · ↑↓ choisir · Entrée continuer", "Tracking: 1 repetitions · 2 duration · ↑↓ choose · Enter continue"},
    {"ui.set.delete", "Supprimer cette série ?", "Delete this set?"},
    {"ui.sync.device.none", "Synchronisation impossible : aucun appareil MTP Trainlog détecté.", "Synchronization unavailable: no Trainlog MTP device detected."},
    {"ui.sync.database", "Synchronisation interrompue : erreur de base de données locale.", "Synchronization stopped: local database error."},
    {"ui.sync.io", "Synchronisation interrompue : erreur d’accès appareil ou fichier.", "Synchronization stopped: device or file access error."},
    {"ui.sync.no.report", "Synchronisation interrompue sans rapport de réussite.", "Synchronization stopped without a success report."},
    {"ui.sync.invalid", "Synchronisation refusée : données ou demande invalides.", "Synchronization rejected: invalid data or request."},
    {"ui.sync.busy", "Synchronisation refusée : une autre exécution est active.", "Synchronization rejected: another run is active."},
    {"ui.sync.version", "Synchronisation refusée : version de données non prise en charge.", "Synchronization rejected: unsupported data version."},
    {"ui.linked.session", "Séance liée : %s", "Linked session: %s"},
    {"ui.sessions.full", "Séances %zu · exercices %zu · occurrences %zu · jours %zu · semaines %zu", "Sessions %zu · exercises %zu · occurrences %zu · days %zu · weeks %zu"},
    {"ui.sessions.days", "Séances %zu · jours %zu · semaines %zu", "Sessions %zu · days %zu · weeks %zu"},
    {"ui.sessions.recorded", "Séances enregistrées : %zu", "Recorded sessions: %zu"},
    {"ui.sessions.timed", "Séances horodatées %s · moyenne %.0f min", "Timed sessions %s · average %.0f min"},
    {"ui.sets.activity", "Séries %zu · répétitions %llu · durée d’activité %s", "Sets %zu · repetitions %llu · activity duration %s"},
    {"ui.sets.repetitions", "Séries %zu · répétitions %llu", "Sets %zu · repetitions %llu"},
    {"ui.target.sets", "Séries cibles", "Target sets"},
    {"ui.actual.sets", "Séries réalisées — cibles séparées : %d×%d, repos %d s", "Completed sets — separate targets: %d×%d, rest %d s"},
    {"ui.waist.hip", "Taille / hanches", "Waist / hips"},
    {"ui.height.value", "Taille : %.1f cm", "Height: %.1f cm"},
    {"ui.height.range.error", "Taille attendue entre 100 et 250 cm; texte conservé.", "Expected height between 100 and 250 cm; text preserved."},
    {"ui.height.range", "Taille en cm : 100 à 250", "Height in cm: 100 to 250"},
    {"ui.speed", "Vitesse : %.1f km/h", "Speed: %.1f km/h"},
    {"ui.speed.state", "Vitesse : %s%.2f km/h", "Speed: %s%.2f km/h"},
    {"ui.zone", "Zone : %s", "Zone: %s"},
    {"ui.primary.secondary", "Zone principale : %s · secondaires : %zu", "Primary zone: %s · secondary: %zu"},
    {"ui.primary", "Zone principale : %s", "Primary zone: %s"},
    {"ui.recent.zone", "Zone travaillée récemment", "Recently trained zone"},
    {"ui.scientific.zones", "Zones scientifiques :", "Scientific zones:"},
    {"ui.secondary.zones", "Zones secondaires :", "Secondary zones:"},
    {"ui.no.data.lower", "aucune donnée", "no data"},
    {"ui.warning.help", "c/Entrée continuer · z changer de zone · Échap conserver et revenir", "c/Enter continue · z change zone · Esc keep and return"},
    {"ui.zone.help", "p principale · Espace secondaire · n effacer · Entrée enregistrer", "p primary · Space secondary · n clear · Enter save"},
    {"ui.save.draft.fail", "Échec de l’enregistrement; le brouillon est conservé.", "Save failed; the draft is preserved."},
    {"ui.previous.zone", "↑ Zone précédente", "↑ Previous zone"},
    {"ui.zone.bars", "↑/↓ zone · barres = séances distinctes sur 30 jours", "↑/↓ zone · bars = distinct sessions over 30 days"},
    {"ui.choose.save", "↑↓ ou 1–3 choisir · Entrée enregistrer", "↑↓ or 1–3 choose · Enter save"},
    {"ui.region.help", "↑↓ région · Entrée détail · chiffres période", "↑↓ region · Enter details · number selects period"},
    {"ui.next.zone", "↓ Zone suivante", "↓ Next zone"},
    {"ui.sub.exercise", "· Exercice", "· Exercise"},
    {"ui.sub.body", "· Mensurations", "· Body measurements"},
    {"ui.sub.current", "· Séance en cours", "· Current session"},
    {"ui.sub.zones", "· Zones", "· Zones"},
    {"ui.sub.body.trend", "· Évolution 12 mois", "· 12-month change"},
    {"ui.sub.plan", "· Programmer", "· Plan"},
    {"ui.sub.new.session", "· Nouvelle séance", "· New session"},
    {"ui.sub.completed", "· Effectuées", "· Completed"},
    {"ui.sub.session.detail", "· Détail séance", "· Session details"},
    {"ui.sub.exercise.detail", "· Fiche exercice", "· Exercise details"},
    {"ui.sub.knowledge", "· Connaissances", "· Knowledge"},
    {"ui.sub.performance", "· Performances", "· Performance"},
    {"ui.sub.exercise.max", "· MAX exercice", "· Exercise MAX"},
    {"ui.sub.equipment", "· Fiche équipement", "· Equipment details"},
    {"ui.sub.by.exercise", "· Par exercice", "· By exercise"},
    {"ui.sub.overview", "· Vue globale", "· Overview"},
    {"ui.sub.observation", "· Relevé corporel", "· Body observation"},
    {"ui.sub.metric.history", "· Historique mesure", "· Measurement history"},
    {"ui.sub.body.analysis", "· Analyse corporelle", "· Body analysis"},
    {"ui.sub.capacities", "· Capacités / MAX", "· Capacities / MAX"},
};

static const char *translated(const TrainlogTranslation *items, size_t count,
                              const char *value, bool by_key)
{
    size_t index;
    if (value == NULL) return by_key ? "[missing translation key]" : "";
    for (index = 0U; index < count; ++index) {
        const char *candidate = by_key ? items[index].key : items[index].fr;
        if (strcmp(candidate, value) == 0)
            return current_language == TRAINLOG_PRESENTATION_ENGLISH
                ? items[index].en : items[index].fr;
    }
    return by_key ? value : value;
}

void trainlog_presentation_init(void)
{
    current_language = TRAINLOG_PRESENTATION_FRENCH;
}

TrainlogPresentationLanguage trainlog_presentation_language(void)
{
    return current_language;
}

bool trainlog_presentation_set_language(TrainlogPresentationLanguage language)
{
    if (language != TRAINLOG_PRESENTATION_FRENCH &&
        language != TRAINLOG_PRESENTATION_ENGLISH) return false;
    current_language = language;
    return true;
}

const char *trainlog_presentation_language_code(
    TrainlogPresentationLanguage language)
{
    return language == TRAINLOG_PRESENTATION_ENGLISH ? "en" : "fr";
}

const char *trainlog_presentation_language_name(
    TrainlogPresentationLanguage language)
{
    return trainlog_presentation_text(language == TRAINLOG_PRESENTATION_ENGLISH
        ? "language.en" : "language.fr");
}

const char *trainlog_presentation_text(const char *key)
{
    return translated(translations,
        sizeof(translations) / sizeof(translations[0]), key, true);
}

const char *trainlog_presentation_source(const char *source)
{
    const char *result = translated(translations,
        sizeof(translations) / sizeof(translations[0]), source, false);
    if (result != source || source == NULL) return result;
    return translated(source_translations,
        sizeof(source_translations) / sizeof(source_translations[0]), source,
        false);
}

const char *trainlog_presentation_month_label(const char *source,
    char *output, size_t output_size)
{
    static const char *const french[] = {"janv.", "févr.", "mars", "avr.", "mai", "juin", "juil.", "août", "sept.", "oct.", "nov.", "déc."};
    static const char *const english[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    size_t index;
    if (source == NULL || output == NULL || output_size == 0U || current_language != TRAINLOG_PRESENTATION_ENGLISH) return source;
    for (index = 0U; index < 12U; ++index) {
        size_t prefix = strlen(french[index]);
        if (strncmp(source, french[index], prefix) == 0 && source[prefix] == ' ') {
            (void)snprintf(output, output_size, "%s%s", english[index], source + prefix);
            return output;
        }
    }
    return source;
}

int trainlog_presentation_vformat(char *output, size_t capacity,
    const char *source, va_list arguments)
{
    const char *format = trainlog_presentation_source(source);
    if (output == NULL && capacity > 0U) return -1;
    /* The catalog is compiled into this translation unit and its placeholder
     * parity is covered by tests. Keep nonliteral formatting contained at this
     * one presentation boundary without weakening project warning flags. */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    {
        int result = presentation_vsnprintf(output, capacity, format, arguments);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        return result;
    }
}

int trainlog_presentation_format(char *output, size_t capacity,
    const char *source, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, source);
    result = trainlog_presentation_vformat(output, capacity, source, arguments);
    va_end(arguments);
    return result;
}

int trainlog_presentation_format_input(char *output, size_t capacity,
    const char *format, ...)
{
    va_list arguments;
    locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    locale_t previous = (locale_t)0;
    int result;
    /* INVARIANT: changing presentation language never rewrites editable
     * numeric syntax, stored values, or exchange bytes. */
    if (c_locale != (locale_t)0) previous = uselocale(c_locale);
    va_start(arguments, format);
    result = vsnprintf(output, capacity, format, arguments);
    va_end(arguments);
    if (c_locale != (locale_t)0) {
        (void)uselocale(previous);
        freelocale(c_locale);
    }
    return result;
}

int trainlog_presentation_format_key(char *output, size_t capacity,
    const char *key, ...)
{
    va_list arguments;
    int result;
    const char *format = trainlog_presentation_text(key);
    va_start(arguments, key);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    result = presentation_vsnprintf(output, capacity, format, arguments);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    va_end(arguments);
    return result;
}

bool trainlog_presentation_compact_date(const char *iso_date, bool with_year,
    char *output, size_t capacity)
{
    int written;
    char first[3];
    char second[3];
    if (iso_date == NULL || output == NULL || strlen(iso_date) < 10U ||
        iso_date[4] != '-' || iso_date[7] != '-') return false;
    if (current_language == TRAINLOG_PRESENTATION_FRENCH) {
        first[0] = iso_date[8]; first[1] = iso_date[9];
        second[0] = iso_date[5]; second[1] = iso_date[6];
    } else {
        first[0] = iso_date[5]; first[1] = iso_date[6];
        second[0] = iso_date[8]; second[1] = iso_date[9];
    }
    first[2] = '\0'; second[2] = '\0';
    written = with_year
        ? snprintf(output, capacity, "%s/%s/%c%c", first, second,
            iso_date[2], iso_date[3])
        : snprintf(output, capacity, "%s/%s", first, second);
    return written >= 0 && (size_t)written < capacity;
}

bool trainlog_presentation_config_path(char *output, size_t capacity)
{
    const char *base = getenv("XDG_CONFIG_HOME");
    const char *home;
    int written;
    if (output == NULL || capacity == 0U) return false;
    if (base != NULL && base[0] != '\0')
        written = snprintf(output, capacity, "%s/trainlog/presentation.conf", base);
    else {
        home = getenv("HOME");
        if (home == NULL || home[0] == '\0') return false;
        written = snprintf(output, capacity, "%s/.config/trainlog/presentation.conf",
            home);
    }
    return written >= 0 && (size_t)written < capacity;
}

static bool ensure_parent_directories(const char *path)
{
    char copy[4096];
    char *slash;
    char *cursor;
    if (path == NULL || strlen(path) >= sizeof(copy)) return false;
    (void)snprintf(copy, sizeof(copy), "%s", path);
    slash = strrchr(copy, '/');
    if (slash == NULL) return false;
    *slash = '\0';
    for (cursor = copy + 1; *cursor != '\0'; ++cursor) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(copy, 0700) != 0 && errno != EEXIST) return false;
        *cursor = '/';
    }
    return mkdir(copy, 0700) == 0 || errno == EEXIST;
}

static bool sync_parent_directory(const char *path)
{
    char parent[4096];
    char *slash;
    int descriptor;
    bool ok;
    if (path == NULL || strlen(path) >= sizeof(parent)) return false;
    (void)snprintf(parent, sizeof(parent), "%s", path);
    slash = strrchr(parent, '/');
    if (slash == NULL) return false;
    *slash = '\0';
    descriptor = open(parent, O_RDONLY | O_DIRECTORY);
    if (descriptor < 0) return false;
    ok = fsync(descriptor) == 0;
    if (close(descriptor) != 0) ok = false;
    return ok;
}

bool trainlog_presentation_load(void)
{
    char path[4096];
    char buffer[128];
    FILE *file;
    size_t length;
    trainlog_presentation_init();
    if (!trainlog_presentation_config_path(path, sizeof(path))) return false;
    file = fopen(path, "rb");
    if (file == NULL) return errno == ENOENT;
    length = fread(buffer, 1U, sizeof(buffer) - 1U, file);
    if (ferror(file) != 0 || fclose(file) != 0) return false;
    buffer[length] = '\0';
    if (strcmp(buffer, "version=1\nlanguage=fr\n") == 0) return true;
    if (strcmp(buffer, "version=1\nlanguage=en\n") == 0) {
        current_language = TRAINLOG_PRESENTATION_ENGLISH;
        return true;
    }
    return false;
}

TrainlogPresentationSaveResult trainlog_presentation_save(void)
{
    char path[4096];
    char temporary[4128];
    const char *code = trainlog_presentation_language_code(current_language);
    int descriptor;
    int length;
    ssize_t written;
    bool file_ok;
    if (!trainlog_presentation_config_path(path, sizeof(path)) ||
        !ensure_parent_directories(path))
        return TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED;
    length = snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED;
    /* WHY: a crashed prior process may leave a temporary file behind.
     * CONTRACT: mkstemp provides collision-safe same-directory creation;
     * mode 0600, file fsync and atomic rename remain mandatory.
     * INVARIANT: failure never replaces the last valid preference. */
    descriptor = mkstemp(temporary);
    if (descriptor < 0) return TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED;
    if (fchmod(descriptor, 0600) != 0) {
        (void)close(descriptor); (void)unlink(temporary);
        return TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED;
    }
    length = snprintf(path, sizeof(path), "version=1\nlanguage=%s\n", code);
    written = write(descriptor, path, (size_t)length);
    /* CONTRACT: close executes exactly once on every post-creation path.
     * Short-write, fsync and close failures are aggregated; unlink follows
     * closure, and no failure before rename changes the prior preference. */
    file_ok = written == length;
    if (file_ok && fsync(descriptor) != 0) file_ok = false;
    if (close(descriptor) != 0) file_ok = false;
    if (!file_ok) {
        (void)unlink(temporary);
        return TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED;
    }
    if (!trainlog_presentation_config_path(path, sizeof(path)) ||
        rename(temporary, path) != 0) {
        (void)unlink(temporary);
        return TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED;
    }
    /* CONTRACT: rename supplies atomic replacement; syncing its directory
     * makes the new directory entry durable before success is reported. */
    /* INVARIANT: after rename the selected preference is committed and must
     * not be described or treated as rolled back, even if directory fsync
     * cannot prove crash durability. */
    return sync_parent_directory(path)
        ? TRAINLOG_PRESENTATION_SAVE_COMMITTED_DURABLE
        : TRAINLOG_PRESENTATION_SAVE_COMMITTED_DURABILITY_UNCERTAIN;
}

void trainlog_presentation_resolve_save(
    TrainlogPresentationLanguage previous_language,
    TrainlogPresentationSaveResult result)
{
    /* CONTRACT: only a pre-rename failure rolls runtime presentation back.
     * Both committed outcomes retain the selected language because the new
     * file is already visible after rename. */
    if (result == TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED)
        (void)trainlog_presentation_set_language(previous_language);
}
