#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "trainlog/app_shell.h"
#include "trainlog/presentation.h"
#include "trainlog/sync_screen_action.h"

static void test_default_and_catalog(void)
{
    TrainlogShellLayout french;
    TrainlogShellLayout english;
    char date[9];
    char dynamic[512];
    trainlog_presentation_init();
    assert(trainlog_presentation_language() == TRAINLOG_PRESENTATION_FRENCH);
    assert(strcmp(trainlog_presentation_text("nav.settings"), "Paramètres") == 0);
    assert(strcmp(trainlog_route_title(TRAINLOG_ROUTE_STATS_TRAINING),
        "Statistiques / Entraînement") == 0);
    assert(trainlog_presentation_compact_date("2026-09-15", true,
        date, sizeof(date)));
    assert(strcmp(date, "15/09/26") == 0);
    trainlog_shell_layout_compute(72, 20, &french);
    assert(french.usable);

    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_ENGLISH));
    assert(strcmp(trainlog_presentation_text("nav.settings"), "Settings") == 0);
    assert(strcmp(trainlog_route_title(TRAINLOG_ROUTE_STATS_TRAINING),
        "Statistics / Training") == 0);
    assert(strcmp(trainlog_sync_screen_confirmation(TRAINLOG_SYNC_BIDIRECTIONAL),
        "Synchronize Android and PC in both directions now?") == 0);
    assert(strstr(trainlog_sync_screen_footer(100), "Refresh device") != NULL);
    assert(strcmp(trainlog_presentation_source("Aucune donnée disponible."),
        "No data available.") == 0);
    assert(trainlog_presentation_format_key(dynamic, sizeof(dynamic),
        "msg.actual.required", "Squat Français") > 0);
    assert(strcmp(dynamic, "Enter an actual result for Squat Français.") == 0);
    assert(trainlog_presentation_format_key(dynamic, sizeof(dynamic),
        "sync.history.legacy", "2026-09-15T10:30:00Z",
        trainlog_presentation_text("sync.state.success")) > 0);
    assert(strstr(dynamic, "Historical PC↔Android synchronization") != NULL);
    assert(strstr(dynamic, "Timestamp: 2026-09-15T10:30:00Z") != NULL);
    assert(strstr(dynamic, "Status: successful") != NULL);
    /* Representative generator/profile/body/MAX/search/merge/feedback UI
     * formats must render English while preserving caller-provided values. */
    assert(trainlog_presentation_format(dynamic, sizeof(dynamic),
        "Objectif : %s", "Force") > 0);
    assert(strcmp(dynamic, "Goal: Force") == 0);
    assert(trainlog_presentation_format(dynamic, sizeof(dynamic),
        "%zu exercice(s) · cible %d min · estimation %d min", (size_t)3,
        45, 42) > 0);
    assert(strcmp(dynamic, "3 exercise(s) · target 45 min · estimate 42 min") == 0);
    assert(trainlog_presentation_format(dynamic, sizeof(dynamic),
        "Tour de taille %.1f cm · variation %+.1f cm", 81.5, -1.0) > 0);
    assert(strcmp(dynamic, "Waist circumference 81.5 cm · change -1.0 cm") == 0);
    assert(trainlog_presentation_format(dynamic, sizeof(dynamic),
        "Historique des MAX") > 0);
    assert(strcmp(dynamic, "MAX history") == 0);
    assert(trainlog_presentation_format(dynamic, sizeof(dynamic),
        "Recherche cible : %s_", "Squat") > 0);
    assert(strcmp(dynamic, "Search target: Squat_") == 0);
    assert(trainlog_presentation_format(dynamic, sizeof(dynamic),
        "Contexte : %s · %s · %zu point%s", "Squat", "MAX", (size_t)1,
        "") > 0);
    assert(strcmp(dynamic, "Context: Squat · MAX · 1 point") == 0);
    assert(trainlog_presentation_compact_date("2026-09-15", true,
        date, sizeof(date)));
    assert(strcmp(date, "09/15/26") == 0);
    /* Persisted and catalog names are not keys and must remain byte-exact. */
    assert(strcmp(trainlog_presentation_source("Développé incliné — Alice"),
        "Développé incliné — Alice") == 0);
    trainlog_shell_layout_compute(72, 20, &english);
    assert(memcmp(&french, &english, sizeof(french)) == 0);
    assert(!trainlog_presentation_set_language(
        (TrainlogPresentationLanguage)99));
    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_FRENCH));
    /* Decimal localization is scoped to the float conversion: persisted/user
     * text is not parsed or mutated merely because it contains digits. */
    assert(trainlog_presentation_format(dynamic, sizeof(dynamic),
        "Valeur %.1f · modèle %s", 12.5, "Model 1.2") > 0);
    assert(strcmp(dynamic, "Valeur 12,5 · modèle Model 1.2") == 0);
    assert(strcmp(trainlog_presentation_source("Pendant la séance"),
        "Pendant la séance") == 0);
    assert(strcmp(trainlog_presentation_month_label("févr. 2026", dynamic,
        sizeof(dynamic)), "févr. 2026") == 0);
    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_ENGLISH));
    assert(strcmp(trainlog_presentation_source("Pendant la séance"),
        "During session") == 0);
    assert(strcmp(trainlog_presentation_month_label("févr. 2026", dynamic,
        sizeof(dynamic)), "Feb 2026") == 0);
    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_FRENCH));
    assert(trainlog_presentation_format_key(dynamic, sizeof(dynamic),
        "msg.actual.required", "Squat Français") > 0);
    assert(strcmp(dynamic, "Saisissez un résultat réel pour Squat Français.") == 0);
}

static void test_selected_language_owns_decimals(void)
{
    char output[128];
    char *original = setlocale(LC_NUMERIC, NULL);
    char saved[128];
    if (original != NULL) (void)snprintf(saved, sizeof(saved), "%s", original);
    else saved[0] = '\0';

    trainlog_presentation_init();
    assert(trainlog_presentation_format(output, sizeof(output),
        "Valeur %.2f · delta %+.1f", 12.5, 0.25) > 0);
    assert(strcmp(output, "Valeur 12,50 · delta +0,2") == 0);
    assert(trainlog_presentation_format(output, sizeof(output),
        "%+.1f%%", 3.25) > 0);
    assert(strcmp(output, "+3,2%") == 0);
    assert(trainlog_presentation_format(output, sizeof(output),
        "%2$*3$.*4$f · %1$s · %%", "Model 1.2", 12.5, 7, 2) > 0);
    assert(strcmp(output, "  12,50 · Model 1.2 · %") == 0);
    assert(trainlog_presentation_format(output, sizeof(output),
        "%Lf · %zu", (long double)1.25L, (size_t)4) > 0);
    assert(strcmp(output, "1,250000 · 4") == 0);
    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_ENGLISH));
    assert(trainlog_presentation_format(output, sizeof(output),
        "Value %.2f", 12.5) > 0);
    assert(strcmp(output, "Value 12.50") == 0);

    /* Contrary-locale integration proof when the host supplies French. The
     * assertions above remain the locale-independent fallback proof. */
    if (setlocale(LC_NUMERIC, "fr_FR.UTF-8") != NULL ||
        setlocale(LC_NUMERIC, "fr_FR.utf8") != NULL) {
        assert(trainlog_presentation_format(output, sizeof(output),
            "Value %.2f", 12.5) > 0);
        assert(strcmp(output, "Value 12.50") == 0);
        assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_FRENCH));
        assert(trainlog_presentation_format(output, sizeof(output),
            "Valeur %.2f", 12.5) > 0);
        assert(strcmp(output, "Valeur 12,50") == 0);
    }
    if (saved[0] != '\0') assert(setlocale(LC_NUMERIC, saved) != NULL);
}

static void test_persistence(void)
{
    char root_template[] = "/tmp/trainlog-presentation-XXXXXX";
    char *root = mkdtemp(root_template);
    char path[4096];
    char directory[4096];
    char invalid[] = "version=1\nlanguage=de\n";
    FILE *file;
    assert(root != NULL);
    assert(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    assert(trainlog_presentation_config_path(path, sizeof(path)));
    assert(strstr(path, "/trainlog/presentation.conf") != NULL);

    assert(unsetenv("XDG_CONFIG_HOME") == 0);
    assert(setenv("HOME", root, 1) == 0);
    assert(trainlog_presentation_config_path(path, sizeof(path)));
    assert(strstr(path, "/.config/trainlog/presentation.conf") != NULL);
    assert(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    assert(trainlog_presentation_config_path(path, sizeof(path)));

    trainlog_presentation_init();
    assert(trainlog_presentation_load());
    assert(trainlog_presentation_language() == TRAINLOG_PRESENTATION_FRENCH);
    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_ENGLISH));
    (void)snprintf(directory, sizeof(directory), "%s/trainlog", root);
    assert(mkdir(directory, 0700) == 0);
    {
        char stale[4128];
        FILE *stale_file;
        (void)snprintf(stale, sizeof(stale), "%s.tmp.%ld", path,
            (long)getpid());
        stale_file = fopen(stale, "wb");
        assert(stale_file != NULL);
        assert(fclose(stale_file) == 0);
        assert(trainlog_presentation_save() ==
            TRAINLOG_PRESENTATION_SAVE_COMMITTED_DURABLE);
        assert(unlink(stale) == 0);
    }
    trainlog_presentation_init();
    assert(trainlog_presentation_load());
    assert(trainlog_presentation_language() == TRAINLOG_PRESENTATION_ENGLISH);

    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_FRENCH));
    assert(trainlog_presentation_save() ==
        TRAINLOG_PRESENTATION_SAVE_COMMITTED_DURABLE);
    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_ENGLISH));
    assert(trainlog_presentation_load());
    assert(trainlog_presentation_language() == TRAINLOG_PRESENTATION_FRENCH);

    file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(invalid, 1U, strlen(invalid), file) == strlen(invalid));
    assert(fclose(file) == 0);
    assert(!trainlog_presentation_load());
    assert(trainlog_presentation_language() == TRAINLOG_PRESENTATION_FRENCH);

    assert(unlink(path) == 0);
    (void)snprintf(directory, sizeof(directory), "%s/trainlog", root);
    assert(rmdir(directory) == 0);
    file = fopen(directory, "wb");
    assert(file != NULL);
    assert(fclose(file) == 0);
    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_ENGLISH));
    assert(trainlog_presentation_save() ==
        TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED);
    trainlog_presentation_resolve_save(TRAINLOG_PRESENTATION_FRENCH,
        TRAINLOG_PRESENTATION_SAVE_NOT_COMMITTED);
    assert(trainlog_presentation_language() == TRAINLOG_PRESENTATION_FRENCH);
    assert(trainlog_presentation_set_language(TRAINLOG_PRESENTATION_ENGLISH));
    trainlog_presentation_resolve_save(TRAINLOG_PRESENTATION_FRENCH,
        TRAINLOG_PRESENTATION_SAVE_COMMITTED_DURABILITY_UNCERTAIN);
    assert(trainlog_presentation_language() == TRAINLOG_PRESENTATION_ENGLISH);
    assert(unlink(directory) == 0);

    assert(rmdir(root) == 0);
}

int main(void)
{
    test_default_and_catalog();
    test_selected_language_owns_decimals();
    test_persistence();
    puts("presentation tests passed");
    return 0;
}
