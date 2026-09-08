/**
 * @file tui.c
 * @brief Trainlog Notcurses terminal interface.
 */

#include "trainlog/tui.h"

#include <ctype.h>
#include <fcntl.h>
#include <locale.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <wchar.h>
#include <unistd.h>

#include <uuid/uuid.h>

#include "trainlog/terminal.h"

#include "trainlog/bodyviz.h"
#include "trainlog/body_analytics.h"
#include "trainlog/catalog.h"
#include "trainlog/duration.h"
#include "trainlog/equipment_catalog.h"
#include "trainlog/id.h"
#include "trainlog/measured_max.h"
#include "trainlog/mtp.h"
#include "trainlog/sync.h"
#include "trainlog/sync_history.h"
#include "trainlog/sync_screen_action.h"
#include "trainlog/reps.h"
#include "trainlog/theme.h"
#include "trainlog/timeutil.h"
#include "trainlog/usb.h"

#define MAX_EXERCISES 128U
#define MAX_SESSION_EXERCISES 32U
#define MAX_SETS_PER_EXERCISE 64U
#define MAX_SESSIONS 128U
#define MAX_WEIGHT_POINTS 256U

#define MAX_BODY_METRIC_POINTS 256U

/*
 * INVARIANT: this pointer is assigned only for the dynamic extent of one
 * trainlog_tui_run() call and is cleared before Notcurses shutdown.  The
 * screen implementation predates explicit context parameters; the terminal
 * object itself remains owned by the public run entry point.
 */
static TrainlogTerminal *tui_terminal;


/* TRAINLOG_TUI_V02_POLISH */
/* TRAINLOG_TUI_PROFILED_EXERCISE_CREATION */
/* TRAINLOG_VARIABLE_SET_REPS_V1 */
/* TRAINLOG_SYNC_RESPONSIVE_CACHE */
/* TRAINLOG_SYNC_LARGE_LAYOUT_S_FIX */
/* TRAINLOG_SYNC_HISTORY_BIDIRECTIONAL_V1 */
/* TRAINLOG_SYNC_PC_TO_ANDROID_DIAGNOSTICS */
/* TRAINLOG_SYNC_FULL_MTP_SILENCE */

typedef enum DashboardAction {
    DASHBOARD_NEW_SESSION = 0,
    DASHBOARD_HISTORY,
    DASHBOARD_EXERCISES,
    DASHBOARD_EQUIPMENT,
    DASHBOARD_BODY,
    DASHBOARD_SYNC,
    DASHBOARD_QUIT
} DashboardAction;

/* CONTRACT: every primary navigation renderer and selector uses this single
 * list/count pair so adding a workflow cannot desynchronize array bounds. */
static const char *const primary_nav_labels[] = {
    "0 Accueil", "1 Séance", "2 Historique", "3 Exercices",
    "4 Équipements", "5 Corps", "6 Sync"
};

#define PRIMARY_NAV_COUNT \
    ((int)(sizeof(primary_nav_labels) / sizeof(primary_nav_labels[0])))

/* TRAINLOG_DASHBOARD_FORWARD_DECLARATIONS */
static DashboardAction screen_dashboard(
    TrainlogDatabase *database
);

static void screen_exercises(
    TrainlogDatabase *database
);

static void screen_sync(
    TrainlogDatabase *database
);
static void screen_equipment(TrainlogDatabase *database);
static void screen_equipment_detail(const TrainlogResolvedEquipment *equipment);
static void wait_key(void);
static void status_line(const char *text, TrainlogColorRole role);
static bool prompt_text(int row, const char *label, char *output, size_t output_size, bool allow_empty);
static bool prompt_int_value(int row, const char *label, int minimum,
                             int maximum, int default_value, int *output);

static void dashboard_panel(
    int top,
    int left,
    int bottom,
    int right,
    const char *label
);

static void dashboard_ascii_header(void);

static void draw_dashboard_body_graph(
    TrainlogDatabase *database
);

static void focused_panel(
    int top,
    int left,
    int bottom,
    int right,
    const char *label,
    bool active
);

static void primary_top_navbar(
    int active_page,
    int selected_page,
    bool focused
);

static bool primary_top_nav_forward(int key);
static bool primary_top_nav_activate(int selected_page);

static void section_ascii_header(const char *subtitle);


static void session_history_datetime(
    const char *timestamp,
    char output[17]
);


static void draw_shell(const char *heading, const char *footer)
{
    trainlog_terminal_erase(tui_terminal);
    trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

    trainlog_terminal_style_on(tui_terminal, TRAINLOG_TEXT_BOLD | trainlog_theme_style(TRAINLOG_COLOR_ACCENT));
    trainlog_terminal_printf(tui_terminal, 1, 2, " %s ", heading);
    trainlog_terminal_style_off(tui_terminal, TRAINLOG_TEXT_BOLD | trainlog_theme_style(TRAINLOG_COLOR_ACCENT));

    trainlog_terminal_style_on(tui_terminal, trainlog_theme_style(TRAINLOG_COLOR_MUTED));
    trainlog_terminal_printf(tui_terminal, trainlog_terminal_rows(tui_terminal) - 2, 2, "%-*s", trainlog_terminal_columns(tui_terminal) - 4, footer);
    trainlog_terminal_style_off(tui_terminal, trainlog_theme_style(TRAINLOG_COLOR_MUTED));
}

/* WHY: one read-only catalogue view combines frozen supplied definitions with
 * durable local ones; neither source is silently substituted for the other. */
static bool equipment_text_matches(const char *text, const char *query)
{
    size_t index;
    size_t query_index;
    if (query == NULL || query[0] == '\0') return true;
    if (text == NULL) return false;
    for (index = 0U; text[index] != '\0'; ++index) {
        for (query_index = 0U; query[query_index] != '\0'; ++query_index) {
            unsigned char left = (unsigned char)text[index + query_index];
            unsigned char right = (unsigned char)query[query_index];
            if (left == '\0' || tolower(left) != tolower(right)) break;
        }
        if (query[query_index] == '\0') return true;
    }
    return false;
}

static size_t equipment_collect(TrainlogDatabase *database, const char *query,
                                TrainlogResolvedEquipment *output, size_t capacity)
{
    TrainlogCustomEquipment customs[128];
    const TrainlogEquipment *supplied_matches[128];
    size_t custom_count = 0U;
    size_t supplied_count;
    size_t count = 0U;
    size_t index;
    supplied_count = query != NULL && query[0] != '\0'
        ? trainlog_equipment_catalog_search(query, supplied_matches, 128U)
        : trainlog_equipment_catalog_count();
    if (supplied_count > 128U) supplied_count = 128U;
    for (index = 0U; index < supplied_count && count < capacity; ++index) {
        const TrainlogEquipment *item = query != NULL && query[0] != '\0'
            ? supplied_matches[index] : trainlog_equipment_catalog_at(index);
        if (item != NULL && trainlog_database_resolve_equipment(database,
                item->equipment_id, &output[count]) == TRAINLOG_STATUS_OK) ++count;
    }
    if (trainlog_database_list_custom_equipment(database, customs, 128U,
            &custom_count) != TRAINLOG_STATUS_OK) return count;
    if (custom_count > 128U) custom_count = 128U;
    for (index = 0U; index < custom_count && count < capacity; ++index) {
        if (equipment_text_matches(customs[index].display_name, query) ||
            equipment_text_matches(customs[index].label_name, query) ||
            equipment_text_matches(customs[index].equipment_type, query)) {
            if (trainlog_database_resolve_equipment(database,
                    customs[index].equipment_id, &output[count]) == TRAINLOG_STATUS_OK) ++count;
        }
    }
    return count;
}

static const char *equipment_origin_label(TrainlogEquipmentOrigin origin)
{
    if (origin == TRAINLOG_EQUIPMENT_SUPPLIED) return "fourni";
    if (origin == TRAINLOG_EQUIPMENT_CUSTOM) return "personnalisé";
    return "inconnu";
}

static void generate_custom_equipment_uuid(char output[TRAINLOG_UUID_TEXT_LENGTH + 1U])
{
    uuid_t value;
    /* CONTRACT: equipment IDs are opaque alongside manifest slugs. A raw
     * UUIDv4 avoids extending the frozen creator-prefix namespace. */
    uuid_generate_random(value);
    uuid_unparse_lower(value, output);
}

static void screen_equipment_detail(const TrainlogResolvedEquipment *equipment)
{
    if (equipment == NULL) return;
    draw_shell("Fiche équipement", "Une touche pour revenir");
    trainlog_terminal_printf(tui_terminal, 4, 4, "%s", equipment->display_name);
    trainlog_terminal_printf(tui_terminal, 6, 4, "Identifiant : %s", equipment->equipment_id);
    trainlog_terminal_printf(tui_terminal, 7, 4, "Étiquette : %s",
        equipment->label_name[0] != '\0' ? equipment->label_name : "—");
    trainlog_terminal_printf(tui_terminal, 8, 4, "Type : %s",
        equipment->equipment_type[0] != '\0' ? equipment->equipment_type : "—");
    trainlog_terminal_printf(tui_terminal, 9, 4, "Charge : %s · Origine : %s",
        equipment->load_semantics[0] != '\0' ? equipment->load_semantics : "—",
        equipment_origin_label(equipment->origin));
    wait_key();
}

static bool choose_equipment(TrainlogDatabase *database, char *output, size_t output_size)
{
    TrainlogResolvedEquipment items[256];
    char query[TRAINLOG_NAME_MAX + 1U] = "";
    size_t selected = 0U;
    for (;;) {
        size_t count = equipment_collect(database, query, items, 256U);
        size_t visible = trainlog_terminal_rows(tui_terminal) > 7
            ? (size_t)(trainlog_terminal_rows(tui_terminal) - 7) : 1U;
        size_t top = selected > 0U && selected - 1U >= visible
            ? selected - visible : 0U;
        size_t i;
        int key;
        if (selected > count) selected = count;
        draw_shell("Choisir un équipement", "↑↓ naviguer  Entrée choisir  / rechercher  x aucun  Échap annuler");
        trainlog_terminal_printf(tui_terminal, 3, 4, "Recherche : %s", query[0] ? query : "—");
        if (selected == 0U) trainlog_terminal_style_on(tui_terminal, TRAINLOG_TEXT_REVERSE);
        trainlog_terminal_printf(tui_terminal, 4, 4, " %-48s ", "Aucun équipement");
        if (selected == 0U) trainlog_terminal_style_off(tui_terminal, TRAINLOG_TEXT_REVERSE);
        for (i = 0U; i < visible && top + i < count; ++i) {
            size_t absolute = top + i;
            if (absolute + 1U == selected) trainlog_terminal_style_on(tui_terminal, TRAINLOG_TEXT_REVERSE);
            trainlog_terminal_printf(tui_terminal, 5 + (int)i, 4, "%-46.46s  %s",
                items[absolute].display_name, equipment_origin_label(items[absolute].origin));
            if (absolute + 1U == selected) trainlog_terminal_style_off(tui_terminal, TRAINLOG_TEXT_REVERSE);
        }
        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);
        if (key == 27) return false;
        if (key == '/') {
            draw_shell("Rechercher un équipement", "Entrée applique · Échap annule");
            if (prompt_text(4, "Recherche", query, sizeof(query), true)) selected = 0U;
        } else if (key == 'x' || key == 'X') {
            output[0] = '\0';
            return true;
        } else if (key == TRAINLOG_KEY_UP && selected > 0U) --selected;
        else if (key == TRAINLOG_KEY_DOWN && selected < count) ++selected;
        else if (key == '\n' || key == TRAINLOG_KEY_ENTER) {
            if (selected == 0U) output[0] = '\0';
            else (void)snprintf(output, output_size, "%s", items[selected - 1U].equipment_id);
            return true;
        }
    }
}

static void screen_equipment(TrainlogDatabase *database)
{
    char query[TRAINLOG_NAME_MAX + 1U] = "";
    size_t selected = 0U;
    for (;;) {
        TrainlogResolvedEquipment items[256];
        size_t total = equipment_collect(database, query, items, 256U);
        size_t visible = trainlog_terminal_rows(tui_terminal) > 7
            ? (size_t)(trainlog_terminal_rows(tui_terminal) - 7) : 1U;
        size_t top;
        size_t i;
        int key;
        if (selected >= total && total > 0U) selected = total - 1U;
        top = selected >= visible ? selected - visible + 1U : 0U;
        draw_shell("TRAINLOG — Équipements", "↑↓ naviguer  Entrée fiche  / rechercher  n créer  b/Échap retour");
        trainlog_terminal_printf(tui_terminal, 3, 4, "Recherche : %s", query[0] ? query : "—");
        for (i = 0U; i < visible && top + i < total; ++i) {
            size_t absolute = top + i;
            if (absolute == selected) trainlog_terminal_style_on(tui_terminal, TRAINLOG_TEXT_REVERSE);
            trainlog_terminal_printf(tui_terminal, 4 + (int)i, 4, "%-46.46s  %s",
                items[absolute].display_name, equipment_origin_label(items[absolute].origin));
            if (absolute == selected) trainlog_terminal_style_off(tui_terminal, TRAINLOG_TEXT_REVERSE);
        }
        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);
        if (key == 27 || key == 'b' || key == 'B') return;
        if (key == TRAINLOG_KEY_UP && selected > 0U) --selected;
        else if (key == TRAINLOG_KEY_DOWN && selected + 1U < total) ++selected;
        else if (key == '/') {
            draw_shell("Rechercher un équipement", "Entrée applique · Échap annule");
            if (prompt_text(4, "Recherche", query, sizeof(query), true)) selected = 0U;
        }
        else if (key == 'n' || key == 'N') {
            TrainlogCustomEquipment equipment;
            char generated[TRAINLOG_UUID_TEXT_LENGTH + 1U];
            int load_semantics = 2;
            (void)memset(&equipment, 0, sizeof(equipment));
            draw_shell("Nouvel équipement", "Échap annule ; Entrée valide chaque champ");
            if (!prompt_text(4, "Nom convivial", equipment.display_name, sizeof(equipment.display_name), false) ||
                !prompt_text(6, "Nom étiquette (optionnel)", equipment.label_name, sizeof(equipment.label_name), true) ||
                !prompt_text(8, "Type", equipment.equipment_type, sizeof(equipment.equipment_type), false) ||
                !prompt_int_value(10, "Charge 1=aucune 2=externe 3=assistance",
                    1, 3, 2, &load_semantics)) continue;
            (void)snprintf(equipment.load_semantics, sizeof(equipment.load_semantics), "%s",
                load_semantics == 1 ? "none" :
                (load_semantics == 2 ? "external" : "assistance"));
            generate_custom_equipment_uuid(generated);
            (void)snprintf(equipment.equipment_id, sizeof(equipment.equipment_id), "%s", generated);
            if (trainlog_database_create_custom_equipment(database, &equipment) != TRAINLOG_STATUS_OK) {
                status_line("Équipement non créé : nom ou identifiant invalide/conflit.", TRAINLOG_COLOR_ERROR); wait_key();
            }
        } else if ((key == '\n' || key == TRAINLOG_KEY_ENTER) && total > 0U)
            screen_equipment_detail(&items[selected]);
    }
}

static void wait_key(void)
{
    trainlog_terminal_style_on(tui_terminal, trainlog_theme_style(TRAINLOG_COLOR_MUTED));
    trainlog_terminal_printf(tui_terminal, trainlog_terminal_rows(tui_terminal) - 2, 2, "Appuyez sur une touche pour continuer...");
    trainlog_terminal_style_off(tui_terminal, trainlog_theme_style(TRAINLOG_COLOR_MUTED));
    trainlog_terminal_render(tui_terminal);
    (void)trainlog_terminal_get_key(tui_terminal);
}

static void status_line(const char *text, TrainlogColorRole role)
{
    trainlog_terminal_style_on(tui_terminal, trainlog_theme_style(role));
    trainlog_terminal_printf(tui_terminal, trainlog_terminal_rows(tui_terminal) - 3, 2, "%-*s", trainlog_terminal_columns(tui_terminal) - 4, text);
    trainlog_terminal_style_off(tui_terminal, trainlog_theme_style(role));
}

static bool prompt_text(
    int row,
    const char *label,
    char *output,
    size_t output_size,
    bool allow_empty
)
{
    size_t used = 0U;
    int input_column;
    int cursor_row;
    bool accepted = false;

    if (label == NULL ||
        output == NULL ||
        output_size < 2U) {
        return false;
    }

    output[0] = '\0';

    trainlog_terminal_printf(tui_terminal,
        row,
        2,
        "%s",
        label
    );

    trainlog_terminal_cursor_yx(tui_terminal, &cursor_row, &input_column);

    (void)cursor_row;


    (void)trainlog_terminal_cursor_visible(tui_terminal, true);

    for (;;) {
        int value;
        char encoded[5];

        trainlog_terminal_move(tui_terminal,
            row,
            input_column
        );

        trainlog_terminal_clear_to_end(tui_terminal);

        if (used > 0U) {
            trainlog_terminal_putn(tui_terminal,
                output,
                used
            );
        }

        trainlog_terminal_render(tui_terminal);

        if (!trainlog_terminal_read_unicode(tui_terminal, &value, encoded)) {
            break;
        }

        if (value < 0) {
            int key = value;

            if (key == TRAINLOG_KEY_ENTER) {
                if (allow_empty ||
                    used > 0U) {
                    accepted = true;
                    break;
                }

                continue;
            }

            if (key == TRAINLOG_KEY_BACKSPACE ||
                key == TRAINLOG_KEY_DELETE) {
                if (used > 0U) {
                    do {
                        --used;
                    } while (
                        used > 0U &&
                        (((unsigned char)output[used] &
                          0xc0U) == 0x80U)
                    );

                    output[used] = '\0';
                }

                continue;
            }

            continue;
        }

        if (value == 27) {
            accepted = false;
            break;
        }

        if (value == '\n' || value == '\r') {
            if (allow_empty ||
                used > 0U) {
                accepted = true;
                break;
            }

            continue;
        }

        if (value == 8 || value == 127) {
            if (used > 0U) {
                do {
                    --used;
                } while (
                    used > 0U &&
                    (((unsigned char)output[used] &
                      0xc0U) == 0x80U)
                );

                output[used] = '\0';
            }

            continue;
        }

        if (value >= 32) {
            size_t encoded_size;
            encoded_size = strlen(encoded);

            if (encoded_size == 0U ||
                encoded_size >
                    output_size - used - 1U) {
                continue;
            }

            (void)memcpy(
                output + used,
                encoded,
                encoded_size
            );

            used += encoded_size;
            output[used] = '\0';
        }
    }


    (void)trainlog_terminal_cursor_visible(tui_terminal, false);

    return accepted;
}

static bool parse_int(const char *text, int minimum, int maximum, int *output)
{
    char *end = NULL;
    long value;

    if (text == NULL || output == NULL || text[0] == '\0') {
        return false;
    }

    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' ||
        value < (long)minimum || value > (long)maximum) {
        return false;
    }

    *output = (int)value;
    return true;
}

static bool parse_double_positive(const char *text, double *output)
{
    char *end = NULL;
    double value;

    if (text == NULL || output == NULL || text[0] == '\0') {
        return false;
    }

    value = strtod(text, &end);
    if (end == text || *end != '\0' || value <= 0.0) {
        return false;
    }

    *output = value;
    return true;
}

static bool prompt_int_value(
    int row,
    const char *label,
    int minimum,
    int maximum,
    int default_value,
    int *output
)
{
    char buffer[64];

    for (;;) {
        char decorated[128];

        (void)snprintf(
            decorated,
            sizeof(decorated),
            "%s [%d]: ",
            label,
            default_value
        );

        if (!prompt_text(row, decorated, buffer, sizeof(buffer), true)) {
            return false;
        }

        if (buffer[0] == '\0') {
            *output = default_value;
            return true;
        }

        if (parse_int(buffer, minimum, maximum, output)) {
            return true;
        }

        status_line("Valeur entière invalide.", TRAINLOG_COLOR_ERROR);
        trainlog_terminal_render(tui_terminal);
    }
}

static bool prompt_optional_double(
    int row,
    const char *label,
    bool *present,
    double *output
)
{
    char buffer[64];

    for (;;) {
        if (!prompt_text(row, label, buffer, sizeof(buffer), true)) {
            return false;
        }

        if (buffer[0] == '\0') {
            *present = false;
            *output = 0.0;
            return true;
        }

        if (parse_double_positive(buffer, output)) {
            *present = true;
            return true;
        }

        status_line("Nombre positif invalide.", TRAINLOG_COLOR_ERROR);
        trainlog_terminal_render(tui_terminal);
    }
}

/* TRAINLOG_EXERCISE_PERFORMANCE_TUI */

#define EXERCISE_GRAPH_POINTS 12U

static const char *exercise_load_mode_label(
    TrainlogLoadMode mode
)
{
    switch (mode) {
    case TRAINLOG_LOAD_EXTERNAL:
        return "charge externe";

    case TRAINLOG_LOAD_ASSISTANCE:
        return "assistance";

    case TRAINLOG_LOAD_NONE:
    default:
        return "sans charge";
    }
}

static void exercise_short_date(
    const char *timestamp,
    char output[11]
)
{
    if (timestamp == NULL ||
        strlen(timestamp) < 10U) {
        (void)snprintf(
            output,
            11U,
            "%s",
            "----------"
        );
        return;
    }

    (void)memcpy(
        output,
        timestamp,
        10U
    );

    output[10] = '\0';
}

static void format_compact_max_weight(
    double value,
    char *output,
    size_t output_size
);

static void exercise_format_performance(
    const TrainlogExercisePerformancePoint *point,
    char *output,
    size_t output_size
)
{
    if (point == NULL ||
        output == NULL ||
        output_size == 0U) {
        return;
    }

    if (point->has_performance == 0) {
        (void)snprintf(
            output,
            output_size,
            "%s",
            "aucune série réussie"
        );
        return;
    }

    if (point->has_explicit_max != 0) {
        char weight[32];
        format_compact_max_weight(point->weight_kg, weight, sizeof(weight));
        (void)snprintf(output, output_size, "%s kg", weight);
        return;
    }

    if (point->tracking_mode ==
        TRAINLOG_TRACKING_DURATION) {
        char duration[64];

        if (trainlog_duration_format(
                point->metric_value,
                duration,
                sizeof(duration)
            ) != TRAINLOG_STATUS_OK) {
            (void)snprintf(
                duration,
                sizeof(duration),
                "%d s",
                point->metric_value
            );
        }

        if (point->load_mode ==
            TRAINLOG_LOAD_EXTERNAL) {
            (void)snprintf(
                output,
                output_size,
                "%.1f kg × %s",
                point->weight_kg,
                duration
            );
        } else if (
            point->load_mode ==
            TRAINLOG_LOAD_ASSISTANCE
        ) {
            (void)snprintf(
                output,
                output_size,
                "%.1f kg aide × %s",
                point->weight_kg,
                duration
            );
        } else {
            (void)snprintf(
                output,
                output_size,
                "%s",
                duration
            );
        }

        return;
    }

    if (point->load_mode ==
        TRAINLOG_LOAD_EXTERNAL) {
        (void)snprintf(
            output,
            output_size,
            "%.1f kg × %d reps",
            point->weight_kg,
            point->metric_value
        );
    } else if (
        point->load_mode ==
        TRAINLOG_LOAD_ASSISTANCE
    ) {
        (void)snprintf(
            output,
            output_size,
            "%.1f kg aide × %d reps",
            point->weight_kg,
            point->metric_value
        );
    } else {
        (void)snprintf(
            output,
            output_size,
            "%d reps",
            point->metric_value
        );
    }
}

static bool exercise_point_better(
    const TrainlogExercisePerformancePoint *candidate,
    const TrainlogExercisePerformancePoint *current
)
{
    if (candidate == NULL ||
        candidate->has_performance == 0) {
        return false;
    }

    if (current == NULL ||
        current->has_performance == 0) {
        return true;
    }

    if (candidate->load_mode !=
        current->load_mode) {
        return false;
    }

    switch (candidate->load_mode) {
    case TRAINLOG_LOAD_EXTERNAL:
        if (candidate->weight_kg >
            current->weight_kg) {
            return true;
        }

        return
            candidate->weight_kg ==
                current->weight_kg &&
            candidate->metric_value >
                current->metric_value;

    case TRAINLOG_LOAD_ASSISTANCE:
        if (candidate->weight_kg <
            current->weight_kg) {
            return true;
        }

        return
            candidate->weight_kg ==
                current->weight_kg &&
            candidate->metric_value >
                current->metric_value;

    case TRAINLOG_LOAD_NONE:
    default:
        return
            candidate->metric_value >
            current->metric_value;
    }
}

static double exercise_graph_value(
    const TrainlogExercisePerformancePoint *point
)
{
    if (point->load_mode ==
        TRAINLOG_LOAD_NONE) {
        return (double)point->metric_value;
    }

    return point->weight_kg;
}

static void draw_exercise_performance_graph(
    const TrainlogExercisePerformancePoint *points,
    size_t count,
    TrainlogLoadMode mode,
    TrainlogTrackingMode tracking_mode,
    int top,
    int height,
    bool measured_max
)
{
    size_t indices[EXERCISE_GRAPH_POINTS];
    size_t selected_count = 0U;
    size_t index;
    double minimum = 0.0;
    double maximum = 0.0;
    const int left = 11;
    int width =
        trainlog_terminal_columns(tui_terminal) - left - 4;

    if (points == NULL ||
        count == 0U ||
        height < 3 ||
        width < 12) {
        return;
    }

    for (index = 0U;
         index < count &&
         selected_count < EXERCISE_GRAPH_POINTS;
         ++index) {
        if (points[index].has_performance != 0 &&
            points[index].load_mode == mode) {
            indices[selected_count] = index;
            ++selected_count;
        }
    }

    if (selected_count == 0U) {
        trainlog_terminal_printf(tui_terminal,
            top + 1,
            4,
            "Aucune performance réussie pour ce mode."
        );
        return;
    }

    minimum =
        exercise_graph_value(
            &points[indices[0]]
        );

    maximum = minimum;

    for (index = 1U;
         index < selected_count;
         ++index) {
        double value =
            exercise_graph_value(
                &points[indices[index]]
            );

        if (value < minimum) {
            minimum = value;
        }

        if (value > maximum) {
            maximum = value;
        }
    }

    if (maximum == minimum) {
        minimum -= 1.0;
        maximum += 1.0;
    }

    if (mode == TRAINLOG_LOAD_ASSISTANCE) {
        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_WARNING
            )
        );

        trainlog_terminal_printf(tui_terminal,
            top,
            left,
            measured_max
                ? "Assistance mesurée (kg) — moins = mieux"
                : "Assistance (kg) — moins = mieux"
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_WARNING
            )
        );
    } else if (mode ==
               TRAINLOG_LOAD_EXTERNAL) {
        trainlog_terminal_printf(tui_terminal,
            top,
            left,
            measured_max
                ? "Max mesuré (kg)"
                : "Charge du meilleur set (kg)"
        );
    } else if (
        tracking_mode ==
        TRAINLOG_TRACKING_DURATION
    ) {
        trainlog_terminal_printf(tui_terminal,
            top,
            left,
            measured_max
                ? "Durée max mesurée"
                : "Meilleure durée"
        );
    } else {
        trainlog_terminal_printf(tui_terminal,
            top,
            left,
            measured_max
                ? "Répétitions max mesurées"
                : "Meilleures répétitions"
        );
    }

    trainlog_terminal_printf(tui_terminal,
        top + 1,
        2,
        "%.1f",
        maximum
    );

    trainlog_terminal_printf(tui_terminal,
        top + height - 1,
        2,
        "%.1f",
        minimum
    );

    trainlog_terminal_style_on(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_GRAPH
        )
    );

    {
        int previous_x = -1;
        int previous_y = -1;
        size_t order;

        for (order = selected_count;
             order > 0U;
             --order) {
            size_t chronological =
                selected_count - order;

            const TrainlogExercisePerformancePoint *point =
                &points[indices[order - 1U]];

            double value =
                exercise_graph_value(point);

            double ratio =
                (value - minimum) /
                (maximum - minimum);

            int x =
                selected_count == 1U
                    ? left + (width / 2)
                    : left +
                        (int)(
                            (chronological *
                             (size_t)(width - 1)) /
                            (selected_count - 1U)
                        );

            int y =
                top +
                height -
                1 -
                (int)(
                    ratio *
                    (double)(height - 1)
                );

            if (previous_x >= 0 &&
                x > previous_x) {
                int line_x;

                for (line_x = previous_x + 1;
                     line_x < x;
                     ++line_x) {
                    int line_y =
                        previous_y +
                        (((y - previous_y) *
                          (line_x - previous_x)) /
                         (x - previous_x));

                    trainlog_terminal_draw(tui_terminal,
                        line_y,
                        line_x,
                        (uint32_t)'.'
                    );
                }
            }

            trainlog_terminal_draw(tui_terminal,
                y,
                x,
                chronological + 1U ==
                    selected_count
                    ? (uint32_t)'O'
                    : (uint32_t)'*'
            );

            previous_x = x;
            previous_y = y;
        }
    }

    trainlog_terminal_style_off(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_GRAPH
        )
    );

    {
        char oldest[11];
        char newest[11];

        exercise_short_date(
            points[indices[selected_count - 1U]]
                .started_at,
            oldest
        );

        exercise_short_date(
            points[indices[0]].started_at,
            newest
        );

        trainlog_terminal_printf(tui_terminal,
            top + height,
            left,
            "%s",
            oldest
        );

        trainlog_terminal_printf(tui_terminal,
            top + height,
            left + width - 10,
            "%s",
            newest
        );
    }
}

/* TRAINLOG_EXERCISE_FRAMES */

static void exercise_panel(
    int top,
    int left,
    int bottom,
    int right,
    const char *label
)
{
    TrainlogPanel *panel;
    int height;
    int width;

    if (top < 0 ||
        left < 0 ||
        bottom <= top ||
        right <= left ||
        bottom >= trainlog_terminal_rows(tui_terminal) ||
        right >= trainlog_terminal_columns(tui_terminal)) {
        return;
    }

    height = bottom - top + 1;
    width = right - left + 1;

    panel = tui_panel_create(tui_terminal, height,
        width,
        top,
        left
    );

    if (panel == NULL) {
        return;
    }

    tui_panel_box(panel);

    if (label != NULL &&
        label[0] != '\0' &&
        width > 8) {
        tui_panel_style_on(
            panel,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        tui_panel_print(
            panel,
            0,
            2,
            " %.*s ",
            width - 6,
            label
        );

        tui_panel_style_off(
            panel,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );
    }

    tui_panel_commit(panel);
    tui_panel_destroy(panel);
}

static void screen_exercise_performance(
    TrainlogDatabase *database,
    const TrainlogExercise *exercise
)
{
    TrainlogExercisePerformancePoint
        points[MAX_SESSIONS];

    size_t count = 0U;
    size_t index;
    size_t history_limit;

    TrainlogLoadMode graph_mode =
        TRAINLOG_LOAD_NONE;

    const TrainlogExercisePerformancePoint *latest =
        NULL;

    const TrainlogExercisePerformancePoint *best =
        NULL;

    char latest_text[128];
    char best_text[128];

    bool decorated =
        trainlog_terminal_columns(tui_terminal) >= 100 &&
        trainlog_terminal_rows(tui_terminal) >= 36;

    int summary_top =
        decorated ? 8 : 3;

    int summary_bottom =
        decorated ? 14 : 9;

    int graph_top =
        decorated ? 15 : 11;

    int graph_bottom =
        decorated ? 25 : 20;

    int history_top =
        decorated ? 26 : 22;

    int history_bottom =
        trainlog_terminal_rows(tui_terminal) - 4;

    if (database == NULL ||
        exercise == NULL) {
        return;
    }

    if (trainlog_database_list_exercise_performance(
            database,
            exercise->exercise_id,
            points,
            MAX_SESSIONS,
            &count
        ) != TRAINLOG_STATUS_OK) {
        draw_shell(
            "Performance exercice",
            "Une touche pour revenir"
        );

        status_line(
            "Impossible de lire l'historique.",
            TRAINLOG_COLOR_ERROR
        );

        wait_key();
        return;
    }

    for (index = 0U;
         index < count;
         ++index) {
        if (points[index].has_performance != 0) {
            latest = &points[index];
            graph_mode =
                points[index].load_mode;
            break;
        }
    }

    if (latest != NULL) {
        for (index = 0U;
             index < count;
             ++index) {
            if (points[index].has_performance != 0 &&
                points[index].load_mode ==
                    graph_mode &&
                exercise_point_better(
                    &points[index],
                    best
                )) {
                best = &points[index];
            }
        }
    }

    exercise_format_performance(
        latest,
        latest_text,
        sizeof(latest_text)
    );

    exercise_format_performance(
        best,
        best_text,
        sizeof(best_text)
    );

    if (decorated) {
        trainlog_terminal_erase(tui_terminal);
        trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

        section_ascii_header(
            ":: P E R F O R M A N C E   E X E R C I C E ::"
        );

        dashboard_panel(
            summary_top,
            2,
            summary_bottom,
            trainlog_terminal_columns(tui_terminal) - 3,
            "PERFORMANCE"
        );

        dashboard_panel(
            graph_top,
            2,
            graph_bottom,
            trainlog_terminal_columns(tui_terminal) - 3,
            "EVOLUTION"
        );

        if (history_bottom >
            history_top + 2) {
            focused_panel(
                history_top,
                2,
                history_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "HISTORIQUE",
                true
            );
        }

        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_printf(tui_terminal,
            trainlog_terminal_rows(tui_terminal) - 2,
            2,
            "%.*s",
            trainlog_terminal_columns(tui_terminal) - 4,
            "b/Échap retour"
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );
    } else {
        draw_shell(
            "TRAINLOG — Performance exercice",
            "b/Échap retour"
        );

        exercise_panel(
            summary_top,
            2,
            summary_bottom,
            trainlog_terminal_columns(tui_terminal) - 3,
            "PERFORMANCE"
        );

        exercise_panel(
            graph_top,
            2,
            graph_bottom,
            trainlog_terminal_columns(tui_terminal) - 3,
            "EVOLUTION"
        );

        if (history_bottom >
            history_top + 2) {
            exercise_panel(
                history_top,
                2,
                history_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "HISTORIQUE"
            );
        }
    }

    trainlog_terminal_style_on(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );

    trainlog_terminal_printf(tui_terminal,
        summary_top + 1,
        5,
        "%s",
        exercise->name
    );

    trainlog_terminal_style_off(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );

    trainlog_terminal_printf(tui_terminal,
        summary_top + 2,
        5,
        "Séances enregistrées : %zu",
        count
    );

    if (latest == NULL) {
        trainlog_terminal_printf(tui_terminal,
            summary_top + 3,
            5,
            "Aucune série réussie enregistrée."
        );
    } else {
        trainlog_terminal_printf(tui_terminal,
            summary_top + 3,
            5,
            "Mode suivi : %s",
            exercise_load_mode_label(
                graph_mode
            )
        );

        trainlog_terminal_printf(tui_terminal,
            summary_top + 4,
            5,
            "Dernier meilleur set : %.*s",
            trainlog_terminal_columns(tui_terminal) - 30,
            latest_text
        );

        trainlog_terminal_printf(tui_terminal,
            summary_top + 5,
            5,
            "Meilleur set enregistré : %.*s",
            trainlog_terminal_columns(tui_terminal) - 33,
            best_text
        );
    }

    if (latest != NULL) {
        int graph_content_top =
            graph_top + 1;

        int graph_height =
            graph_bottom -
            graph_top -
            2;

        draw_exercise_performance_graph(
            points,
            count,
            graph_mode,
            exercise->tracking_mode,
            graph_content_top,
            graph_height,
            false
        );
    }

    if (history_bottom >
        history_top + 2) {
        int history_first_row =
            history_top + 1;

        history_limit =
            history_bottom >
                history_first_row
                ? (size_t)(
                    history_bottom -
                    history_first_row
                )
                : 0U;

        for (index = 0U;
             index < count &&
             index < history_limit;
             ++index) {
            char date[11];
            char summary[128];

            exercise_short_date(
                points[index].started_at,
                date
            );

            exercise_format_performance(
                &points[index],
                summary,
                sizeof(summary)
            );

            trainlog_terminal_printf(tui_terminal,
                history_first_row +
                    (int)index,
                5,
                "%s  %-13s  %.*s",
                date,
                exercise_load_mode_label(
                    points[index].load_mode
                ),
                trainlog_terminal_columns(tui_terminal) - 40,
                summary
            );
        }
    }

    trainlog_terminal_render(tui_terminal);

    for (;;) {
        int key = trainlog_terminal_get_key(tui_terminal);

        if (key == 'b' ||
            key == 'B' ||
            key == 27 ||
            key == '\n' ||
            key == TRAINLOG_KEY_ENTER) {
            return;
        }
    }
}

/* TRAINLOG_MEASURED_MAX_TUI_V1 */

static void screen_exercise_measured_max(
    TrainlogDatabase *database,
    const TrainlogExercise *exercise
)
{
    static const double increments[] = {
        0.5,
        1.0,
        2.5,
        5.0
    };

    static const double percentages[] = {
        60.0,
        70.0,
        80.0,
        90.0
    };

    size_t increment_index = 2U;

    if (
        database == NULL ||
        exercise == NULL
    ) {
        return;
    }

    for (;;) {
        TrainlogExercisePerformancePoint
            points[MAX_SESSIONS];

        TrainlogExercisePerformancePoint
            max_points[MAX_SESSIONS];

        TrainlogMeasuredMaxSummary summary;

        size_t count = 0U;
        size_t max_count = 0U;
        size_t index;
        size_t history_limit;

        bool decorated =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int summary_top =
            decorated ? 8 : 3;

        int summary_bottom =
            decorated ? 15 : 9;

        int graph_top =
            decorated ? 16 : 10;

        int graph_bottom =
            decorated ? 23 : 17;

        int history_top =
            decorated ? 24 : 18;

        int history_bottom =
            trainlog_terminal_rows(tui_terminal) - 4;

        int key;

        if (
            trainlog_database_list_exercise_performance(
                database,
                exercise->exercise_id,
                points,
                MAX_SESSIONS,
                &count
            ) != TRAINLOG_STATUS_OK ||
            trainlog_measured_max_summarize(
                points,
                count,
                &summary
            ) != TRAINLOG_STATUS_OK
        ) {
            draw_shell(
                "TRAINLOG — Max mesuré",
                "Une touche pour revenir"
            );

            status_line(
                "Impossible de lire les tests de max.",
                TRAINLOG_COLOR_ERROR
            );

            wait_key();
            return;
        }

        for (
            index = 0U;
            index < count &&
            max_count < MAX_SESSIONS;
            ++index
        ) {
            if (
                points[index].session_type ==
                TRAINLOG_SESSION_MAX_TEST
            ) {
                max_points[max_count] =
                    points[index];

                ++max_count;
            }
        }

        trainlog_terminal_erase(tui_terminal);
        trainlog_terminal_box(tui_terminal, 0, 0,
                              trainlog_terminal_rows(tui_terminal) - 1,
                              trainlog_terminal_columns(tui_terminal) - 1);

        if (decorated) {
            section_ascii_header(
                ":: M A X   M E S U R E ::"
            );

            dashboard_panel(
                summary_top,
                2,
                summary_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "MAX MESURE"
            );

            dashboard_panel(
                graph_top,
                2,
                graph_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "EVOLUTION DES TESTS MAX"
            );

            if (
                history_bottom >
                history_top + 1
            ) {
                exercise_panel(
                    history_top,
                    2,
                    history_bottom,
                    trainlog_terminal_columns(tui_terminal) - 3,
                    "HISTORIQUE TESTS MAX"
                );
            }

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                "r arrondi charge  b/Échap retour"
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Max mesuré",
                "r arrondi charge  b/Échap retour"
            );

            exercise_panel(
                summary_top,
                2,
                summary_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "MAX MESURE"
            );

            exercise_panel(
                graph_top,
                2,
                graph_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "EVOLUTION"
            );

            if (
                history_bottom >
                history_top + 1
            ) {
                exercise_panel(
                    history_top,
                    2,
                    history_bottom,
                    trainlog_terminal_columns(tui_terminal) - 3,
                    "HISTORIQUE"
                );
            }
        }

        trainlog_terminal_style_on(tui_terminal,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        trainlog_terminal_printf(tui_terminal,
            summary_top + 1,
            5,
            "%s",
            exercise->name
        );

        trainlog_terminal_style_off(tui_terminal,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        trainlog_terminal_printf(tui_terminal,
            summary_top + 2,
            5,
            "Tests max : %zu · réussis : %zu",
            summary.test_count,
            summary.successful_test_count
        );

        if (!summary.found) {
            trainlog_terminal_printf(tui_terminal,
                summary_top + 3,
                5,
                "Aucun max mesuré réussi."
            );

            trainlog_terminal_printf(tui_terminal,
                summary_top + 4,
                5,
                "Seules les séances explicitement « Test de max » comptent."
            );
        } else {
            char current_text[128];
            char record_text[128];
            char current_date[11];
            char record_date[11];
            TrainlogResolvedEquipment current_equipment;
            const char *current_equipment_label = "aucun";

            exercise_format_performance(
                &summary.current,
                current_text,
                sizeof(current_text)
            );

            exercise_format_performance(
                &summary.record,
                record_text,
                sizeof(record_text)
            );

            exercise_short_date(
                summary.current.started_at,
                current_date
            );

            exercise_short_date(
                summary.record.started_at,
                record_date
            );

            if (summary.current.equipment_id[0] != '\0') {
                current_equipment_label =
                    trainlog_database_resolve_equipment(database,
                        summary.current.equipment_id, &current_equipment) ==
                        TRAINLOG_STATUS_OK
                    ? current_equipment.display_name
                    : summary.current.equipment_id;
            }

            trainlog_terminal_printf(tui_terminal,
                summary_top + 3,
                5,
                "Actuel : %s · %s · Machine : %.*s",
                current_date,
                current_text,
                trainlog_terminal_columns(tui_terminal) - 48,
                current_equipment_label
            );

            trainlog_terminal_printf(tui_terminal,
                summary_top + 4,
                5,
                "Record même mode : %s · %.*s",
                record_date,
                trainlog_terminal_columns(tui_terminal) - 42,
                record_text
            );

            if (
                summary.current.load_mode ==
                TRAINLOG_LOAD_EXTERNAL
            ) {
                double working[4];
                bool valid = true;

                for (
                    index = 0U;
                    index < 4U;
                    ++index
                ) {
                    if (
                        trainlog_measured_max_working_load(
                            &summary.current,
                            percentages[index],
                            increments[increment_index],
                            &working[index]
                        ) != TRAINLOG_STATUS_OK
                    ) {
                        valid = false;
                        break;
                    }
                }

                if (valid) {
                    trainlog_terminal_printf(tui_terminal,
                        summary_top + 5,
                        5,
                        "Travail : 60%% %.1f · 70%% %.1f · 80%% %.1f · 90%% %.1f kg",
                        working[0],
                        working[1],
                        working[2],
                        working[3]
                    );

                    trainlog_terminal_printf(tui_terminal,
                        summary_top + 6,
                        5,
                        "Arrondi : %.1f kg (r pour changer) · aucun 1RM estimé",
                        increments[increment_index]
                    );
                }
            } else if (
                summary.current.load_mode ==
                TRAINLOG_LOAD_ASSISTANCE
            ) {
                trainlog_terminal_style_on(tui_terminal,
                    trainlog_theme_style(
                        TRAINLOG_COLOR_WARNING
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    summary_top + 5,
                    5,
                    "Assistance : moins de kg = mieux."
                );

                trainlog_terminal_printf(tui_terminal,
                    summary_top + 6,
                    5,
                    "Pourcentages de charge non applicables à l'assistance."
                );

                trainlog_terminal_style_off(tui_terminal,
                    trainlog_theme_style(
                        TRAINLOG_COLOR_WARNING
                    )
                );
            } else {
                trainlog_terminal_printf(tui_terminal,
                    summary_top + 5,
                    5,
                    "Sans charge externe : pourcentages non applicables."
                );

                trainlog_terminal_printf(tui_terminal,
                    summary_top + 6,
                    5,
                    "Le max reste une valeur réellement réalisée, jamais estimée."
                );
            }
        }

        if (
            summary.found &&
            max_count > 0U
        ) {
            int graph_content_top =
                graph_top + 1;

            int graph_height =
                graph_bottom -
                graph_top -
                2;

            draw_exercise_performance_graph(
                max_points,
                max_count,
                summary.current.load_mode,
                exercise->tracking_mode,
                graph_content_top,
                graph_height,
                true
            );
        } else {
            trainlog_terminal_printf(tui_terminal,
                graph_top + 2,
                5,
                "Aucun point de max mesuré à tracer."
            );
        }

        if (
            history_bottom >
            history_top + 1
        ) {
            int first_row =
                history_top + 1;

            history_limit =
                history_bottom >
                    first_row
                    ? (size_t)(
                        history_bottom -
                        first_row
                    )
                    : 0U;

            for (
                index = 0U;
                index < max_count &&
                index < history_limit;
                ++index
            ) {
                char date[11];
                char text[128];

                exercise_short_date(
                    max_points[index].started_at,
                    date
                );

                exercise_format_performance(
                    &max_points[index],
                    text,
                    sizeof(text)
                );

                trainlog_terminal_printf(tui_terminal,
                    first_row +
                        (int)index,
                    5,
                    "%s  %-13s  %.*s",
                    date,
                    exercise_load_mode_label(
                        max_points[index]
                            .load_mode
                    ),
                    trainlog_terminal_columns(tui_terminal) - 40,
                    text
                );
            }
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (
            key == 'b' ||
            key == 'B' ||
            key == 27 ||
            key == '\n' ||
            key == TRAINLOG_KEY_ENTER
        ) {
            return;
        }

        if (
            key == 'r' ||
            key == 'R'
        ) {
            increment_index =
                (
                    increment_index + 1U
                ) %
                (
                    sizeof(increments) /
                    sizeof(increments[0])
                );
        }
    }
}

/* TRAINLOG_SECTION_ASCII_HEADER */

static void section_ascii_header(
    const char *subtitle
)
{
    int header_left = 2;
    int header_right = trainlog_terminal_columns(tui_terminal) - 3;
    int subtitle_width;

    if (header_right - header_left < 20) {
        return;
    }

    /* WHY: one restrained three-row plaque provides a recognizable terminal
     * identity without consuming the panel rows reserved by every screen. */
    trainlog_terminal_style_on(tui_terminal,
        trainlog_theme_style(TRAINLOG_COLOR_MUTED));
    trainlog_terminal_box(tui_terminal, 1, header_left, 3, header_right);
    trainlog_terminal_style_off(tui_terminal,
        trainlog_theme_style(TRAINLOG_COLOR_MUTED));

    trainlog_terminal_style_on(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );

    trainlog_terminal_printf(tui_terminal, 1, header_left + 3,
                             "◆ TRAINLOG ◆");

    trainlog_terminal_style_off(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );

    if (subtitle != NULL && subtitle[0] != '\0') {
        subtitle_width = header_right - header_left - 4;

        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_printf(tui_terminal,
            2,
            header_left + 2,
            "%.*s",
            subtitle_width,
            subtitle
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );
    }
}

static void section_scrollbar(
    int top,
    int bottom,
    int column,
    size_t selected,
    size_t count,
    size_t visible
)
{
    int track_height;
    int row;
    int thumb;

    if (bottom <= top ||
        count <= visible ||
        count <= 1U) {
        return;
    }

    track_height =
        bottom - top + 1;

    if (track_height < 2) {
        return;
    }

    trainlog_terminal_style_on(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_MUTED
        )
    );

    for (row = top;
         row <= bottom;
         ++row) {
        trainlog_terminal_draw(tui_terminal,
            row,
            column,
            0x2502U
        );
    }

    trainlog_terminal_style_off(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_MUTED
        )
    );

    thumb =
        top +
        (int)(
            (selected *
             (size_t)(track_height - 1)) /
            (count - 1U)
        );

    trainlog_terminal_style_on(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );

    trainlog_terminal_draw(tui_terminal,
        thumb,
        column,
        0x2593U
    );

    trainlog_terminal_style_off(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );
}

static void screen_exercise_detail(
    TrainlogDatabase *database,
    const TrainlogExercise *exercise
)
{
    TrainlogResolvedEquipment explicit_items[64];
    TrainlogResolvedEquipment historic_items[128];
    size_t explicit_count = 0U;
    size_t historic_count = 0U;
    size_t selected = 0U;
    size_t index;

    if (database == NULL || exercise == NULL) return;
    for (index = 0U; index < trainlog_equipment_catalog_relation_count() &&
            explicit_count < 64U; ++index) {
        const TrainlogExerciseEquipmentRelation *relation =
            trainlog_equipment_catalog_relation_at(index);
        if (relation != NULL && strcmp(relation->exercise_id,
                exercise->exercise_id) == 0 &&
            trainlog_database_resolve_equipment(database, relation->equipment_id,
                &explicit_items[explicit_count]) == TRAINLOG_STATUS_OK) {
            ++explicit_count;
        }
    }
    if (trainlog_database_list_exercise_equipment(database,
            exercise->exercise_id, historic_items, 128U,
            &historic_count) != TRAINLOG_STATUS_OK) historic_count = 0U;
    if (historic_count > 128U) historic_count = 128U;

    for (;;) {
        size_t total = explicit_count + historic_count;
        int row = 7;
        int key;
        if (total > 0U && selected >= total) selected = total - 1U;
        draw_shell("TRAINLOG — Fiche exercice",
            "↑↓ équipement  Entrée fiche  p performance  m max mesuré  b/Échap retour");
        trainlog_terminal_printf(tui_terminal, 3, 4, "%s", exercise->name);
        trainlog_terminal_printf(tui_terminal, 4, 4, "Identifiant : %s · suivi : %s",
            exercise->exercise_id,
            exercise->tracking_mode == TRAINLOG_TRACKING_REPS ? "répétitions" : "durée");
        trainlog_terminal_printf(tui_terminal, 6, 4,
            "Relations explicites du manifeste (%zu)", explicit_count);
        if (explicit_count == 0U) trainlog_terminal_printf(tui_terminal, row++, 6, "— aucune");
        for (index = 0U; index < explicit_count; ++index, ++row) {
            if (selected == index) trainlog_terminal_style_on(tui_terminal, TRAINLOG_TEXT_REVERSE);
            trainlog_terminal_printf(tui_terminal, row, 6, " %-58.58s ", explicit_items[index].display_name);
            if (selected == index) trainlog_terminal_style_off(tui_terminal, TRAINLOG_TEXT_REVERSE);
        }
        ++row;
        trainlog_terminal_printf(tui_terminal, row++, 4,
            "Équipements utilisés historiquement (%zu)", historic_count);
        if (historic_count == 0U) trainlog_terminal_printf(tui_terminal, row++, 6, "— aucun");
        for (index = 0U; index < historic_count && row < trainlog_terminal_rows(tui_terminal) - 3;
                ++index, ++row) {
            size_t absolute = explicit_count + index;
            if (selected == absolute) trainlog_terminal_style_on(tui_terminal, TRAINLOG_TEXT_REVERSE);
            trainlog_terminal_printf(tui_terminal, row, 6, " %-58.58s [%s] ",
                historic_items[index].display_name,
                equipment_origin_label(historic_items[index].origin));
            if (selected == absolute) trainlog_terminal_style_off(tui_terminal, TRAINLOG_TEXT_REVERSE);
        }
        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);
        if (key == 27 || key == 'b' || key == 'B') return;
        if (total > 0U && key == TRAINLOG_KEY_UP)
            selected = selected > 0U ? selected - 1U : total - 1U;
        else if (total > 0U && key == TRAINLOG_KEY_DOWN)
            selected = selected + 1U < total ? selected + 1U : 0U;
        else if (total > 0U && (key == '\n' || key == TRAINLOG_KEY_ENTER))
            screen_equipment_detail(selected < explicit_count
                ? &explicit_items[selected] : &historic_items[selected - explicit_count]);
        else if (key == 'p' || key == 'P') screen_exercise_performance(database, exercise);
        else if (key == 'm' || key == 'M') screen_exercise_measured_max(database, exercise);
    }
}

static void screen_exercises(
    TrainlogDatabase *database
)
{
    TrainlogExercise exercises[MAX_EXERCISES];
    size_t selected = 0U;
    int nav_selected = 3;
    int focus = 1;

    for (;;) {
        size_t count = 0U;
        size_t top = 0U;
        size_t index;

        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        bool framed =
            trainlog_terminal_columns(tui_terminal) >= 90 &&
            trainlog_terminal_rows(tui_terminal) >= 24;

        int list_top =
            large_layout
                ? 11
                : 3;

        int list_bottom =
            trainlog_terminal_rows(tui_terminal) - 4;

        int first_row =
            list_top + 1;

        int visible_rows =
            framed
                ? list_bottom -
                    first_row
                : trainlog_terminal_rows(tui_terminal) - 7;

        int key;

        if (visible_rows < 1) {
            return;
        }

        if (trainlog_database_list_exercises(
                database,
                exercises,
                MAX_EXERCISES,
                &count
            ) != TRAINLOG_STATUS_OK) {
            return;
        }

        if (count > 0U &&
            selected >= count) {
            selected =
                count - 1U;
        }

        if (count > 0U &&
            selected >=
                (size_t)visible_rows) {
            top =
                selected -
                (size_t)visible_rows +
                1U;
        }

        if (large_layout) {
            trainlog_terminal_erase(tui_terminal);
            trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

            section_ascii_header(
                ":: E X E R C I C E S ::"
            );

            primary_top_navbar(
                3,
                nav_selected,
                focus == 0
            );

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                "Tab zone  ↑↓/PgUp/PgDn catalogue  ←→ menu  Entrée ouvrir  m max mesuré  a ajouter  0/Home accueil  F1-F5 direct  b/Échap retour"
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Exercices",
                "↑↓ naviguer  Entrée performance  m max mesuré  a ajouter  b/Échap retour"
            );
        }

        if (framed) {
            if (large_layout) {
                focused_panel(
                    list_top,
                    2,
                    list_bottom,
                    trainlog_terminal_columns(tui_terminal) - 3,
                    "CATALOGUE",
                    focus == 1
                );
            } else {
                exercise_panel(
                    list_top,
                    2,
                    list_bottom,
                    trainlog_terminal_columns(tui_terminal) - 3,
                    "CATALOGUE"
                );
            }
        }

        if (count == 0U) {
            trainlog_terminal_printf(tui_terminal,
                first_row + 1,
                framed ? 5 : 4,
                "Aucun exercice."
            );
        }

        for (index = 0U;
             index < (size_t)visible_rows &&
             top + index < count;
             ++index) {
            size_t absolute =
                top + index;

            int item_row =
                first_row +
                (int)index;

            int item_col =
                framed ? 5 : 4;

            if (focus == 1 &&
                absolute == selected) {
                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_printf(tui_terminal,
                item_row,
                item_col,
                " %-42s [%s] ",
                exercises[absolute].name,
                exercises[absolute].tracking_mode ==
                    TRAINLOG_TRACKING_REPS
                    ? "reps"
                    : "durée"
            );

            if (focus == 1 &&
                absolute == selected) {
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        if (large_layout) {
            section_scrollbar(
                first_row,
                list_bottom - 1,
                trainlog_terminal_columns(tui_terminal) - 5,
                selected,
                count,
                (size_t)visible_rows
            );
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (large_layout &&
            (key == TRAINLOG_KEY_TAB ||
             key == TRAINLOG_KEY_SHIFT_TAB)) {
            focus =
                focus == 0
                    ? 1
                    : 0;
            continue;
        }

        if (large_layout &&
primary_top_nav_forward(key)) {
            return;
        }

        if (key == 'b' ||
            key == 'B' ||
            key == 27) {
            return;
        }

        if (large_layout &&
            focus == 0) {
            if (key == TRAINLOG_KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : PRIMARY_NAV_COUNT - 1;
            } else if (key == TRAINLOG_KEY_RIGHT) {
                nav_selected =
                    nav_selected < PRIMARY_NAV_COUNT - 1
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == TRAINLOG_KEY_ENTER
            ) {
if (primary_top_nav_activate(
                        nav_selected
                    )) {
                    return;
                }
            }

            continue;
        }

        if (count > 0U &&
            key == TRAINLOG_KEY_UP) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : 0U;

            continue;
        }

        if (count > 0U &&
            key == TRAINLOG_KEY_DOWN) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : count - 1U;

            continue;
        }

        if (count > 0U &&
            key == TRAINLOG_KEY_PAGE_UP) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected > jump
                    ? selected - jump
                    : 0U;

            continue;
        }

        if (count > 0U &&
            key == TRAINLOG_KEY_PAGE_DOWN) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected + jump < count
                    ? selected + jump
                    : count - 1U;

            continue;
        }

        if (count > 0U &&
            (key == 'm' ||
             key == 'M')) {
            screen_exercise_measured_max(
                database,
                &exercises[selected]
            );

            continue;
        }

        if (count > 0U &&
            (key == '\n' ||
             key == TRAINLOG_KEY_ENTER)) {
            screen_exercise_detail(
                database,
                &exercises[selected]
            );

            continue;
        }

        if (count > 0U && (key == 'p' || key == 'P')) {
            screen_exercise_performance(database, &exercises[selected]);
            continue;
        }

        if (key == 'a' ||
            key == 'A') {
            char name[TRAINLOG_NAME_MAX + 1U];
                        int mode = 1;
            int organization = 1;
            int wants_speed = 0;
            int wants_distance = 0;
            TrainlogExerciseDataFields data_fields = 0U;
            TrainlogExercise created;
            TrainlogStatus status;

            draw_shell(
                "Nouvel exercice",
                "Entrée valide chaque champ · Échap annule"
            );

            if (!prompt_text(
                    4,
                    "Nom : ",
                    name,
                    sizeof(name),
                    false
                )) {
                continue;
            }

            if (!prompt_int_value(
                    5,
                    "Mode 1=reps 2=durée",
                    1,
                    2,
                    1,
                    &mode
                )) {
                continue;
            }

                        if (!prompt_int_value(
                    8,
                    "Organisation (1 séries, 2 continu)",
                    1,
                    2,
                    1,
                    &organization
                )) {
                continue;
            }

            if (organization == 2) {
                mode = 2;

                if (!prompt_int_value(
                        10,
                        "Mesurer la vitesse km/h ? (0 non, 1 oui)",
                        0,
                        1,
                        1,
                        &wants_speed
                    )) {
                    continue;
                }

                if (!prompt_int_value(
                        12,
                        "Mesurer la distance km ? (0 non, 1 oui)",
                        0,
                        1,
                        0,
                        &wants_distance
                    )) {
                    continue;
                }

                if (wants_speed != 0) {
                    data_fields |=
                        TRAINLOG_EXERCISE_DATA_SPEED_KMH;
                }

                if (wants_distance != 0) {
                    data_fields |=
                        TRAINLOG_EXERCISE_DATA_DISTANCE_KM;
                }
            }
status =
                trainlog_catalog_create_exercise_profiled(
                    database,
                    name,
                    mode == 1
                        ? TRAINLOG_TRACKING_REPS
                        : TRAINLOG_TRACKING_DURATION,
                    organization == 2
                            ? TRAINLOG_RECORDING_CONTINUOUS
                            : TRAINLOG_RECORDING_SETS,
                        data_fields,
                        &created
                );

            if (status ==
                TRAINLOG_STATUS_OK) {
                status_line(
                    "✓ Exercice ajouté.",
                    TRAINLOG_COLOR_SUCCESS
                );
            } else if (
                status ==
                TRAINLOG_STATUS_CONFLICT
            ) {
                status_line(
                    "Doublon détecté.",
                    TRAINLOG_COLOR_WARNING
                );
            } else {
                status_line(
                    "Impossible d'ajouter l'exercice.",
                    TRAINLOG_COLOR_ERROR
                );
            }

            wait_key();
        }
    }
}

static bool create_exercise_inline(
    TrainlogDatabase *database
)
{
    char name[TRAINLOG_NAME_MAX + 1U];
        int mode = 1;
    int organization = 1;
    int wants_speed = 0;
    int wants_distance = 0;
    TrainlogExerciseDataFields data_fields = 0U;
    TrainlogExercise created;
    TrainlogStatus status;

    draw_shell(
        "Nouvel exercice",
        "Entrée valide chaque champ · Échap annule"
    );

    if (!prompt_text(
            4,
            "Nom : ",
            name,
            sizeof(name),
            false
        )) {
        return false;
    }

    if (!prompt_int_value(
            5,
            "Mode 1=reps 2=durée",
            1,
            2,
            1,
            &mode
        )) {
        return false;
    }

        if (!prompt_int_value(
            8,
            "Organisation (1 séries, 2 continu)",
            1,
            2,
            1,
            &organization
        )) {
        return false;
    }

    if (organization == 2) {
        mode = 2;

        if (!prompt_int_value(
                10,
                "Mesurer la vitesse km/h ? (0 non, 1 oui)",
                0,
                1,
                1,
                &wants_speed
            )) {
            return false;
        }

        if (!prompt_int_value(
                12,
                "Mesurer la distance km ? (0 non, 1 oui)",
                0,
                1,
                0,
                &wants_distance
            )) {
            return false;
        }

        if (wants_speed != 0) {
            data_fields |=
                TRAINLOG_EXERCISE_DATA_SPEED_KMH;
        }

        if (wants_distance != 0) {
            data_fields |=
                TRAINLOG_EXERCISE_DATA_DISTANCE_KM;
        }
    }
status =
        trainlog_catalog_create_exercise_profiled(
            database,
            name,
            mode == 1
                ? TRAINLOG_TRACKING_REPS
                : TRAINLOG_TRACKING_DURATION,
            organization == 2
                    ? TRAINLOG_RECORDING_CONTINUOUS
                    : TRAINLOG_RECORDING_SETS,
                data_fields,
                &created
        );

    if (status == TRAINLOG_STATUS_OK) {
        status_line(
            "✓ Exercice ajouté au catalogue.",
            TRAINLOG_COLOR_SUCCESS
        );
    } else if (
        status == TRAINLOG_STATUS_CONFLICT
    ) {
        status_line(
            "Doublon détecté.",
            TRAINLOG_COLOR_WARNING
        );
    } else {
        status_line(
            "Impossible d'ajouter l'exercice.",
            TRAINLOG_COLOR_ERROR
        );
    }

    wait_key();

    return status == TRAINLOG_STATUS_OK;
}

static bool choose_exercise(
    TrainlogDatabase *database,
    TrainlogExercise *output
)
{
    TrainlogExercise exercises[MAX_EXERCISES];
    size_t selected = 0U;

    for (;;) {
        size_t count = 0U;
        size_t top = 0U;
        size_t index;
        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;
        int list_top =
            large_layout ? 8 : 3;
        int list_bottom =
            trainlog_terminal_rows(tui_terminal) - 4;
        int first_row =
            list_top + 1;
        int visible_rows =
            list_bottom - first_row;
        int key;

        if (visible_rows < 1) {
            return false;
        }

        if (trainlog_database_list_exercises(
                database,
                exercises,
                MAX_EXERCISES,
                &count
            ) != TRAINLOG_STATUS_OK) {
            return false;
        }

        if (count == 0U) {
            draw_shell(
                "Choisir un exercice",
                "a ajouter un exercice · Échap annuler"
            );

            trainlog_terminal_printf(tui_terminal,
                4,
                4,
                "Aucun exercice disponible."
            );

            trainlog_terminal_render(tui_terminal);
            key = trainlog_terminal_get_key(tui_terminal);

            if (key == 'a' ||
                key == 'A') {
                (void)create_exercise_inline(
                    database
                );
                continue;
            }

            if (key == 27) {
                return false;
            }

            continue;
        }

        if (selected >= count) {
            selected =
                count - 1U;
        }

        if (selected >=
            (size_t)visible_rows) {
            top =
                selected -
                (size_t)visible_rows +
                1U;
        }

        if (large_layout) {
            trainlog_terminal_erase(tui_terminal);
            trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

            section_ascii_header(
                ":: C H O I S I R   E X E R C I C E ::"
            );

            focused_panel(
                list_top,
                2,
                list_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "CATALOGUE",
                true
            );

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                "↑↓ choisir  Entrée sélectionner  a créer un exercice  Échap annuler"
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "Choisir un exercice",
                "↑↓ naviguer  Entrée choisir  a ajouter  Échap annuler"
            );
        }

        for (index = 0U;
             index < (size_t)visible_rows &&
             top + index < count;
             ++index) {
            size_t absolute =
                top + index;

            int item_row =
                first_row +
                (int)index;

            if (absolute == selected) {
                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_printf(tui_terminal,
                item_row,
                large_layout ? 5 : 4,
                " %-42s [%s] ",
                exercises[absolute].name,
                exercises[absolute].tracking_mode ==
                    TRAINLOG_TRACKING_REPS
                    ? "reps"
                    : "durée"
            );

            if (absolute == selected) {
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        if (large_layout) {
            section_scrollbar(
                first_row,
                list_bottom - 1,
                trainlog_terminal_columns(tui_terminal) - 5,
                selected,
                count,
                (size_t)visible_rows
            );
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (key == TRAINLOG_KEY_UP) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : count - 1U;
        } else if (key == TRAINLOG_KEY_DOWN) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : 0U;
        } else if (
            key == '\n' ||
            key == TRAINLOG_KEY_ENTER
        ) {
            *output =
                exercises[selected];

            return true;
        } else if (
            key == 'a' ||
            key == 'A'
        ) {
            (void)create_exercise_inline(
                database
            );
        } else if (key == 27) {
            return false;
        }
    }
}

/* TRAINLOG_DURATION_BODY_GRAPH_HELPERS */

typedef struct TrainlogBodyMetricView {
    TrainlogBodyMetric metric;
    const char *label;
    const char *unit;
} TrainlogBodyMetricView;

static const TrainlogBodyMetricView BODY_METRICS[] = {
    {TRAINLOG_BODY_METRIC_WEIGHT, "Poids", "kg"},
    {TRAINLOG_BODY_METRIC_NECK, "Cou", "cm"},
    {TRAINLOG_BODY_METRIC_SHOULDERS, "Épaules", "cm"},
    {TRAINLOG_BODY_METRIC_CHEST, "Poitrine", "cm"},
    {TRAINLOG_BODY_METRIC_WAIST, "Tour de taille", "cm"},
    {TRAINLOG_BODY_METRIC_HIPS, "Hanches", "cm"},
    {TRAINLOG_BODY_METRIC_LEFT_ARM, "Bras gauche", "cm"},
    {TRAINLOG_BODY_METRIC_RIGHT_ARM, "Bras droit", "cm"},
    {TRAINLOG_BODY_METRIC_LEFT_FOREARM, "Avant-bras gauche", "cm"},
    {TRAINLOG_BODY_METRIC_RIGHT_FOREARM, "Avant-bras droit", "cm"},
    {TRAINLOG_BODY_METRIC_LEFT_THIGH, "Cuisse gauche", "cm"},
    {TRAINLOG_BODY_METRIC_RIGHT_THIGH, "Cuisse droite", "cm"},
    {TRAINLOG_BODY_METRIC_LEFT_CALF, "Mollet gauche", "cm"},
    {TRAINLOG_BODY_METRIC_RIGHT_CALF, "Mollet droit", "cm"}
};

static bool prompt_duration_value(
    int row,
    const char *label,
    int minimum_seconds,
    int maximum_seconds,
    int default_seconds,
    int *output_seconds
)
{
    char buffer[64];
    char default_text[64];
    char decorated[192];

    if (trainlog_duration_format(
            default_seconds,
            default_text,
            sizeof(default_text)
        ) != TRAINLOG_STATUS_OK) {
        return false;
    }

    for (;;) {
        int parsed;

        (void)snprintf(
            decorated,
            sizeof(decorated),
            "%s [%s]: ",
            label,
            default_text
        );

        if (!prompt_text(
                row,
                decorated,
                buffer,
                sizeof(buffer),
                true
            )) {
            return false;
        }

        if (buffer[0] == '\0') {
            *output_seconds = default_seconds;
            return true;
        }

        if (trainlog_duration_parse(buffer, &parsed) ==
                TRAINLOG_STATUS_OK &&
            parsed >= minimum_seconds &&
            parsed <= maximum_seconds) {
            *output_seconds = parsed;
            return true;
        }

        status_line(
            "Durée invalide : 90, 90s, 1:30, 1m30, 2m.",
            TRAINLOG_COLOR_ERROR
        );
        trainlog_terminal_render(tui_terminal);
    }
}

static void draw_body_metric_graph(
    int row,
    int height,
    const TrainlogBodyMetricPoint *points,
    size_t count,
    const char *unit
)
{
    double minimum;
    double maximum;
    size_t start;
    size_t index;
    int width;

    if (count == 0U) {
        trainlog_terminal_printf(tui_terminal, row, 4, "Aucune donnée pour cette mesure.");
        return;
    }

    width = trainlog_terminal_columns(tui_terminal) - 14;
    if (width < 10 || height < 3) {
        return;
    }

    start = count > (size_t)width
        ? count - (size_t)width
        : 0U;

    minimum = points[start].value;
    maximum = points[start].value;

    for (index = start + 1U; index < count; ++index) {
        if (points[index].value < minimum) {
            minimum = points[index].value;
        }
        if (points[index].value > maximum) {
            maximum = points[index].value;
        }
    }

    if (maximum == minimum) {
        int graph_row = row + (height / 2);

        trainlog_terminal_printf(tui_terminal, graph_row, 2, "%.1f %s", minimum, unit);

        trainlog_terminal_style_on(tui_terminal, trainlog_theme_style(TRAINLOG_COLOR_GRAPH));

        for (index = start; index < count; ++index) {
            int x = 12 + (int)(index - start);

            if (x < trainlog_terminal_columns(tui_terminal) - 2) {
                trainlog_terminal_draw(tui_terminal, graph_row, x, (uint32_t)'*');
            }
        }

        trainlog_terminal_style_off(tui_terminal, trainlog_theme_style(TRAINLOG_COLOR_GRAPH));
        return;
    }

    trainlog_terminal_printf(tui_terminal, row, 2, "%.1f", maximum);
    trainlog_terminal_printf(tui_terminal, row + height - 1, 2, "%.1f", minimum);

    trainlog_terminal_style_on(tui_terminal, trainlog_theme_style(TRAINLOG_COLOR_GRAPH));

    for (index = start; index < count; ++index) {
        double ratio =
            (points[index].value - minimum) /
            (maximum - minimum);

        int y = row + height - 1 -
            (int)(ratio * (double)(height - 1));

        int x = 10 + (int)(index - start);

        if (y < row) {
            y = row;
        }
        if (y > row + height - 1) {
            y = row + height - 1;
        }

        if (x < trainlog_terminal_columns(tui_terminal) - 2) {
            trainlog_terminal_draw(tui_terminal, y, x, (uint32_t)'*');
        }
    }

    trainlog_terminal_style_off(tui_terminal, trainlog_theme_style(TRAINLOG_COLOR_GRAPH));
}

static void add_body_observation(TrainlogDatabase *database)
{
    TrainlogBodyObservationInput observation;
    char id[TRAINLOG_GENERATED_ID_CAPACITY];
    char timestamp[TRAINLOG_TIMESTAMP_MAX + 1U];
    int row;

    (void)memset(&observation, 0, sizeof(observation));

    if (trainlog_id_generate("bo", id, sizeof(id)) != TRAINLOG_STATUS_OK ||
        trainlog_time_now_rfc3339(
            timestamp,
            sizeof(timestamp)
        ) != TRAINLOG_STATUS_OK) {
        return;
    }

    (void)snprintf(
        observation.observation_id,
        sizeof(observation.observation_id),
        "%s",
        id
    );

    (void)snprintf(
        observation.observed_at,
        sizeof(observation.observed_at),
        "%s",
        timestamp
    );

    draw_shell(
        "Nouvelle mesure — 1/2",
        "Entrée valider · Échap annuler · vide = mesure non faite"
    );

    row = 4;

#define BODY_PROMPT(label_, flag_, value_)                                   \
    do {                                                                     \
        if (!prompt_optional_double(                                         \
                row++,                                                       \
                (label_),                                                    \
                &(flag_),                                                    \
                &(value_)                                                    \
            )) {                                                             \
            status_line(                                                     \
                "Saisie annulée : aucune mesure enregistrée.",              \
                TRAINLOG_COLOR_MUTED                                         \
            );                                                               \
            wait_key();                                                      \
            return;                                                          \
        }                                                                    \
    } while (0)

    BODY_PROMPT(
        "Poids kg : ",
        observation.has_body_weight,
        observation.body_weight_kg
    );
    BODY_PROMPT(
        "Cou cm : ",
        observation.has_neck,
        observation.neck_cm
    );
    BODY_PROMPT(
        "Épaules cm : ",
        observation.has_shoulders,
        observation.shoulders_cm
    );
    BODY_PROMPT(
        "Poitrine cm : ",
        observation.has_chest,
        observation.chest_cm
    );
    BODY_PROMPT(
        "Tour de taille cm : ",
        observation.has_waist,
        observation.waist_cm
    );
    BODY_PROMPT(
        "Hanches cm : ",
        observation.has_hips,
        observation.hips_cm
    );

    draw_shell(
        "Nouvelle mesure — 2/2",
        "Entrée valider · Échap annuler · vide = mesure non faite"
    );

    row = 4;

    BODY_PROMPT(
        "Bras gauche cm : ",
        observation.has_left_arm,
        observation.left_arm_cm
    );
    BODY_PROMPT(
        "Bras droit cm : ",
        observation.has_right_arm,
        observation.right_arm_cm
    );
    BODY_PROMPT(
        "Avant-bras gauche cm : ",
        observation.has_left_forearm,
        observation.left_forearm_cm
    );
    BODY_PROMPT(
        "Avant-bras droit cm : ",
        observation.has_right_forearm,
        observation.right_forearm_cm
    );
    BODY_PROMPT(
        "Cuisse gauche cm : ",
        observation.has_left_thigh,
        observation.left_thigh_cm
    );
    BODY_PROMPT(
        "Cuisse droite cm : ",
        observation.has_right_thigh,
        observation.right_thigh_cm
    );
    BODY_PROMPT(
        "Mollet gauche cm : ",
        observation.has_left_calf,
        observation.left_calf_cm
    );
    BODY_PROMPT(
        "Mollet droit cm : ",
        observation.has_right_calf,
        observation.right_calf_cm
    );

#undef BODY_PROMPT

    if (trainlog_database_insert_body_observation(
            database,
            &observation
        ) == TRAINLOG_STATUS_OK) {
        status_line(
            "✓ Mesures enregistrées.",
            TRAINLOG_COLOR_SUCCESS
        );
    } else {
        status_line(
            "Aucune mesure valide enregistrée.",
            TRAINLOG_COLOR_WARNING
        );
    }

    wait_key();
}

/* TRAINLOG_GLOBAL_BODY_OVERLAY_HELPERS */

#define MAX_GLOBAL_BODY_DATES \
    (MAX_BODY_METRIC_POINTS * 14U)

typedef struct TrainlogGlobalBodySeries {
    TrainlogBodyMetricPoint points[MAX_BODY_METRIC_POINTS];
    size_t count;
    double baseline;
    double latest_percent;
    char symbol;
    TrainlogColorRole role;
} TrainlogGlobalBodySeries;

static const char GLOBAL_BODY_SYMBOLS[] = {
    'P', 'N', 'E', 'C', 'T', 'H', 'A',
    'B', 'F', 'G', 'Q', 'R', 'M', 'D'
};

static const TrainlogColorRole GLOBAL_BODY_ROLES[] = {
    TRAINLOG_COLOR_ACCENT,
    TRAINLOG_COLOR_SUCCESS,
    TRAINLOG_COLOR_WARNING,
    TRAINLOG_COLOR_ERROR,
    TRAINLOG_COLOR_GRAPH,
    TRAINLOG_COLOR_MUTED,
    TRAINLOG_COLOR_ACCENT,
    TRAINLOG_COLOR_SUCCESS,
    TRAINLOG_COLOR_WARNING,
    TRAINLOG_COLOR_ERROR,
    TRAINLOG_COLOR_GRAPH,
    TRAINLOG_COLOR_MUTED,
    TRAINLOG_COLOR_ACCENT,
    TRAINLOG_COLOR_SUCCESS
};

static int global_date_compare(
    const void *left,
    const void *right
)
{
    return strcmp(
        (const char *)left,
        (const char *)right
    );
}

static bool global_date_add(
    char dates[MAX_GLOBAL_BODY_DATES][TRAINLOG_TIMESTAMP_MAX + 1U],
    size_t *date_count,
    const char *value
)
{
    size_t index;

    if (dates == NULL ||
        date_count == NULL ||
        value == NULL) {
        return false;
    }

    for (index = 0U; index < *date_count; ++index) {
        if (strcmp(dates[index], value) == 0) {
            return true;
        }
    }

    if (*date_count >= MAX_GLOBAL_BODY_DATES) {
        return false;
    }

    (void)snprintf(
        dates[*date_count],
        sizeof(dates[*date_count]),
        "%s",
        value
    );

    ++(*date_count);
    return true;
}

static size_t global_date_index(
    char dates[MAX_GLOBAL_BODY_DATES][TRAINLOG_TIMESTAMP_MAX + 1U],
    size_t date_count,
    const char *value
)
{
    size_t low = 0U;
    size_t high = date_count;

    while (low < high) {
        size_t middle =
            low + ((high - low) / 2U);

        int comparison =
            strcmp(dates[middle], value);

        if (comparison < 0) {
            low = middle + 1U;
        } else {
            high = middle;
        }
    }

    return low < date_count
        ? low
        : date_count - 1U;
}

static void global_plot_point(
    int row,
    int column,
    char symbol,
    TrainlogColorRole role
)
{
    uint32_t current;
    uint32_t character;

    if (row < 0 ||
        row >= trainlog_terminal_rows(tui_terminal) ||
        column < 0 ||
        column >= trainlog_terminal_columns(tui_terminal)) {
        return;
    }

    /* Standard-plane writes are deterministic; a later series marks overlap. */
    current = (uint32_t)' ';

    character =
        (uint32_t)(unsigned char)symbol;

    if (current != (uint32_t)' ' &&
        current != (uint32_t)'.' &&
        current != character) {
        character = (uint32_t)'#';
    }

    trainlog_terminal_style_on(tui_terminal, trainlog_theme_style(role));
    trainlog_terminal_draw(tui_terminal, row, column, character);
    trainlog_terminal_style_off(tui_terminal, trainlog_theme_style(role));
}

static void global_plot_segment(
    int x1,
    int y1,
    int x2,
    int y2,
    char symbol,
    TrainlogColorRole role
)
{
    int x;

    if (x2 < x1) {
        return;
    }

    if (x1 == x2) {
        global_plot_point(
            y2,
            x2,
            symbol,
            role
        );
        return;
    }

    for (x = x1; x <= x2; ++x) {
        int y =
            y1 +
            (((y2 - y1) * (x - x1)) /
             (x2 - x1));

        global_plot_point(
            y,
            x,
            symbol,
            role
        );
    }
}

static int normalized_graph_row(
    double value,
    double minimum,
    double maximum,
    int graph_top,
    int graph_height
)
{
    double ratio;
    int row;

    if (maximum <= minimum) {
        return graph_top +
            (graph_height / 2);
    }

    ratio =
        (value - minimum) /
        (maximum - minimum);

    row =
        graph_top +
        graph_height -
        1 -
        (int)(
            ratio *
            (double)(graph_height - 1)
        );

    if (row < graph_top) {
        row = graph_top;
    }

    if (row >
        graph_top + graph_height - 1) {
        row =
            graph_top +
            graph_height -
            1;
    }

    return row;
}

static void draw_global_body_overlay(
    TrainlogDatabase *database
)
{
    static TrainlogGlobalBodySeries series[14];

    static char dates[MAX_GLOBAL_BODY_DATES]
        [TRAINLOG_TIMESTAMP_MAX + 1U];

    const size_t metric_count =
        sizeof(BODY_METRICS) /
        sizeof(BODY_METRICS[0]);

    size_t date_count = 0U;
    size_t metric_index;
    double minimum = 100.0;
    double maximum = 100.0;
    const int graph_top = 5;
    const int graph_height = 5;
    const int graph_left = 8;
    int graph_width =
        trainlog_terminal_columns(tui_terminal) - graph_left - 3;

    (void)memset(
        series,
        0,
        sizeof(series)
    );

    (void)memset(
        dates,
        0,
        sizeof(dates)
    );

    for (metric_index = 0U;
         metric_index < metric_count;
         ++metric_index) {
        TrainlogGlobalBodySeries *item =
            &series[metric_index];

        size_t point_index;

        item->symbol =
            GLOBAL_BODY_SYMBOLS[metric_index];

        item->role =
            GLOBAL_BODY_ROLES[metric_index];

        if (trainlog_database_list_body_metric_points(
                database,
                BODY_METRICS[metric_index].metric,
                item->points,
                MAX_BODY_METRIC_POINTS,
                &item->count
            ) != TRAINLOG_STATUS_OK ||
            item->count == 0U) {
            continue;
        }

        item->baseline =
            item->points[0].value;

        (void)trainlog_body_percent_change(
            item->baseline,
            item->points[item->count - 1U].value,
            &item->latest_percent
        );

        for (point_index = 0U;
             point_index < item->count;
             ++point_index) {
            double normalized = 100.0;

            (void)global_date_add(
                dates,
                &date_count,
                item->points[point_index].observed_at
            );

            if (trainlog_body_index100(
                    item->baseline,
                    item->points[point_index].value,
                    &normalized
                ) == TRAINLOG_STATUS_OK) {
                if (normalized < minimum) {
                    minimum = normalized;
                }

                if (normalized > maximum) {
                    maximum = normalized;
                }
            }
        }
    }

    if (date_count == 0U) {
        trainlog_terminal_printf(tui_terminal,
            4,
            4,
            "Aucune mensuration disponible pour la vue globale."
        );
        return;
    }

    if (graph_width < 10) {
        return;
    }

    qsort(
        dates,
        date_count,
        sizeof(dates[0]),
        global_date_compare
    );

    trainlog_terminal_printf(tui_terminal,
        3,
        4,
        "Vue globale — première mesure de chaque série = 100"
    );

    if (minimum == maximum) {
        minimum = 99.0;
        maximum = 101.0;
    }

    trainlog_terminal_printf(tui_terminal,
        graph_top,
        2,
        "%.1f",
        maximum
    );

    trainlog_terminal_printf(tui_terminal,
        graph_top + graph_height - 1,
        2,
        "%.1f",
        minimum
    );

    if (100.0 >= minimum &&
        100.0 <= maximum) {
        int baseline_row =
            normalized_graph_row(
                100.0,
                minimum,
                maximum,
                graph_top,
                graph_height
            );

        int column;

        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        for (column = graph_left;
             column <
                graph_left + graph_width;
             ++column) {
            trainlog_terminal_draw(tui_terminal,
                baseline_row,
                column,
                (uint32_t)'.'
            );
        }

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );
    }

    for (metric_index = 0U;
         metric_index < metric_count;
         ++metric_index) {
        TrainlogGlobalBodySeries *item =
            &series[metric_index];

        size_t point_index;
        int previous_x = -1;
        int previous_y = -1;

        if (item->count == 0U) {
            continue;
        }

        for (point_index = 0U;
             point_index < item->count;
             ++point_index) {
            size_t date_index =
                global_date_index(
                    dates,
                    date_count,
                    item->points[point_index].observed_at
                );

            double normalized = 100.0;
            int x;
            int y;

            if (trainlog_body_index100(
                    item->baseline,
                    item->points[point_index].value,
                    &normalized
                ) != TRAINLOG_STATUS_OK) {
                continue;
            }

            if (date_count == 1U) {
                x =
                    graph_left +
                    (graph_width / 2);
            } else {
                x =
                    graph_left +
                    (int)(
                        (date_index *
                         (size_t)(graph_width - 1)) /
                        (date_count - 1U)
                    );
            }

            y =
                normalized_graph_row(
                    normalized,
                    minimum,
                    maximum,
                    graph_top,
                    graph_height
                );

            if (previous_x >= 0 &&
                previous_y >= 0) {
                global_plot_segment(
                    previous_x,
                    previous_y,
                    x,
                    y,
                    item->symbol,
                    item->role
                );
            } else {
                global_plot_point(
                    y,
                    x,
                    item->symbol,
                    item->role
                );
            }

            previous_x = x;
            previous_y = y;
        }
    }

    {
        size_t row_index = 0U;

        for (metric_index = 0U;
             metric_index < metric_count;
             ++metric_index) {
            TrainlogGlobalBodySeries *item =
                &series[metric_index];

            int column;
            int row;

            if (item->count == 0U) {
                continue;
            }

            row =
                11 +
                (int)(row_index % 7U);

            column =
                row_index < 7U
                    ? 4
                    : (trainlog_terminal_columns(tui_terminal) / 2);

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    item->role
                )
            );

            trainlog_terminal_printf(tui_terminal,
                row,
                column,
                "%c %-16s %+.1f%%",
                item->symbol,
                BODY_METRICS[metric_index].label,
                item->latest_percent
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    item->role
                )
            );

            ++row_index;
        }
    }
}

/* TRAINLOG_DASHBOARD_GRAPH_ONLY */

/* TRAINLOG_DASHBOARD_12_MONTHS */

#define DASHBOARD_MONTH_COUNT 12U

typedef struct TrainlogDashboardMonth {
    int year;
    int month;
} TrainlogDashboardMonth;

typedef struct TrainlogDashboardMonthValue {
    bool present;
    double value;
} TrainlogDashboardMonthValue;

static bool dashboard_parse_year_month(
    const char *timestamp,
    int *year,
    int *month
)
{
    int parsed_year;
    int parsed_month;

    if (timestamp == NULL ||
        year == NULL ||
        month == NULL ||
        strlen(timestamp) < 7U ||
        timestamp[4] != '-' ||
        timestamp[0] < '0' || timestamp[0] > '9' ||
        timestamp[1] < '0' || timestamp[1] > '9' ||
        timestamp[2] < '0' || timestamp[2] > '9' ||
        timestamp[3] < '0' || timestamp[3] > '9' ||
        timestamp[5] < '0' || timestamp[5] > '9' ||
        timestamp[6] < '0' || timestamp[6] > '9') {
        return false;
    }

    parsed_year =
        ((timestamp[0] - '0') * 1000) +
        ((timestamp[1] - '0') * 100) +
        ((timestamp[2] - '0') * 10) +
        (timestamp[3] - '0');

    parsed_month =
        ((timestamp[5] - '0') * 10) +
        (timestamp[6] - '0');

    if (parsed_year < 1 ||
        parsed_month < 1 ||
        parsed_month > 12) {
        return false;
    }

    *year = parsed_year;
    *month = parsed_month;
    return true;
}

static long dashboard_month_key(
    int year,
    int month
)
{
    return ((long)year * 12L) +
        (long)(month - 1);
}

static void dashboard_month_from_key(
    long key,
    TrainlogDashboardMonth *output
)
{
    long year;
    long month_zero;

    year = key / 12L;
    month_zero = key % 12L;

    if (month_zero < 0L) {
        month_zero += 12L;
        --year;
    }

    output->year = (int)year;
    output->month = (int)month_zero + 1;
}

static bool dashboard_current_month_key(
    long *output_key
)
{
    time_t now;
    struct tm local_time;

    if (output_key == NULL) {
        return false;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return false;
    }

    if (localtime_r(&now, &local_time) == NULL) {
        return false;
    }

    *output_key =
        dashboard_month_key(
            local_time.tm_year + 1900,
            local_time.tm_mon + 1
        );

    return true;
}

static int dashboard_month_x(
    size_t month_index,
    int graph_left,
    int graph_width
)
{
    if (DASHBOARD_MONTH_COUNT <= 1U) {
        return graph_left;
    }

    return graph_left +
        (int)(
            (month_index *
             (size_t)(graph_width - 1)) /
            (DASHBOARD_MONTH_COUNT - 1U)
        );
}

static void dashboard_draw_month_axis(
    const TrainlogDashboardMonth months[DASHBOARD_MONTH_COUNT],
    int row,
    int graph_left,
    int graph_width
)
{
    size_t index;
    int spacing =
        graph_width /
        (int)(DASHBOARD_MONTH_COUNT - 1U);

    trainlog_terminal_style_on(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_MUTED
        )
    );

    for (index = 0U;
         index < DASHBOARD_MONTH_COUNT;
         ++index) {
        int x =
            dashboard_month_x(
                index,
                graph_left,
                graph_width
            );

        if (spacing >= 6) {
            char label[8];

            (void)snprintf(
                label,
                sizeof(label),
                "%02d/%02d",
                months[index].month,
                months[index].year % 100
            );

            /*
             * Center the month label on its slot, then clamp it inside
             * the terminal so the first and last visible months are not
             * silently lost at the borders.
             */
            x -= 2;

            if (x < 2) {
                x = 2;
            }

            if (x > trainlog_terminal_columns(tui_terminal) - 8) {
                x = trainlog_terminal_columns(tui_terminal) - 8;
            }

            trainlog_terminal_printf(tui_terminal,
                row,
                x,
                "%s",
                label
            );
        } else {
            if (x + 2 < trainlog_terminal_columns(tui_terminal) - 1) {
                trainlog_terminal_printf(tui_terminal,
                    row,
                    x,
                    "%02d",
                    months[index].month
                );
            }
        }
    }

    trainlog_terminal_style_off(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_MUTED
        )
    );
}

/* TRAINLOG_DASHBOARD_ASCII_PANELS */

static void dashboard_panel(
    int top,
    int left,
    int bottom,
    int right,
    const char *label
)
{
    TrainlogPanel *panel;
    int height;
    int width;

    if (top < 0 ||
        left < 0 ||
        bottom <= top ||
        right <= left ||
        bottom >= trainlog_terminal_rows(tui_terminal) ||
        right >= trainlog_terminal_columns(tui_terminal)) {
        return;
    }

    height = bottom - top + 1;
    width = right - left + 1;

    panel = tui_panel_create(tui_terminal, height,
        width,
        top,
        left
    );

    if (panel == NULL) {
        return;
    }

    tui_panel_box(panel);

    if (label != NULL &&
        label[0] != '\0' &&
        width > 8) {
        tui_panel_style_on(
            panel,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        tui_panel_print(
            panel,
            0,
            2,
            " %.*s ",
            width - 6,
            label
        );

        tui_panel_style_off(
            panel,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );
    }

    /*
     * derwin() shares the parent screen storage. syncok()+wsyncup() makes
     * the panel border part of tui_terminal, so later dashboard content and one
     * final trainlog_terminal_render(tui_terminal) compose cleanly.
     */
    tui_panel_commit(panel);
    tui_panel_destroy(panel);
}

/* TRAINLOG_PRIMARY_TOP_NAVIGATION */

/* TRAINLOG_FOCUSED_PANELS */

static void focused_panel(
    int top,
    int left,
    int bottom,
    int right,
    const char *label,
    bool active
)
{
    TrainlogPanel *panel;
    int height;
    int width;
    uint32_t border_attribute;

    if (top < 0 ||
        left < 0 ||
        bottom <= top ||
        right <= left ||
        bottom >= trainlog_terminal_rows(tui_terminal) ||
        right >= trainlog_terminal_columns(tui_terminal)) {
        return;
    }

    height = bottom - top + 1;
    width = right - left + 1;

    panel = tui_panel_create(tui_terminal, height,
        width,
        top,
        left
    );

    if (panel == NULL) {
        return;
    }

    border_attribute =
        active
            ? TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_WARNING
                )
            : trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            );

    tui_panel_style_on(
        panel,
        border_attribute
    );

    tui_panel_box(panel);

    if (label != NULL &&
        label[0] != '\0' &&
        width > 8) {
        tui_panel_print(
            panel,
            0,
            2,
            " %.*s ",
            width - 6,
            label
        );
    }

    tui_panel_style_off(
        panel,
        border_attribute
    );

    tui_panel_commit(panel);
    tui_panel_destroy(panel);
}

static void primary_top_navbar(
    int active_page,
    int selected_page,
    bool focused
)
{
    int column = 5;
    int index;

    focused_panel(
        8,
        2,
        10,
        trainlog_terminal_columns(tui_terminal) - 3,
        "NAVIGATION",
        focused
    );

    for (index = 0;
         index < PRIMARY_NAV_COUNT;
         ++index) {
        int width =
            (int)strlen(primary_nav_labels[index]) + 4;

        if (index == selected_page) {
            trainlog_terminal_style_on(tui_terminal,
                TRAINLOG_TEXT_REVERSE |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        } else if (index == active_page) {
            trainlog_terminal_style_on(tui_terminal,
                TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        }

        trainlog_terminal_printf(tui_terminal,
            9,
            column,
            " %s ",
            primary_nav_labels[index]
        );

        if (index == selected_page) {
            trainlog_terminal_style_off(tui_terminal,
                TRAINLOG_TEXT_REVERSE |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        } else if (index == active_page) {
            trainlog_terminal_style_off(tui_terminal,
                TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        }

        column += width + 2;
    }
}

static bool primary_top_nav_activate(
    int selected_page
)
{
    if (selected_page < 0 ||
        selected_page >= PRIMARY_NAV_COUNT) {
        return false;
    }

    if (selected_page == 0) {
        return true;
    }

    return trainlog_terminal_push_key(tui_terminal, '0' + selected_page);
}

/*
 * Primary screens return to the main dispatcher. Requeueing the numeric
 * shortcut lets the dashboard consume it immediately, so cross-page
 * navigation does not recursively nest TUI screens.
 */
static bool primary_top_nav_forward(
    int key
)
{
    int forwarded = 0;

    switch (key) {
    case '0':
    case TRAINLOG_KEY_HOME:
        return true;

    case TRAINLOG_KEY_F1:
    case '1':
        forwarded = '1';
        break;

    case TRAINLOG_KEY_F2:
    case '2':
        forwarded = '2';
        break;

    case TRAINLOG_KEY_F3:
    case '3':
        forwarded = '3';
        break;

    case TRAINLOG_KEY_F4:
    case '4':
        forwarded = '4';
        break;

    case TRAINLOG_KEY_F5:
    case '5':
        forwarded = '5';
        break;

    case '6':
        forwarded = '6';
        break;

    default:
        return false;
    }

    return trainlog_terminal_push_key(tui_terminal, forwarded);
}

static void dashboard_ascii_header(void)
{
    section_ascii_header("Accueil · Séance · Progression");
}

static void draw_dashboard_body_graph(
    TrainlogDatabase *database
)
{
    static TrainlogGlobalBodySeries series[14];

    static TrainlogDashboardMonthValue
        monthly[14][DASHBOARD_MONTH_COUNT];

    TrainlogDashboardMonth
        months[DASHBOARD_MONTH_COUNT];

    const size_t metric_count =
        sizeof(BODY_METRICS) /
        sizeof(BODY_METRICS[0]);

    bool large_layout =
        trainlog_terminal_columns(tui_terminal) >= 100 &&
        trainlog_terminal_rows(tui_terminal) >= 30;

    int graph_panel_top =
        large_layout ? 11 : 2;

    int graph_panel_bottom =
        large_layout ? 20 : 9;

    int graph_top =
        graph_panel_top + 1;

    int graph_height =
        graph_panel_bottom -
        graph_panel_top -
        3;

    int graph_left =
        large_layout ? 10 : 8;

    int graph_right =
        trainlog_terminal_columns(tui_terminal) - 5;

    int graph_width =
        graph_right -
        graph_left +
        1;

    int axis_row =
        graph_panel_bottom - 1;

    int legend_panel_top =
        large_layout
            ? graph_panel_bottom + 1
            : 10;

    int legend_rows =
        large_layout ? 5 : 6;

    int legend_panel_bottom =
        legend_panel_top +
        legend_rows +
        1;

    int legend_top =
        legend_panel_top + 1;

    size_t metric_index;
    size_t plotted_series = 0U;
    double minimum = 0.0;
    double maximum = 0.0;
    long current_key;
    long first_key;

    if (large_layout) {
        dashboard_panel(
            graph_panel_top,
            2,
            graph_panel_bottom,
            trainlog_terminal_columns(tui_terminal) - 3,
            "EVOLUTION CORPORELLE - 12 MOIS"
        );

        dashboard_panel(
            legend_panel_top,
            2,
            legend_panel_bottom,
            trainlog_terminal_columns(tui_terminal) - 3,
            "MESURES"
        );
    } else {
        trainlog_terminal_style_on(tui_terminal,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        trainlog_terminal_printf(tui_terminal,
            1,
            3,
            "TRAINLOG :: DASHBOARD"
        );

        trainlog_terminal_style_off(tui_terminal,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );
    }

    if (!dashboard_current_month_key(
            &current_key
        )) {
        trainlog_terminal_printf(tui_terminal,
            graph_top + 1,
            5,
            "Impossible de déterminer le mois courant."
        );
        return;
    }

    first_key =
        current_key -
        (long)(DASHBOARD_MONTH_COUNT - 1U);

    for (metric_index = 0U;
         metric_index < DASHBOARD_MONTH_COUNT;
         ++metric_index) {
        dashboard_month_from_key(
            first_key + (long)metric_index,
            &months[metric_index]
        );
    }

    (void)memset(series, 0, sizeof(series));
    (void)memset(monthly, 0, sizeof(monthly));

    for (metric_index = 0U;
         metric_index < metric_count;
         ++metric_index) {
        TrainlogGlobalBodySeries *item =
            &series[metric_index];

        size_t point_index;
        size_t visible_count = 0U;
        size_t first_visible = 0U;
        size_t latest_visible = 0U;
        bool have_first = false;

        item->symbol =
            GLOBAL_BODY_SYMBOLS[metric_index];

        item->role =
            GLOBAL_BODY_ROLES[metric_index];

        if (trainlog_database_list_body_metric_points(
                database,
                BODY_METRICS[metric_index].metric,
                item->points,
                MAX_BODY_METRIC_POINTS,
                &item->count
            ) != TRAINLOG_STATUS_OK ||
            item->count == 0U) {
            continue;
        }

        /*
         * The dashboard is monthly: multiple observations in one month keep
         * the last actual value, while absent months remain absent.
         */
        for (point_index = 0U;
             point_index < item->count;
             ++point_index) {
            int year;
            int month;
            long key;
            long offset;

            if (!dashboard_parse_year_month(
                    item->points[point_index].observed_at,
                    &year,
                    &month
                )) {
                continue;
            }

            key =
                dashboard_month_key(
                    year,
                    month
                );

            offset = key - first_key;

            if (offset < 0L ||
                offset >=
                    (long)DASHBOARD_MONTH_COUNT) {
                continue;
            }

            monthly[metric_index][(size_t)offset]
                .present = true;

            monthly[metric_index][(size_t)offset]
                .value =
                    item->points[point_index].value;
        }

        for (point_index = 0U;
             point_index < DASHBOARD_MONTH_COUNT;
             ++point_index) {
            if (!monthly[metric_index][point_index]
                    .present) {
                continue;
            }

            if (!have_first) {
                first_visible = point_index;
                have_first = true;
            }

            latest_visible = point_index;
            ++visible_count;
        }

        if (!have_first) {
            item->count = 0U;
            continue;
        }

        item->baseline =
            monthly[metric_index][first_visible]
                .value;

        if (trainlog_body_percent_change(
                item->baseline,
                monthly[metric_index][latest_visible]
                    .value,
                &item->latest_percent
            ) != TRAINLOG_STATUS_OK) {
            item->count = 0U;
            continue;
        }

        item->count = visible_count;

        if (visible_count < 2U) {
            continue;
        }

        ++plotted_series;

        for (point_index = 0U;
             point_index < DASHBOARD_MONTH_COUNT;
             ++point_index) {
            double percent = 0.0;

            if (!monthly[metric_index][point_index]
                    .present) {
                continue;
            }

            if (trainlog_body_percent_change(
                    item->baseline,
                    monthly[metric_index][point_index]
                        .value,
                    &percent
                ) == TRAINLOG_STATUS_OK) {
                if (percent < minimum) {
                    minimum = percent;
                }

                if (percent > maximum) {
                    maximum = percent;
                }
            }
        }
    }

    if (graph_width < 12 ||
        graph_height < 3) {
        return;
    }

    if (plotted_series == 0U) {
        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_printf(tui_terminal,
            graph_top + 1,
            graph_left,
            "Premières courbes après 2 mois relevés pour une même mesure."
        );

        trainlog_terminal_printf(tui_terminal,
            graph_top + 2,
            graph_left,
            "Un mois sans relevé reste vide."
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        dashboard_draw_month_axis(
            months,
            axis_row,
            graph_left,
            graph_width
        );
    } else {
        if (minimum == maximum) {
            minimum = -1.0;
            maximum = 1.0;
        }

        trainlog_terminal_printf(tui_terminal,
            graph_top,
            3,
            "%+.1f%%",
            maximum
        );

        trainlog_terminal_printf(tui_terminal,
            graph_top + graph_height - 1,
            3,
            "%+.1f%%",
            minimum
        );

        {
            int zero_row =
                normalized_graph_row(
                    0.0,
                    minimum,
                    maximum,
                    graph_top,
                    graph_height
                );

            int column;

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            for (column = graph_left;
                 column <= graph_right;
                 ++column) {
                trainlog_terminal_draw(tui_terminal,
                    zero_row,
                    column,
                    (uint32_t)'.'
                );
            }

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        }

        for (metric_index = 0U;
             metric_index < metric_count;
             ++metric_index) {
            TrainlogGlobalBodySeries *item =
                &series[metric_index];

            size_t month_index;
            int previous_x = -1;
            int previous_y = -1;
            long previous_month = -2L;

            if (item->count < 2U) {
                continue;
            }

            for (month_index = 0U;
                 month_index < DASHBOARD_MONTH_COUNT;
                 ++month_index) {
                double percent = 0.0;
                int x;
                int y;

                if (!monthly[metric_index][month_index]
                        .present) {
                    previous_x = -1;
                    previous_y = -1;
                    previous_month = -2L;
                    continue;
                }

                if (trainlog_body_percent_change(
                        item->baseline,
                        monthly[metric_index][month_index]
                            .value,
                        &percent
                    ) != TRAINLOG_STATUS_OK) {
                    continue;
                }

                x =
                    dashboard_month_x(
                        month_index,
                        graph_left,
                        graph_width
                    );

                y =
                    normalized_graph_row(
                        percent,
                        minimum,
                        maximum,
                        graph_top,
                        graph_height
                    );

                if (previous_x >= 0 &&
                    previous_y >= 0 &&
                    previous_month + 1L ==
                        (long)month_index) {
                    global_plot_segment(
                        previous_x,
                        previous_y,
                        x,
                        y,
                        item->symbol,
                        item->role
                    );
                } else {
                    global_plot_point(
                        y,
                        x,
                        item->symbol,
                        item->role
                    );
                }

                previous_x = x;
                previous_y = y;
                previous_month =
                    (long)month_index;
            }
        }

        dashboard_draw_month_axis(
            months,
            axis_row,
            graph_left,
            graph_width
        );
    }

    {
        size_t legend_index = 0U;

        int columns =
            large_layout ? 3 : 2;

        int cell_width =
            (trainlog_terminal_columns(tui_terminal) - 8) /
            columns;

        for (metric_index = 0U;
             metric_index < metric_count;
             ++metric_index) {
            TrainlogGlobalBodySeries *item =
                &series[metric_index];

            char label[96];
            char evolution[16];
            int logical_column;
            int logical_row;
            int row;
            int column;
            int label_width;
            size_t month_index;
            size_t latest_month = 0U;
            bool found = false;
            double latest = 0.0;

            if (item->count == 0U) {
                continue;
            }

            for (month_index = 0U;
                 month_index < DASHBOARD_MONTH_COUNT;
                 ++month_index) {
                if (monthly[metric_index][month_index]
                        .present) {
                    latest_month = month_index;
                    found = true;
                }
            }

            if (!found) {
                continue;
            }

            logical_column =
                (int)(
                    legend_index /
                    (size_t)legend_rows
                );

            logical_row =
                (int)(
                    legend_index %
                    (size_t)legend_rows
                );

            if (logical_column >= columns) {
                break;
            }

            latest =
                monthly[metric_index][latest_month]
                    .value;

            row =
                legend_top +
                logical_row;

            column =
                (large_layout ? 4 : 2) +
                (logical_column * cell_width);

            label_width =
                cell_width - 18;

            if (label_width < 7) {
                label_width = 7;
            }

            (void)snprintf(
                label,
                sizeof(label),
                "%s (%s)",
                BODY_METRICS[metric_index].label,
                BODY_METRICS[metric_index].unit
            );

            if (item->count < 2U) {
                (void)snprintf(
                    evolution,
                    sizeof(evolution),
                    "%s",
                    "réf."
                );
            } else {
                (void)snprintf(
                    evolution,
                    sizeof(evolution),
                    "%+.1f%%",
                    item->latest_percent
                );
            }

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    item->role
                )
            );

            trainlog_terminal_printf(tui_terminal,
                row,
                column,
                "%c %-*.*s %6.1f",
                item->symbol,
                label_width,
                label_width,
                label,
                latest
            );

            trainlog_terminal_printf(tui_terminal,
                row,
                column + cell_width - 7,
                "%6s",
                evolution
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    item->role
                )
            );

            ++legend_index;
        }
    }
}

static DashboardAction screen_dashboard(
    TrainlogDatabase *database
)
{
    static const char *const footer =
        "←→ naviguer  Entrée ouvrir  0 accueil  F1-F5 accès direct  q quitter";

    int selected = 0;

    for (;;) {
        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int nav_top =
            large_layout ? 8 : trainlog_terminal_rows(tui_terminal) - 4;

        int nav_bottom =
            large_layout ? 10 : trainlog_terminal_rows(tui_terminal) - 3;

        int key;
        int index;
        int column;

        trainlog_terminal_erase(tui_terminal);
        trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

        if (large_layout) {
            dashboard_ascii_header();

            focused_panel(
                nav_top,
                2,
                nav_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "NAVIGATION",
                true
            );
        }

        draw_dashboard_body_graph(database);

        column =
            large_layout ? 5 : 3;

        for (index = 0;
             index < PRIMARY_NAV_COUNT;
             ++index) {
            int width =
                (int)strlen(primary_nav_labels[index]) + 4;

            if (index == selected) {
                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_printf(tui_terminal,
                large_layout
                    ? nav_top + 1
                    : trainlog_terminal_rows(tui_terminal) - 3,
                column,
                " %s ",
                primary_nav_labels[index]
            );

            if (index == selected) {
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            column += width + 2;
        }

        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_printf(tui_terminal,
            trainlog_terminal_rows(tui_terminal) - 2,
            2,
            "%.*s",
            trainlog_terminal_columns(tui_terminal) - 4,
            footer
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        switch (key) {
        case TRAINLOG_KEY_UP:
        case TRAINLOG_KEY_LEFT:
            selected =
                selected > 0
                    ? selected - 1
                    : PRIMARY_NAV_COUNT - 1;
            break;

        case TRAINLOG_KEY_DOWN:
        case TRAINLOG_KEY_RIGHT:
            selected =
                selected < PRIMARY_NAV_COUNT - 1
                    ? selected + 1
                    : 0;
            break;

        case '\n':
        case TRAINLOG_KEY_ENTER:
            switch (selected) {
            case 0:
                break;
            case 1:
                return DASHBOARD_NEW_SESSION;
            case 2:
                return DASHBOARD_HISTORY;
            case 3:
                return DASHBOARD_EXERCISES;
            case 4:
                return DASHBOARD_EQUIPMENT;
            case 5:
                return DASHBOARD_BODY;
            case 6:
                return DASHBOARD_SYNC;
            default:
                break;
            }
            break;

        case '0':
        case TRAINLOG_KEY_HOME:
            selected = 0;
            break;

        case TRAINLOG_KEY_F1:
        case '1':
            return DASHBOARD_NEW_SESSION;

        case TRAINLOG_KEY_F2:
        case '2':
            return DASHBOARD_HISTORY;

        case TRAINLOG_KEY_F3:
        case '3':
            return DASHBOARD_EXERCISES;

        case TRAINLOG_KEY_F4:
        case '4':
            return DASHBOARD_EQUIPMENT;

        case TRAINLOG_KEY_F5:
        case '5':
            return DASHBOARD_BODY;

        case '6':
            return DASHBOARD_SYNC;

        case 'q':
        case 'Q':
            return DASHBOARD_QUIT;

        default:
            break;
        }
    }
}

static bool build_session_exercise(
    TrainlogDatabase *database,
    TrainlogSessionType session_type,
    TrainlogSessionExerciseInput *output,
    TrainlogSetInput *set_storage,
    size_t set_capacity
)
{
    TrainlogExercise exercise;
    int load_mode = 1;
    int target_sets = 3;
    int target_metric;
    int rest_seconds = 60;
    int actual_sets = 0;
    int rep_values[MAX_SETS_PER_EXERCISE];
    size_t rep_count = 0U;
    char rep_sequence[512];
    bool target_has_weight = false;
    double target_weight = 0.0;
    size_t set_index;

    if (!choose_exercise(
            database,
            &exercise
        )) {
        return false;
    }

    (void)memset(
        output,
        0,
        sizeof(*output)
    );

    (void)snprintf(
        output->exercise_id,
        sizeof(output->exercise_id),
        "%s",
        exercise.exercise_id
    );

    output->recording_mode =
        exercise.recording_mode;

    output->data_fields =
        exercise.data_fields;

    if (!choose_equipment(database, output->equipment_id,
            sizeof(output->equipment_id))) {
        return false;
    }

    if (session_type == TRAINLOG_SESSION_MAX_TEST) {
        bool has_max = false;
        double max_weight = 0.0;

        draw_shell(
            exercise.name,
            "Échap annuler · Test de max"
        );
        if (!prompt_optional_double(
                4,
                "Poids max (kg) : ",
                &has_max,
                &max_weight
            ) || !has_max || max_weight <= 0.0) {
            status_line(
                "Poids max requis et strictement positif.",
                TRAINLOG_COLOR_ERROR
            );
            return false;
        }

        /* CONTRACT: the max result has no hidden series/repetition payload. */
        output->has_max_weight = true;
        output->max_weight_kg = max_weight;
        output->load_mode = TRAINLOG_LOAD_NONE;
        output->sets = NULL;
        output->set_count = 0U;
        return true;
    }

    if (exercise.recording_mode ==
        TRAINLOG_RECORDING_CONTINUOUS) {
        int duration_minutes = 30;
        int duration_seconds;
        bool has_value = false;
        double value = 0.0;

        draw_shell(
            exercise.name,
            "Échap annuler · activité continue"
        );

        if (!prompt_int_value(
                4,
                "Durée (minutes)",
                1,
                1440,
                duration_minutes,
                &duration_minutes
            )) {
            return false;
        }

        duration_seconds =
            duration_minutes * 60;

        output->continuous_duration_seconds =
            duration_seconds;

        output->load_mode =
            TRAINLOG_LOAD_NONE;

        output->rest_seconds = 0;
        output->target_sets = 0;
        output->target_reps = 0;
        output->target_duration_seconds = 0;
        output->target_has_weight = false;
        output->sets = NULL;
        output->set_count = 0U;

        if ((exercise.data_fields &
             TRAINLOG_EXERCISE_DATA_SPEED_KMH) != 0U) {
            if (!prompt_optional_double(
                    5,
                    "Vitesse km/h : ",
                    &has_value,
                    &value
                ) ||
                !has_value) {
                return false;
            }

            output->continuous_has_speed =
                true;

            output->continuous_speed_kmh =
                value;
        }

        if ((exercise.data_fields &
             TRAINLOG_EXERCISE_DATA_DISTANCE_KM) != 0U) {
            has_value = false;
            value = 0.0;

            if (!prompt_optional_double(
                    6,
                    "Distance km : ",
                    &has_value,
                    &value
                ) ||
                !has_value) {
                return false;
            }

            output->continuous_has_distance =
                true;

            output->continuous_distance_km =
                value;
        }

        return true;
    }

    target_metric =
        exercise.tracking_mode ==
            TRAINLOG_TRACKING_REPS
            ? 10
            : 45;

    draw_shell(
        exercise.name,
        "Échap annuler · Durées : 90, 90s, 1:30, 1m30, 2m"
    );

    if (!prompt_int_value(
            4,
            "Charge 1=aucune 2=externe 3=assistance",
            1,
            3,
            1,
            &load_mode
        )) {
        return false;
    }

    if (load_mode != 1) {
        if (!prompt_optional_double(
                5,
                "Charge cible kg : ",
                &target_has_weight,
                &target_weight
            ) ||
            !target_has_weight) {
            return false;
        }
    }

    if (!prompt_int_value(
            6,
            "Séries prévues",
            1,
            (int)set_capacity,
            3,
            &target_sets
        )) {
        return false;
    }

    if (exercise.tracking_mode ==
        TRAINLOG_TRACKING_REPS) {
        if (!prompt_int_value(
                7,
                "Répétitions cibles",
                1,
                10000,
                target_metric,
                &target_metric
            )) {
            return false;
        }
    } else {
        if (!prompt_duration_value(
                7,
                "Durée cible",
                1,
                86400,
                target_metric,
                &target_metric
            )) {
            return false;
        }
    }

    if (!prompt_duration_value(
            8,
            "Repos prévu",
            0,
            86400,
            60,
            &rest_seconds
        )) {
        return false;
    }

    if (
        exercise.tracking_mode ==
        TRAINLOG_TRACKING_REPS
    ) {
        for (;;) {
            if (!prompt_text(
                    9,
                    "Séries réalisées (5x10 | 4,5,6,... | 4..10..4) : ",
                    rep_sequence,
                    sizeof(rep_sequence),
                    false
                )) {
                return false;
            }

            if (
                trainlog_reps_parse_sequence(
                    rep_sequence,
                    rep_values,
                    set_capacity,
                    &rep_count
                ) ==
                TRAINLOG_STATUS_OK
            ) {
                break;
            }

            status_line(
                "Séries invalides. Exemples : 5x10 · 4,5,6,7 · 4..10..4",
                TRAINLOG_COLOR_ERROR
            );

            trainlog_terminal_render(tui_terminal);
        }
    } else {
        if (!prompt_int_value(
                9,
                "Séries réellement faites",
                0,
                (int)set_capacity,
                target_sets,
                &actual_sets
            )) {
            return false;
        }
    }

    output->load_mode =
        load_mode == 1
            ? TRAINLOG_LOAD_NONE
            : (
                load_mode == 2
                    ? TRAINLOG_LOAD_EXTERNAL
                    : TRAINLOG_LOAD_ASSISTANCE
            );

    output->rest_seconds = rest_seconds;
    output->target_sets = target_sets;

    output->target_reps =
        exercise.tracking_mode ==
            TRAINLOG_TRACKING_REPS
            ? target_metric
            : 0;

    output->target_duration_seconds =
        exercise.tracking_mode ==
            TRAINLOG_TRACKING_DURATION
            ? target_metric
            : 0;

    output->target_has_weight =
        target_has_weight;

    output->target_weight_kg =
        target_weight;

    output->sets = set_storage;
    output->set_count =
        exercise.tracking_mode ==
            TRAINLOG_TRACKING_REPS
            ? rep_count
            : (size_t)actual_sets;

    for (set_index = 0U;
         set_index < output->set_count;
         ++set_index) {
        int actual_metric =
            target_metric;

        (void)memset(
            &set_storage[set_index],
            0,
            sizeof(set_storage[set_index])
        );

        if (
            exercise.tracking_mode ==
                TRAINLOG_TRACKING_DURATION ||
            target_has_weight
        ) {
            draw_shell(
                exercise.name,
                "Échap annuler · Durées : 90, 90s, 1:30, 1m30, 2m"
            );

            trainlog_terminal_printf(tui_terminal,
                3,
                4,
                "Série %zu / %zu",
                set_index + 1U,
                output->set_count
            );
        }

        if (
            exercise.tracking_mode ==
            TRAINLOG_TRACKING_REPS
        ) {
            actual_metric =
                rep_values[set_index];

            set_storage[set_index].reps =
                actual_metric;
        } else {
            if (!prompt_duration_value(
                    5,
                    "Durée réalisée",
                    1,
                    86400,
                    target_metric,
                    &actual_metric
                )) {
                return false;
            }

            set_storage[
                set_index
            ].duration_seconds =
                actual_metric;
        }

        if (target_has_weight) {
            char buffer[64];
            char prompt[128];
            double actual_weight =
                target_weight;

            (void)snprintf(
                prompt,
                sizeof(prompt),
                "Charge kg [%.1f] : ",
                target_weight
            );

            if (!prompt_text(
                    6,
                    prompt,
                    buffer,
                    sizeof(buffer),
                    true
                )) {
                return false;
            }

            if (buffer[0] != '\0' &&
                !parse_double_positive(
                    buffer,
                    &actual_weight
                )) {
                status_line(
                    "Charge invalide.",
                    TRAINLOG_COLOR_ERROR
                );

                wait_key();
                return false;
            }

            set_storage[
                set_index
            ].has_weight =
                true;

            set_storage[
                set_index
            ].weight_kg =
                actual_weight;
        }
    }

    return true;
}

/* TRAINLOG_SESSION_TYPE_TUI */

static const char *session_type_label(
    TrainlogSessionType type
)
{
    switch (type) {
    case TRAINLOG_SESSION_MAX_TEST:
        return "Test de max";

    case TRAINLOG_SESSION_TRAINING:
    default:
        return "Entraînement";
    }
}

static const char *session_type_history_label(
    TrainlogSessionType type
)
{
    return type == TRAINLOG_SESSION_MAX_TEST
        ? "[MAX]"
        : "[ENTRAINEMENT]";
}

static bool choose_session_type(
    TrainlogSessionType *output
)
{
    int selected = 0;
    int nav_selected = 1;
    int focus = 1;

    if (output == NULL) {
        return false;
    }

    for (;;) {
        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int key;

        if (large_layout) {
            trainlog_terminal_erase(tui_terminal);
            trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

            section_ascii_header(
                ":: N O U V E L L E   S E A N C E ::"
            );

            primary_top_navbar(
                1,
                nav_selected,
                focus == 0
            );

            focused_panel(
                11,
                4,
                20,
                trainlog_terminal_columns(tui_terminal) - 5,
                "TYPE DE SEANCE",
                focus == 1
            );

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                "Tab zone  ←→ menu  ↑↓ type  Entrée valider  0/Home accueil  F2-F5 direct  Échap annuler"
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            if (focus == 1 &&
                selected == 0) {
                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_printf(tui_terminal,
                13,
                7,
                " Entraînement "
            );

            if (focus == 1 &&
                selected == 0) {
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                14,
                9,
                "Séance normale : progression, volume, travail courant."
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            if (focus == 1 &&
                selected == 1) {
                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_printf(tui_terminal,
                17,
                7,
                " Test de max "
            );

            if (focus == 1 &&
                selected == 1) {
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                18,
                9,
                "Séance explicitement dédiée aux mesures de max."
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            TrainlogPanel *panel;
            int panel_width =
                trainlog_terminal_columns(tui_terminal) - 8;

            draw_shell(
                "TRAINLOG — Nouvelle séance",
                "↑↓ choisir  Entrée valider  Échap annuler"
            );

            if (panel_width < 40) {
                panel_width = 40;
            }

            panel = tui_panel_create(tui_terminal, 9,
                panel_width,
                3,
                4
            );

            if (panel == NULL) {
                return false;
            }

            tui_panel_box(panel);

            tui_panel_style_on(
                panel,
                TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );

            tui_panel_print(
                panel,
                0,
                2,
                " TYPE DE SEANCE "
            );

            tui_panel_style_off(
                panel,
                TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );

            if (selected == 0) {
                tui_panel_style_on(
                    panel,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            tui_panel_print(
                panel,
                2,
                3,
                " Entraînement "
            );

            if (selected == 0) {
                tui_panel_style_off(
                    panel,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            tui_panel_print(
                panel,
                3,
                5,
                "Séance normale : progression, volume, travail courant."
            );

            if (selected == 1) {
                tui_panel_style_on(
                    panel,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            tui_panel_print(
                panel,
                5,
                3,
                " Test de max "
            );

            if (selected == 1) {
                tui_panel_style_off(
                    panel,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            tui_panel_print(
                panel,
                6,
                5,
                "Séance explicitement dédiée aux mesures de max."
            );

            tui_panel_commit(panel);
            tui_panel_destroy(panel);
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (large_layout &&
            (key == TRAINLOG_KEY_TAB ||
             key == TRAINLOG_KEY_SHIFT_TAB)) {
            focus =
                focus == 0
                    ? 1
                    : 0;
            continue;
        }

        if (key == 27) {
            return false;
        }

        if (large_layout &&
            (key == '0' ||
             key == TRAINLOG_KEY_HOME)) {
            return false;
        }

        if (large_layout &&
            (key == '2' ||
             key == TRAINLOG_KEY_F2 ||
             key == '3' ||
             key == TRAINLOG_KEY_F3 ||
             key == '4' ||
             key == TRAINLOG_KEY_F4 ||
             key == '5' ||
             key == TRAINLOG_KEY_F5 ||
             key == '6')) {
            if (primary_top_nav_forward(key)) {
                return false;
            }

            continue;
        }

        if (large_layout &&
            focus == 0) {
            if (key == TRAINLOG_KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : PRIMARY_NAV_COUNT - 1;
            } else if (key == TRAINLOG_KEY_RIGHT) {
                nav_selected =
                    nav_selected < PRIMARY_NAV_COUNT - 1
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == TRAINLOG_KEY_ENTER
            ) {
                if (nav_selected == 1) {
                    focus = 1;
                } else if (
                    primary_top_nav_activate(
                        nav_selected
                    )
                ) {
                    return false;
                }
            }

            continue;
        }

        if (key == TRAINLOG_KEY_UP ||
            key == TRAINLOG_KEY_LEFT ||
            key == TRAINLOG_KEY_DOWN ||
            key == TRAINLOG_KEY_RIGHT) {
            selected =
                selected == 0
                    ? 1
                    : 0;
            continue;
        }

        if (key == '\n' ||
            key == TRAINLOG_KEY_ENTER) {
            *output =
                selected == 0
                    ? TRAINLOG_SESSION_TRAINING
                    : TRAINLOG_SESSION_MAX_TEST;
            return true;
        }
    }
}

/* TRAINLOG_SESSION_EDIT_TUI */

typedef struct TrainlogSessionDraftExercise {
    TrainlogSessionExerciseInput input;
    TrainlogSetInput sets[MAX_SETS_PER_EXERCISE];
    char name[TRAINLOG_NAME_MAX + 1U];
    TrainlogTrackingMode tracking_mode;
    char notes[TRAINLOG_NOTE_MAX + 1U];
} TrainlogSessionDraftExercise;

static bool draft_lookup_exercise(
    TrainlogDatabase *database,
    const char *exercise_id,
    char *output_name,
    size_t output_name_size,
    TrainlogTrackingMode *output_tracking
)
{
    TrainlogExercise exercises[MAX_EXERCISES];
    size_t count = 0U;
    size_t index;

    if (database == NULL ||
        exercise_id == NULL ||
        output_name == NULL ||
        output_name_size == 0U ||
        output_tracking == NULL) {
        return false;
    }

    if (trainlog_database_list_exercises(
            database,
            exercises,
            MAX_EXERCISES,
            &count
        ) != TRAINLOG_STATUS_OK) {
        return false;
    }

    for (index = 0U;
         index < count;
         ++index) {
        if (strcmp(
                exercises[index].exercise_id,
                exercise_id
            ) == 0) {
            (void)snprintf(
                output_name,
                output_name_size,
                "%s",
                exercises[index].name
            );

            *output_tracking =
                exercises[index].tracking_mode;

            return true;
        }
    }

    return false;
}

static bool draft_build_exercise(
    TrainlogDatabase *database,
    TrainlogSessionType session_type,
    TrainlogSessionDraftExercise *draft,
    const char *preserved_notes
)
{
    TrainlogSessionExerciseInput input;
    TrainlogSetInput sets[MAX_SETS_PER_EXERCISE];

    if (database == NULL ||
        draft == NULL) {
        return false;
    }

    (void)memset(
        &input,
        0,
        sizeof(input)
    );

    (void)memset(
        sets,
        0,
        sizeof(sets)
    );

    if (!build_session_exercise(
            database,
            session_type,
            &input,
            sets,
            MAX_SETS_PER_EXERCISE
        )) {
        return false;
    }

    (void)memset(
        draft,
        0,
        sizeof(*draft)
    );

    draft->input = input;

    if (input.set_count >
        MAX_SETS_PER_EXERCISE) {
        return false;
    }

    (void)memcpy(
        draft->sets,
        sets,
        input.set_count *
            sizeof(draft->sets[0])
    );

    draft->input.sets =
        draft->sets;

    if (preserved_notes != NULL &&
        preserved_notes[0] != '\0') {
        (void)snprintf(
            draft->notes,
            sizeof(draft->notes),
            "%s",
            preserved_notes
        );

        draft->input.notes =
            draft->notes;
    } else {
        draft->notes[0] = '\0';
        draft->input.notes = NULL;
    }

    if (!draft_lookup_exercise(
            database,
            draft->input.exercise_id,
            draft->name,
            sizeof(draft->name),
            &draft->tracking_mode
        )) {
        return false;
    }

    return true;
}

static void draft_bind_input(
    TrainlogSessionDraftExercise *draft
)
{
    if (draft == NULL) {
        return;
    }

    draft->input.sets =
        draft->sets;

    draft->input.notes =
        draft->notes[0] != '\0'
            ? draft->notes
            : NULL;
}

static void draft_delete_exercise(
    TrainlogSessionDraftExercise *drafts,
    size_t *count,
    size_t selected
)
{
    size_t index;

    if (drafts == NULL ||
        count == NULL ||
        selected >= *count) {
        return;
    }

    for (index = selected;
         index + 1U < *count;
         ++index) {
        drafts[index] =
            drafts[index + 1U];

        draft_bind_input(
            &drafts[index]
        );
    }

    --(*count);

    if (*count < MAX_SESSION_EXERCISES) {
        (void)memset(
            &drafts[*count],
            0,
            sizeof(drafts[*count])
        );
    }
}

static void draft_set_summary(
    const TrainlogSessionDraftExercise *draft,
    char *output,
    size_t output_size
)
{
    if (draft == NULL ||
        output == NULL ||
        output_size == 0U) {
        return;
    }

    if (draft->input.has_max_weight) {
        char weight[32];

        format_compact_max_weight(
            draft->input.max_weight_kg,
            weight,
            sizeof(weight)
        );
        (void)snprintf(
            output,
            output_size,
            "Max %s kg",
            weight
        );
    } else if (draft->input.load_mode ==
        TRAINLOG_LOAD_EXTERNAL) {
        (void)snprintf(
            output,
            output_size,
            "%zu séries · %.1f kg",
            draft->input.set_count,
            draft->input.target_weight_kg
        );
    } else if (
        draft->input.load_mode ==
        TRAINLOG_LOAD_ASSISTANCE
    ) {
        (void)snprintf(
            output,
            output_size,
            "%zu séries · %.1f kg aide",
            draft->input.set_count,
            draft->input.target_weight_kg
        );
    } else {
        (void)snprintf(
            output,
            output_size,
            "%zu séries",
            draft->input.set_count
        );
    }
}

static bool confirm_draft_delete(
    const char *name
)
{
    int answer = 0;

    draw_shell(
        "Supprimer de la séance ?",
        "1 confirmer  0 annuler"
    );

    trainlog_terminal_printf(tui_terminal,
        4,
        4,
        "%s",
        name != NULL
            ? name
            : "Exercice"
    );

    if (!prompt_int_value(
            6,
            "Supprimer ? 1=oui 0=non",
            0,
            1,
            0,
            &answer
        )) {
        return false;
    }

    return answer == 1;
}

static bool edit_session_draft(
    TrainlogDatabase *database,
    TrainlogSessionDraftExercise drafts[MAX_SESSION_EXERCISES],
    size_t *count,
    TrainlogSessionType session_type
)
{
    size_t selected = 0U;

    if (database == NULL ||
        drafts == NULL ||
        count == NULL) {
        return false;
    }

    for (;;) {
        size_t index;
        size_t top = 0U;

        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int frame_top =
            large_layout ? 8 : 3;

        int frame_bottom =
            trainlog_terminal_rows(tui_terminal) - 4;

        int first_row =
            frame_top + 3;

        int visible_rows =
            frame_bottom -
            first_row;

        int key;

        if (visible_rows < 1) {
            return false;
        }

        if (*count > 0U &&
            selected >= *count) {
            selected =
                *count - 1U;
        }

        if (*count > 0U &&
            selected >=
                (size_t)visible_rows) {
            top =
                selected -
                (size_t)visible_rows +
                1U;
        }

        if (large_layout) {
            trainlog_terminal_erase(tui_terminal);
            trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

            section_ascii_header(
                ":: S E A N C E   E N   C O U R S ::"
            );

            focused_panel(
                frame_top,
                2,
                frame_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "RESUME AVANT ENREGISTREMENT",
                true
            );

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                "↑↓ choisir  e/Entrée modifier  a ajouter  d supprimer  f enregistrer  q/Échap abandonner"
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Séance en cours",
                "↑↓ choisir  e/Entrée modifier  a ajouter  d supprimer  f enregistrer  q/Échap abandonner"
            );
        }

        trainlog_terminal_style_on(tui_terminal,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        trainlog_terminal_printf(tui_terminal,
            frame_top + 1,
            large_layout ? 5 : 4,
            "Type : %s",
            session_type_label(
                session_type
            )
        );

        trainlog_terminal_style_off(tui_terminal,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        if (*count == 0U) {
            trainlog_terminal_printf(tui_terminal,
                first_row + 1,
                large_layout ? 5 : 4,
                "Aucun exercice saisi. a = ajouter."
            );
        }

        for (index = 0U;
             index < (size_t)visible_rows &&
             top + index < *count;
             ++index) {
            size_t absolute =
                top + index;

            char summary[96];

            int row =
                first_row +
                (int)index;

            draft_set_summary(
                &drafts[absolute],
                summary,
                sizeof(summary)
            );

            if (absolute == selected) {
                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_printf(tui_terminal,
                row,
                large_layout ? 5 : 4,
                " %2zu  %-36.36s  %-28.28s ",
                absolute + 1U,
                drafts[absolute].name,
                summary
            );

            if (absolute == selected) {
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (key == 'q' ||
            key == 'Q' ||
            key == 27) {
            return false;
        }

        if (key == 'f' ||
            key == 'F') {
            if (*count == 0U) {
                status_line(
                    "Ajoutez au moins un exercice avant d'enregistrer.",
                    TRAINLOG_COLOR_WARNING
                );

                trainlog_terminal_render(tui_terminal);
                (void)trainlog_terminal_get_key(tui_terminal);
                continue;
            }

            return true;
        }

        if (key == TRAINLOG_KEY_UP &&
            *count > 0U) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : *count - 1U;
            continue;
        }

        if (key == TRAINLOG_KEY_DOWN &&
            *count > 0U) {
            selected =
                selected + 1U < *count
                    ? selected + 1U
                    : 0U;
            continue;
        }

        if (key == 'a' ||
            key == 'A') {
            if (*count >=
                MAX_SESSION_EXERCISES) {
                status_line(
                    "Nombre maximal d'exercices atteint.",
                    TRAINLOG_COLOR_WARNING
                );

                trainlog_terminal_render(tui_terminal);
                (void)trainlog_terminal_get_key(tui_terminal);
                continue;
            }

            if (draft_build_exercise(
                    database,
                    session_type,
                    &drafts[*count],
                    NULL
                )) {
                selected = *count;
                ++(*count);
            }

            continue;
        }

        if ((key == 'e' ||
             key == 'E' ||
             key == '\n' ||
             key == TRAINLOG_KEY_ENTER) &&
            *count > 0U) {
            TrainlogSessionDraftExercise replacement;

            if (draft_build_exercise(
                    database,
                    session_type,
                    &replacement,
                    drafts[selected].notes
                )) {
                /* INVARIANT: editing replaces values, never occurrence
                 * identity. Sync idempotency depends on stable entry_id. */
                (void)snprintf(
                    replacement.input.entry_id,
                    sizeof(replacement.input.entry_id),
                    "%s",
                    drafts[selected].input.entry_id
                );
                drafts[selected] =
                    replacement;

                draft_bind_input(
                    &drafts[selected]
                );
            }

            continue;
        }

        if ((key == 'd' ||
             key == 'D') &&
            *count > 0U) {
            if (confirm_draft_delete(
                    drafts[selected].name
                )) {
                draft_delete_exercise(
                    drafts,
                    count,
                    selected
                );

                if (*count > 0U &&
                    selected >= *count) {
                    selected =
                        *count - 1U;
                }
            }
        }
    }
}

static bool load_persisted_draft(
    TrainlogDatabase *database,
    const char *session_id,
    TrainlogSessionSummary *session,
    TrainlogSessionDraftExercise drafts[MAX_SESSION_EXERCISES],
    size_t *draft_count
)
{
    TrainlogEditableExerciseRecord
        records[MAX_SESSION_EXERCISES];

    TrainlogSetInput
        sets[MAX_SESSION_EXERCISES *
             MAX_SETS_PER_EXERCISE];

    size_t record_count = 0U;
    size_t set_count = 0U;
    size_t index;

    if (database == NULL ||
        session_id == NULL ||
        session == NULL ||
        drafts == NULL ||
        draft_count == NULL) {
        return false;
    }

    if (trainlog_database_load_session_editable(
            database,
            session_id,
            session,
            records,
            MAX_SESSION_EXERCISES,
            &record_count,
            sets,
            MAX_SESSION_EXERCISES *
                MAX_SETS_PER_EXERCISE,
            &set_count
        ) != TRAINLOG_STATUS_OK) {
        return false;
    }

    (void)set_count;

    (void)memset(
        drafts,
        0,
        sizeof(*drafts) *
            MAX_SESSION_EXERCISES
    );

    for (index = 0U;
         index < record_count;
         ++index) {
        TrainlogSessionDraftExercise *draft =
            &drafts[index];

        const TrainlogEditableExerciseRecord *record =
            &records[index];

        if (record->set_count >
            MAX_SETS_PER_EXERCISE) {
            return false;
        }

        (void)snprintf(
            draft->input.exercise_id,
            sizeof(draft->input.exercise_id),
            "%s",
            record->exercise_id
        );

        (void)snprintf(
            draft->name,
            sizeof(draft->name),
            "%s",
            record->name
        );

        draft->tracking_mode =
            record->tracking_mode;

        draft->input.recording_mode =
            record->recording_mode;
        draft->input.data_fields =
            record->data_fields;

        draft->input.load_mode =
            record->load_mode;

        draft->input.rest_seconds =
            record->rest_seconds;

        draft->input.target_sets =
            record->target_sets;

        draft->input.target_reps =
            record->target_reps;

        draft->input.target_duration_seconds =
            record->target_duration_seconds;

        draft->input.target_has_weight =
            record->has_target_weight != 0;

        draft->input.target_weight_kg =
            record->target_weight_kg;

        draft->input.has_max_weight =
            record->has_max_weight != 0;
        draft->input.max_weight_kg =
            record->max_weight_kg;

        draft->input.continuous_duration_seconds =
            record->continuous_duration_seconds;
        draft->input.continuous_has_speed =
            record->has_continuous_speed != 0;
        draft->input.continuous_speed_kmh =
            record->continuous_speed_kmh;
        draft->input.continuous_has_distance =
            record->has_continuous_distance != 0;
        draft->input.continuous_distance_km =
            record->continuous_distance_km;

        (void)snprintf(
            draft->input.entry_id,
            sizeof(draft->input.entry_id),
            "%s",
            record->entry_id
        );
        (void)snprintf(
            draft->input.equipment_id,
            sizeof(draft->input.equipment_id),
            "%s",
            record->equipment_id
        );

        (void)snprintf(
            draft->notes,
            sizeof(draft->notes),
            "%s",
            record->notes
        );

        if (record->set_count > 0U) {
            (void)memcpy(
                draft->sets,
                &sets[record->set_offset],
                record->set_count *
                    sizeof(draft->sets[0])
            );
        }

        draft->input.set_count =
            record->set_count;

        draft_bind_input(draft);
    }

    *draft_count =
        record_count;

    return true;
}

static bool persist_draft_replacement(
    TrainlogDatabase *database,
    const char *session_id,
    TrainlogSessionDraftExercise drafts[MAX_SESSION_EXERCISES],
    size_t count
)
{
    TrainlogSessionExerciseInput
        inputs[MAX_SESSION_EXERCISES];

    size_t index;

    for (index = 0U;
         index < count;
         ++index) {
        draft_bind_input(
            &drafts[index]
        );

        inputs[index] =
            drafts[index].input;
    }

    return trainlog_database_replace_session_exercises(
        database,
        session_id,
        inputs,
        count
    ) == TRAINLOG_STATUS_OK;
}

static void edit_persisted_session(
    TrainlogDatabase *database,
    const char *session_id
)
{
    TrainlogSessionSummary session;
    TrainlogSessionDraftExercise
        drafts[MAX_SESSION_EXERCISES];

    size_t count = 0U;

    if (!load_persisted_draft(
            database,
            session_id,
            &session,
            drafts,
            &count
        )) {
        status_line(
            "Impossible de charger la séance à modifier.",
            TRAINLOG_COLOR_ERROR
        );

        wait_key();
        return;
    }

    if (!edit_session_draft(
            database,
            drafts,
            &count,
            session.session_type
        )) {
        status_line(
            "Modification abandonnée : aucune donnée changée.",
            TRAINLOG_COLOR_MUTED
        );

        wait_key();
        return;
    }

    if (persist_draft_replacement(
            database,
            session_id,
            drafts,
            count
        )) {
        status_line(
            "✓ Séance corrigée.",
            TRAINLOG_COLOR_SUCCESS
        );
    } else {
        status_line(
            "Échec de la correction : ancienne séance conservée.",
            TRAINLOG_COLOR_ERROR
        );
    }

    wait_key();
}

static void screen_new_session(
    TrainlogDatabase *database
)
{
    TrainlogSessionDraftExercise
        drafts[MAX_SESSION_EXERCISES];

    TrainlogSessionExerciseInput
        exercise_inputs[MAX_SESSION_EXERCISES];

    TrainlogSessionInput session;
    char session_id[TRAINLOG_GENERATED_ID_CAPACITY];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char ended_at[TRAINLOG_TIMESTAMP_MAX + 1U];

    TrainlogSessionType session_type =
        TRAINLOG_SESSION_TRAINING;

    size_t catalog_count = 0U;
    size_t exercise_count = 0U;
    size_t index;
    TrainlogStatus status;

    if (trainlog_database_exercise_count(
            database,
            &catalog_count
        ) != TRAINLOG_STATUS_OK) {
        return;
    }

    if (catalog_count == 0U) {
        draw_shell(
            "Nouvelle séance",
            "Une touche pour revenir"
        );

        status_line(
            "Ajoutez d'abord au moins un exercice.",
            TRAINLOG_COLOR_WARNING
        );

        wait_key();
        return;
    }

    if (!choose_session_type(
            &session_type
        )) {
        return;
    }

    if (trainlog_id_generate(
            "se",
            session_id,
            sizeof(session_id)
        ) != TRAINLOG_STATUS_OK ||
        trainlog_time_now_rfc3339(
            started_at,
            sizeof(started_at)
        ) != TRAINLOG_STATUS_OK) {
        return;
    }

    (void)memset(
        drafts,
        0,
        sizeof(drafts)
    );

    /*
     * Start with the first exercise entry immediately. If the user cancels
     * that form, nothing has been committed and the review screen still lets
     * them add another exercise or abandon the whole session.
     */
    if (draft_build_exercise(
            database,
            session_type,
            &drafts[0],
            NULL
        )) {
        exercise_count = 1U;
    }

    if (!edit_session_draft(
            database,
            drafts,
            &exercise_count,
            session_type
        )) {
        draw_shell(
            "Séance abandonnée",
            "Une touche pour revenir"
        );

        status_line(
            "Aucune donnée de séance n'a été enregistrée.",
            TRAINLOG_COLOR_MUTED
        );

        wait_key();
        return;
    }

    if (trainlog_time_now_rfc3339(
            ended_at,
            sizeof(ended_at)
        ) != TRAINLOG_STATUS_OK) {
        return;
    }

    for (index = 0U;
         index < exercise_count;
         ++index) {
        draft_bind_input(
            &drafts[index]
        );

        exercise_inputs[index] =
            drafts[index].input;
    }

    (void)memset(
        &session,
        0,
        sizeof(session)
    );

    session.session_type =
        session_type;

    (void)snprintf(
        session.session_id,
        sizeof(session.session_id),
        "%s",
        session_id
    );

    (void)snprintf(
        session.started_at,
        sizeof(session.started_at),
        "%s",
        started_at
    );

    (void)snprintf(
        session.ended_at,
        sizeof(session.ended_at),
        "%s",
        ended_at
    );

    session.exercises =
        exercise_inputs;

    session.exercise_count =
        exercise_count;

    status =
        trainlog_database_insert_session(
            database,
            &session
        );

    draw_shell(
        "Fin de séance",
        "Une touche pour revenir"
    );

    if (status == TRAINLOG_STATUS_OK) {
        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_SUCCESS
            )
        );

        trainlog_terminal_printf(tui_terminal,
            4,
            2,
            "✓ Séance enregistrée."
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_SUCCESS
            )
        );

        trainlog_terminal_printf(tui_terminal,
            6,
            2,
            "Début : %s",
            started_at
        );

        trainlog_terminal_printf(tui_terminal,
            7,
            2,
            "Fin   : %s",
            ended_at
        );

        trainlog_terminal_printf(tui_terminal,
            8,
            2,
            "Type  : %s",
            session_type_label(
                session_type
            )
        );

        trainlog_terminal_printf(tui_terminal,
            9,
            2,
            "Exercices : %zu",
            exercise_count
        );
    } else {
        status_line(
            "Échec lors de l'enregistrement de la séance.",
            TRAINLOG_COLOR_ERROR
        );
    }

    wait_key();
}

/* TRAINLOG_SESSION_DETAILS_SCREEN */

static const char *session_detail_load_label(TrainlogLoadMode mode)
{
    switch (mode) {
    case TRAINLOG_LOAD_EXTERNAL:
        return "externe";
    case TRAINLOG_LOAD_ASSISTANCE:
        return "assistance";
    case TRAINLOG_LOAD_NONE:
    default:
        return "aucune";
    }
}

static void format_compact_max_weight(
    double value,
    char *output,
    size_t output_size
)
{
    size_t length;

    if (output == NULL || output_size == 0U) {
        return;
    }

    (void)snprintf(output, output_size, "%.2f", value);
    length = strlen(output);
    while (length > 0U && output[length - 1U] == '0') {
        output[--length] = '\0';
    }
    if (length > 0U && output[length - 1U] == '.') {
        output[--length] = '\0';
    }
}

static void draw_max_test_table(
    TrainlogDatabase *database,
    const TrainlogPersistedExerciseDetail *exercises,
    size_t count,
    size_t selected,
    bool decorated
)
{
    int header_row = decorated ? 18 : 7;
    int first_row = header_row + 1;
    int last_row = trainlog_terminal_rows(tui_terminal) - 3;
    size_t visible = last_row >= first_row
        ? (size_t)(last_row - first_row + 1)
        : 1U;
    size_t start = selected >= visible
        ? selected - visible + 1U
        : 0U;
    size_t end = start + visible < count ? start + visible : count;
    size_t index;
    size_t max_count = 0U;
    int column = decorated ? 5 : 4;

    for (index = 0U; index < count; ++index) {
        if (exercises[index].has_max_weight != 0) {
            ++max_count;
        }
    }

    trainlog_terminal_style_on(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(TRAINLOG_COLOR_ACCENT));
    trainlog_terminal_printf(tui_terminal, decorated ? 16 : 6, column,
        "Test de max — %zu résultat(s) MAX", max_count);
    trainlog_terminal_style_off(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(TRAINLOG_COLOR_ACCENT));

    trainlog_terminal_printf(tui_terminal, header_row, column,
        decorated
            ? "%-28s  %-30s  %10s"
            : "%-20s  %-27s  %12s",
        "Exercice", "Machine", "Max");

    for (index = start; index < end; ++index) {
        TrainlogResolvedEquipment resolved;
        const char *equipment_label = "—";
        char max_text[32] = "";
        char max_label[40] = "—";

        if (exercises[index].equipment_id[0] != '\0') {
            equipment_label =
                trainlog_database_resolve_equipment(database,
                    exercises[index].equipment_id, &resolved) ==
                    TRAINLOG_STATUS_OK
                ? resolved.display_name
                : exercises[index].equipment_id;
        }
        if (exercises[index].has_max_weight != 0) {
            format_compact_max_weight(exercises[index].max_weight_kg,
                max_text, sizeof(max_text));
            (void)snprintf(max_label, sizeof(max_label), "%s kg", max_text);
        }

        if (index == selected) {
            trainlog_terminal_style_on(tui_terminal,
                TRAINLOG_TEXT_REVERSE |
                trainlog_theme_style(TRAINLOG_COLOR_SUCCESS));
        }
        trainlog_terminal_printf(tui_terminal,
            first_row + (int)(index - start), column,
            decorated
                ? "%-28.28s  %-30.30s  %10.10s"
                : "%-20.20s  %-27.27s  %12.12s",
            exercises[index].name, equipment_label, max_label);
        if (index == selected) {
            trainlog_terminal_style_off(tui_terminal,
                TRAINLOG_TEXT_REVERSE |
                trainlog_theme_style(TRAINLOG_COLOR_SUCCESS));
        }
    }
}

static void screen_session_detail(
    TrainlogDatabase *database,
    const char *session_id
)
{
    TrainlogSessionSummary session;
    TrainlogPersistedExerciseDetail
        exercises[MAX_SESSION_EXERCISES];

    size_t count = 0U;
    size_t selected = 0U;
    TrainlogStatus status;

    status =
        trainlog_database_get_session_details(
            database,
            session_id,
            &session,
            exercises,
            MAX_SESSION_EXERCISES,
            &count
        );

    if (status != TRAINLOG_STATUS_OK) {
        draw_shell(
            "Détail séance",
            "Une touche pour revenir"
        );

        status_line(
            "Impossible de charger la séance.",
            TRAINLOG_COLOR_ERROR
        );

        wait_key();
        return;
    }

    for (;;) {
        bool decorated =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 32;

        int key;

        if (decorated) {
            trainlog_terminal_erase(tui_terminal);
            trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

            section_ascii_header(
                ":: D E T A I L   S E A N C E ::"
            );

            dashboard_panel(
                8,
                2,
                13,
                trainlog_terminal_columns(tui_terminal) - 3,
                "SEANCE"
            );

            focused_panel(
                14,
                2,
                trainlog_terminal_rows(tui_terminal) - 4,
                trainlog_terminal_columns(tui_terminal) - 3,
                "EXERCICE",
                true
            );

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                "←→/↑↓ exercice précédent/suivant  i fiche équipement  e modifier  b/Échap retour"
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                10,
                5,
                "Début : %s",
                session.started_at
            );

            trainlog_terminal_printf(tui_terminal,
                11,
                5,
                "Fin   : %s",
                session.ended_at[0] != '\0'
                    ? session.ended_at
                    : "séance ouverte"
            );

            trainlog_terminal_printf(tui_terminal,
                12,
                5,
                "Type  : %s",
                session_type_label(
                    session.session_type
                )
            );
        } else if (session.session_type == TRAINLOG_SESSION_MAX_TEST) {
            /* CONTRACT: max history is exercise-indexed; equipment is only
             * rendered as occurrence context and never owns the value. */
            draw_max_test_table(database, exercises, count, selected, decorated);
        } else {
            draw_shell(
                "TRAINLOG — Détail séance",
                "←→/↑↓ naviguer  i fiche équipement  e modifier  b/Échap retour"
            );

            trainlog_terminal_printf(tui_terminal,
                3,
                4,
                "Début : %s",
                session.started_at
            );

            trainlog_terminal_printf(tui_terminal,
                4,
                4,
                "Fin   : %s",
                session.ended_at[0] != '\0'
                    ? session.ended_at
                    : "séance ouverte"
            );

            trainlog_terminal_printf(tui_terminal,
                5,
                4,
                "Type  : %s",
                session_type_label(
                    session.session_type
                )
            );
        }

        if (count == 0U) {
            trainlog_terminal_printf(tui_terminal,
                decorated ? 17 : 7,
                decorated ? 5 : 4,
                "Aucun exercice dans cette séance."
            );
        } else {
            TrainlogPersistedExerciseDetail *exercise =
                &exercises[selected];
            TrainlogResolvedEquipment resolved_equipment;
            bool has_equipment = exercise->equipment_id[0] != '\0' &&
                trainlog_database_resolve_equipment(database,
                    exercise->equipment_id, &resolved_equipment) == TRAINLOG_STATUS_OK;
            const char *equipment_label = has_equipment
                ? resolved_equipment.display_name
                : (exercise->equipment_id[0] != '\0'
                    ? exercise->equipment_id
                    : "—");

            /* CONTRACT: history identifies the selected occurrence by its
             * stable entry_id, so two appearances of Marche stay distinct. */
            trainlog_terminal_printf(tui_terminal,
                decorated ? 19 : 9,
                decorated ? 5 : 4,
                "Occurrence : %s   Équipement : %s",
                exercise->entry_id,
                equipment_label);

            if (exercise->has_max_weight != 0) {
                int title_row = decorated ? 16 : 6;
                int header_row = decorated ? 20 : 10;
                int value_row = decorated ? 21 : 11;

                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(TRAINLOG_COLOR_ACCENT));
                trainlog_terminal_printf(tui_terminal,
                    title_row,
                    decorated ? 5 : 4,
                    "Exercice %zu/%zu — Test de max",
                    selected + 1U,
                    count);
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(TRAINLOG_COLOR_ACCENT));

                trainlog_terminal_printf(tui_terminal,
                    header_row,
                    decorated ? 5 : 4,
                    "%-28s  %-30s  %10s",
                    "Exercice", "Machine", "Max");
                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(TRAINLOG_COLOR_SUCCESS));
                trainlog_terminal_printf(tui_terminal,
                    value_row,
                    decorated ? 5 : 4,
                    "%-28.28s  %-30.30s  %7.2f kg",
                    exercise->name,
                    equipment_label,
                    exercise->max_weight_kg);
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(TRAINLOG_COLOR_SUCCESS));
            } else if (exercise->recording_mode ==
                TRAINLOG_RECORDING_CONTINUOUS) {
                char duration_text[64];

                int title_row =
                    decorated ? 16 : 6;

                int mode_row =
                    decorated ? 18 : 8;

                int first_data_row =
                    decorated ? 20 : 10;

                int row =
                    first_data_row;

                if (trainlog_duration_format(
                        exercise->continuous_duration_seconds,
                        duration_text,
                        sizeof(duration_text)
                    ) != TRAINLOG_STATUS_OK) {
                    (void)snprintf(
                        duration_text,
                        sizeof(duration_text),
                        "%d s",
                        exercise->continuous_duration_seconds
                    );
                }

                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    title_row,
                    decorated ? 5 : 4,
                    "Exercice %zu/%zu — %s",
                    selected + 1U,
                    count,
                    exercise->name
                );

                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    mode_row,
                    decorated ? 5 : 4,
                    "Mode : continu"
                );

                trainlog_terminal_printf(tui_terminal,
                    row++,
                    decorated ? 5 : 4,
                    "Durée : %s",
                    duration_text
                );

                if (exercise->has_continuous_speed != 0) {
                    trainlog_terminal_printf(tui_terminal,
                        row++,
                        decorated ? 5 : 4,
                        "Vitesse : %.1f km/h",
                        exercise->continuous_speed_kmh
                    );
                }

                if (exercise->has_continuous_distance != 0) {
                    trainlog_terminal_printf(tui_terminal,
                        row++,
                        decorated ? 5 : 4,
                        "Distance : %.2f km",
                        exercise->continuous_distance_km
                    );
                }

                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_SUCCESS
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    row + 1,
                    decorated ? 5 : 4,
                    "Réalisé : activité continue"
                );

                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_SUCCESS
                    )
                );
            } else {
                char rest_text[64];
                char target_duration_text[64];

                int title_row =
                    decorated ? 16 : 6;

                int mode_row =
                    decorated ? 18 : 8;

                int target_row =
                    decorated ? 20 : 10;

                int weight_row =
                    decorated ? 21 : 11;

                int actual_row =
                    decorated ? 23 : 13;

                int summary_row =
                    decorated ? 25 : 15;

                int warning_row =
                    decorated ? 27 : 17;

                if (trainlog_duration_format(
                        exercise->rest_seconds,
                        rest_text,
                        sizeof(rest_text)
                    ) != TRAINLOG_STATUS_OK) {
                    (void)snprintf(
                        rest_text,
                        sizeof(rest_text),
                        "%ds",
                        exercise->rest_seconds
                    );
                }

                target_duration_text[0] = '\0';

                if (exercise->tracking_mode ==
                    TRAINLOG_TRACKING_DURATION) {
                    (void)trainlog_duration_format(
                        exercise->target_duration_seconds,
                        target_duration_text,
                        sizeof(target_duration_text)
                    );
                }

                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    title_row,
                    decorated ? 5 : 4,
                    "Exercice %zu/%zu — %s",
                    selected + 1U,
                    count,
                    exercise->name
                );

                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    mode_row,
                    decorated ? 5 : 4,
                    "Mode : %-12s   Charge : %-10s   Repos : %s",
                    exercise->tracking_mode ==
                        TRAINLOG_TRACKING_REPS
                        ? "répétitions"
                        : "durée",
                    session_detail_load_label(
                        exercise->load_mode
                    ),
                    rest_text
                );

                if (
                    exercise->target_sets <= 0
                ) {
                    trainlog_terminal_printf(tui_terminal,
                        target_row,
                        decorated ? 5 : 4,
                        "Cible : non renseignée"
                    );
                } else if (
                    exercise->tracking_mode ==
                    TRAINLOG_TRACKING_REPS
                ) {
                    trainlog_terminal_printf(tui_terminal,
                        target_row,
                        decorated ? 5 : 4,
                        "Cible : %d série(s) × %d reps",
                        exercise->target_sets,
                        exercise->target_reps
                    );
                } else {
                    trainlog_terminal_printf(tui_terminal,
                        target_row,
                        decorated ? 5 : 4,
                        "Cible : %d série(s) × %s",
                        exercise->target_sets,
                        target_duration_text
                    );
                }

                if (exercise->has_target_weight != 0) {
                    trainlog_terminal_printf(tui_terminal,
                        weight_row,
                        decorated ? 5 : 4,
                        "Charge cible : %.1f kg",
                        exercise->target_weight_kg
                    );
                } else {
                    trainlog_terminal_printf(tui_terminal,
                        weight_row,
                        decorated ? 5 : 4,
                        "Charge cible : —"
                    );
                }

                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_SUCCESS
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    actual_row,
                    decorated ? 5 : 4,
                    "Réalisé : %zu série(s)",
                    exercise->actual_set_count
                );

                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_BOLD |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_SUCCESS
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    summary_row,
                    decorated ? 5 : 4,
                    "%.*s",
                    trainlog_terminal_columns(tui_terminal) - 10,
                    exercise->actual_summary
                );

                if (exercise->load_mode ==
                    TRAINLOG_LOAD_ASSISTANCE) {
                    trainlog_terminal_style_on(tui_terminal,
                        trainlog_theme_style(
                            TRAINLOG_COLOR_WARNING
                        )
                    );

                    trainlog_terminal_printf(tui_terminal,
                        warning_row,
                        decorated ? 5 : 4,
                        "Assistance : plus de kg = davantage d'aide."
                    );

                    trainlog_terminal_style_off(tui_terminal,
                        trainlog_theme_style(
                            TRAINLOG_COLOR_WARNING
                        )
                    );
                }
            }
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (key == 'b' ||
            key == 'B' ||
            key == 27) {
            return;
        }

        if (key == 'e' ||
            key == 'E') {
            edit_persisted_session(
                database,
                session_id
            );

            return;
        }

        if (count > 0U && (key == 'i' || key == 'I') &&
            exercises[selected].equipment_id[0] != '\0') {
            TrainlogResolvedEquipment equipment;
            if (trainlog_database_resolve_equipment(database,
                    exercises[selected].equipment_id, &equipment) == TRAINLOG_STATUS_OK) {
                screen_equipment_detail(&equipment);
            }
            continue;
        }

        if (count > 0U &&
            (key == TRAINLOG_KEY_RIGHT ||
             key == TRAINLOG_KEY_DOWN)) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : 0U;
        } else if (
            count > 0U &&
            (key == TRAINLOG_KEY_LEFT ||
             key == TRAINLOG_KEY_UP)
        ) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : count - 1U;
        }
    }
}

/* TRAINLOG_HISTORY_ASCII_HEADER */

static void history_ascii_header(void)
{
    /* CONTRACT: history shares the current page shell; the old local logo
     * occupied six unrelated rows and made this screen an exception. */
    section_ascii_header("Historique");
}

static void history_scrollbar(
    int top,
    int bottom,
    int column,
    size_t selected,
    size_t count,
    size_t visible
)
{
    int track_height;
    int row;
    int thumb;

    if (bottom <= top ||
        count <= visible ||
        count <= 1U) {
        return;
    }

    track_height =
        bottom - top + 1;

    if (track_height < 2) {
        return;
    }

    trainlog_terminal_style_on(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_MUTED
        )
    );

    for (row = top;
         row <= bottom;
         ++row) {
        trainlog_terminal_draw(tui_terminal,
            row,
            column,
            0x2502U
        );
    }

    trainlog_terminal_style_off(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_MUTED
        )
    );

    thumb =
        top +
        (int)(
            (selected *
             (size_t)(track_height - 1)) /
            (count - 1U)
        );

    trainlog_terminal_style_on(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );

    trainlog_terminal_draw(tui_terminal,
        thumb,
        column,
        0x2593U
    );

    trainlog_terminal_style_off(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );
}

static void screen_history(
    TrainlogDatabase *database
)
{
    TrainlogSessionSummary
        sessions[MAX_SESSIONS];

    size_t selected = 0U;
    int nav_selected = 2;
    int focus = 1;

    for (;;) {
        size_t count = 0U;
        size_t top = 0U;
        size_t index;

        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int list_top =
            large_layout
                ? 11
                : 3;

        int list_bottom =
            trainlog_terminal_rows(tui_terminal) - 4;

        int first_row =
            list_top + 1;

        int visible_rows =
            list_bottom -
            first_row;

        int key;

        if (visible_rows < 1) {
            return;
        }

        if (trainlog_database_list_sessions(
                database,
                sessions,
                MAX_SESSIONS,
                &count
            ) != TRAINLOG_STATUS_OK) {
            return;
        }

        trainlog_terminal_erase(tui_terminal);
        trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

        if (large_layout) {
            history_ascii_header();

            primary_top_navbar(
                2,
                nav_selected,
                focus == 0
            );

            focused_panel(
                list_top,
                2,
                list_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "SEANCES ENREGISTREES",
                focus == 1
            );
        } else {
            trainlog_terminal_style_on(tui_terminal,
                TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );

            trainlog_terminal_printf(tui_terminal,
                1,
                2,
                " TRAINLOG — Historique "
            );

            trainlog_terminal_style_off(tui_terminal,
                TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        }

        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_printf(tui_terminal,
            trainlog_terminal_rows(tui_terminal) - 2,
            2,
            "%.*s",
            trainlog_terminal_columns(tui_terminal) - 4,
            large_layout
                ? "Tab zone  ↑↓/PgUp/PgDn liste  ←→ menu  Entrée ouvrir  e modifier  0/Home accueil  F1-F5 direct  b/Échap retour"
                : "↑↓ naviguer  Entrée détail  e modifier  b/Échap retour"
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        if (count == 0U) {
            trainlog_terminal_printf(tui_terminal,
                first_row + 1,
                large_layout ? 6 : 4,
                "Aucune séance."
            );
        }

        if (count > 0U) {
            if (selected >= count) {
                selected =
                    count - 1U;
            }

            if (selected >=
                (size_t)visible_rows) {
                top =
                    selected -
                    (size_t)visible_rows +
                    1U;
            }

            for (index = 0U;
                 index < (size_t)visible_rows &&
                 top + index < count;
                 ++index) {
                size_t absolute =
                    top + index;

                int item_row =
                    first_row +
                    (int)index;

                int item_col =
                    large_layout ? 5 : 4;

                if (focus == 1 &&
                    absolute == selected) {
                    trainlog_terminal_style_on(tui_terminal,
                        TRAINLOG_TEXT_REVERSE |
                        trainlog_theme_style(
                            TRAINLOG_COLOR_ACCENT
                        )
                    );
                }

                {
                    char display_date[17];

                    session_history_datetime(
                        sessions[absolute].started_at,
                        display_date
                    );

                    trainlog_terminal_printf(tui_terminal,
                        item_row,
                        item_col,
                        " %-16s  %-14s  %2zu exercice(s) ",
                        display_date,
                        session_type_history_label(
                            sessions[absolute].session_type
                        ),
                        sessions[absolute].exercise_count
                    );
                }

                if (focus == 1 &&
                    absolute == selected) {
                    trainlog_terminal_style_off(tui_terminal,
                        TRAINLOG_TEXT_REVERSE |
                        trainlog_theme_style(
                            TRAINLOG_COLOR_ACCENT
                        )
                    );
                }
            }

            if (large_layout) {
                history_scrollbar(
                    first_row,
                    list_bottom - 1,
                    trainlog_terminal_columns(tui_terminal) - 5,
                    selected,
                    count,
                    (size_t)visible_rows
                );
            }
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (large_layout &&
            (key == TRAINLOG_KEY_TAB ||
             key == TRAINLOG_KEY_SHIFT_TAB)) {
            focus =
                focus == 0
                    ? 1
                    : 0;
            continue;
        }

        if (large_layout &&
            primary_top_nav_forward(key)) {
            return;
        }

        if (key == 'b' ||
            key == 'B' ||
            key == 27) {
            return;
        }

        if (large_layout &&
            focus == 0) {
            if (key == TRAINLOG_KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : PRIMARY_NAV_COUNT - 1;
            } else if (key == TRAINLOG_KEY_RIGHT) {
                nav_selected =
                    nav_selected < PRIMARY_NAV_COUNT - 1
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == TRAINLOG_KEY_ENTER
            ) {
                if (primary_top_nav_activate(
                        nav_selected
                    )) {
                    return;
                }
            }

            continue;
        }

        if (count == 0U) {
            continue;
        }

        if (key == TRAINLOG_KEY_UP) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : 0U;

            continue;
        }

        if (key == TRAINLOG_KEY_DOWN) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : count - 1U;

            continue;
        }

        if (key == TRAINLOG_KEY_PAGE_UP) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected > jump
                    ? selected - jump
                    : 0U;

            continue;
        }

        if (key == TRAINLOG_KEY_PAGE_DOWN) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected + jump < count
                    ? selected + jump
                    : count - 1U;

            continue;
        }

        if (key == '\n' ||
            key == TRAINLOG_KEY_ENTER) {
            screen_session_detail(
                database,
                sessions[selected].session_id
            );

            continue;
        }

        if (key == 'e' ||
            key == 'E') {
            edit_persisted_session(
                database,
                sessions[selected].session_id
            );
        }
    }
}

/* TRAINLOG_BODY_RECORDS_TUI */

#define MAX_BODY_OBSERVATIONS 512U

static void body_panel(
    int top,
    int left,
    int bottom,
    int right,
    const char *label
)
{
    TrainlogPanel *panel;
    int height;
    int width;

    if (top < 0 ||
        left < 0 ||
        bottom <= top ||
        right <= left ||
        bottom >= trainlog_terminal_rows(tui_terminal) ||
        right >= trainlog_terminal_columns(tui_terminal)) {
        return;
    }

    height = bottom - top + 1;
    width = right - left + 1;

    panel = tui_panel_create(tui_terminal, height,
        width,
        top,
        left
    );

    if (panel == NULL) {
        return;
    }

    tui_panel_box(panel);

    if (label != NULL &&
        label[0] != '\0' &&
        width > 8) {
        tui_panel_style_on(
            panel,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        tui_panel_print(
            panel,
            0,
            2,
            " %.*s ",
            width - 6,
            label
        );

        tui_panel_style_off(
            panel,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );
    }

    tui_panel_commit(panel);
    tui_panel_destroy(panel);
}

static void body_short_date(
    const char *timestamp,
    char output[9]
)
{
    if (timestamp == NULL ||
        strlen(timestamp) < 10U) {
        (void)snprintf(
            output,
            9U,
            "%s",
            "--/--/--"
        );

        return;
    }

    (void)snprintf(
        output,
        9U,
        "%c%c/%c%c/%c%c",
        timestamp[8],
        timestamp[9],
        timestamp[5],
        timestamp[6],
        timestamp[2],
        timestamp[3]
    );
}

static size_t body_record_metric_count(
    const TrainlogBodyObservationRecord *record
)
{
    size_t count = 0U;

    if (record == NULL) {
        return 0U;
    }

#define COUNT_BODY_VALUE(flag_)                                              \
    do {                                                                     \
        if ((flag_)) {                                                       \
            ++count;                                                         \
        }                                                                    \
    } while (0)

    COUNT_BODY_VALUE(record->has_body_weight);
    COUNT_BODY_VALUE(record->has_neck);
    COUNT_BODY_VALUE(record->has_shoulders);
    COUNT_BODY_VALUE(record->has_chest);
    COUNT_BODY_VALUE(record->has_waist);
    COUNT_BODY_VALUE(record->has_hips);
    COUNT_BODY_VALUE(record->has_left_arm);
    COUNT_BODY_VALUE(record->has_right_arm);
    COUNT_BODY_VALUE(record->has_left_forearm);
    COUNT_BODY_VALUE(record->has_right_forearm);
    COUNT_BODY_VALUE(record->has_left_thigh);
    COUNT_BODY_VALUE(record->has_right_thigh);
    COUNT_BODY_VALUE(record->has_left_calf);
    COUNT_BODY_VALUE(record->has_right_calf);

#undef COUNT_BODY_VALUE

    return count;
}

static void body_draw_scrollbar(
    int top,
    int bottom,
    int column,
    size_t selected,
    size_t count,
    size_t visible
)
{
    int track_height;
    int row;
    int thumb;

    if (bottom <= top ||
        count <= visible ||
        count <= 1U) {
        return;
    }

    track_height =
        bottom - top + 1;

    if (track_height < 2) {
        return;
    }

    trainlog_terminal_style_on(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_MUTED
        )
    );

    for (row = top;
         row <= bottom;
         ++row) {
        trainlog_terminal_draw(tui_terminal,
            row,
            column,
            0x2502U
        );
    }

    trainlog_terminal_style_off(tui_terminal,
        trainlog_theme_style(
            TRAINLOG_COLOR_MUTED
        )
    );

    thumb =
        top +
        (int)(
            (selected *
             (size_t)(track_height - 1)) /
            (count - 1U)
        );

    trainlog_terminal_style_on(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );

    trainlog_terminal_draw(tui_terminal,
        thumb,
        column,
        0x2593U
    );

    trainlog_terminal_style_off(tui_terminal,
        TRAINLOG_TEXT_BOLD |
        trainlog_theme_style(
            TRAINLOG_COLOR_ACCENT
        )
    );
}

static void body_summary_text(
    const TrainlogBodyObservationRecord *record,
    char *output,
    size_t output_size
)
{
    char weight[24];
    char waist[24];
    char chest[24];
    size_t metric_count;

    if (record == NULL ||
        output == NULL ||
        output_size == 0U) {
        return;
    }

    if (record->has_body_weight) {
        (void)snprintf(
            weight,
            sizeof(weight),
            "%.1f kg",
            record->body_weight_kg
        );
    } else {
        (void)snprintf(
            weight,
            sizeof(weight),
            "%s",
            "— kg"
        );
    }

    if (record->has_waist) {
        (void)snprintf(
            waist,
            sizeof(waist),
            "taille %.1f",
            record->waist_cm
        );
    } else {
        (void)snprintf(
            waist,
            sizeof(waist),
            "%s",
            "taille —"
        );
    }

    if (record->has_chest) {
        (void)snprintf(
            chest,
            sizeof(chest),
            "poitrine %.1f",
            record->chest_cm
        );
    } else {
        (void)snprintf(
            chest,
            sizeof(chest),
            "%s",
            "poitrine —"
        );
    }

    metric_count =
        body_record_metric_count(record);

    (void)snprintf(
        output,
        output_size,
        "%-9s  %-9s  %-14s  %2zu valeur(s)",
        weight,
        waist,
        chest,
        metric_count
    );
}

static bool prompt_optional_double_existing(
    int row,
    const char *label,
    bool *present,
    double *value
)
{
    char prompt[160];
    char buffer[64];

    if (label == NULL ||
        present == NULL ||
        value == NULL) {
        return false;
    }

    if (*present) {
        (void)snprintf(
            prompt,
            sizeof(prompt),
            "%s [%.1f] (Entrée=garder, -=effacer) : ",
            label,
            *value
        );
    } else {
        (void)snprintf(
            prompt,
            sizeof(prompt),
            "%s [vide] : ",
            label
        );
    }

    if (!prompt_text(
            row,
            prompt,
            buffer,
            sizeof(buffer),
            true
        )) {
        return false;
    }

    if (buffer[0] == '\0') {
        return true;
    }

    if (strcmp(buffer, "-") == 0) {
        *present = false;
        *value = 0.0;
        return true;
    }

    if (!parse_double_positive(
            buffer,
            value
        )) {
        status_line(
            "Nombre positif invalide.",
            TRAINLOG_COLOR_ERROR
        );

        wait_key();
        return false;
    }

    *present = true;
    return true;
}

static void body_input_from_record(
    const TrainlogBodyObservationRecord *record,
    TrainlogBodyObservationInput *input
)
{
    if (record == NULL ||
        input == NULL) {
        return;
    }

    (void)memset(
        input,
        0,
        sizeof(*input)
    );

    (void)snprintf(
        input->observation_id,
        sizeof(input->observation_id),
        "%s",
        record->observation_id
    );

    (void)snprintf(
        input->observed_at,
        sizeof(input->observed_at),
        "%s",
        record->observed_at
    );

    input->has_body_weight =
        record->has_body_weight;

    input->body_weight_kg =
        record->body_weight_kg;

    input->has_neck =
        record->has_neck;

    input->neck_cm =
        record->neck_cm;

    input->has_shoulders =
        record->has_shoulders;

    input->shoulders_cm =
        record->shoulders_cm;

    input->has_chest =
        record->has_chest;

    input->chest_cm =
        record->chest_cm;

    input->has_waist =
        record->has_waist;

    input->waist_cm =
        record->waist_cm;

    input->has_hips =
        record->has_hips;

    input->hips_cm =
        record->hips_cm;

    input->has_left_arm =
        record->has_left_arm;

    input->left_arm_cm =
        record->left_arm_cm;

    input->has_right_arm =
        record->has_right_arm;

    input->right_arm_cm =
        record->right_arm_cm;

    input->has_left_forearm =
        record->has_left_forearm;

    input->left_forearm_cm =
        record->left_forearm_cm;

    input->has_right_forearm =
        record->has_right_forearm;

    input->right_forearm_cm =
        record->right_forearm_cm;

    input->has_left_thigh =
        record->has_left_thigh;

    input->left_thigh_cm =
        record->left_thigh_cm;

    input->has_right_thigh =
        record->has_right_thigh;

    input->right_thigh_cm =
        record->right_thigh_cm;

    input->has_left_calf =
        record->has_left_calf;

    input->left_calf_cm =
        record->left_calf_cm;

    input->has_right_calf =
        record->has_right_calf;

    input->right_calf_cm =
        record->right_calf_cm;

    input->notes =
        record->notes[0] != '\0'
            ? record->notes
            : NULL;
}

static bool body_edit_record(
    TrainlogDatabase *database,
    const char *observation_id
)
{
    TrainlogBodyObservationRecord record;
    TrainlogBodyObservationInput input;
    int row;

    if (trainlog_database_get_body_observation(
            database,
            observation_id,
            &record
        ) != TRAINLOG_STATUS_OK) {
        return false;
    }

    body_input_from_record(
        &record,
        &input
    );

    draw_shell(
        "Modifier relevé — 1/2",
        "Entrée garde · - efface une mesure · Échap annule tout"
    );

    row = 4;

#define EDIT_BODY_VALUE(label_, flag_, value_)                               \
    do {                                                                     \
        if (!prompt_optional_double_existing(                                \
                row++,                                                       \
                (label_),                                                    \
                &(flag_),                                                    \
                &(value_)                                                    \
            )) {                                                             \
            return false;                                                    \
        }                                                                    \
    } while (0)

    EDIT_BODY_VALUE(
        "Poids kg",
        input.has_body_weight,
        input.body_weight_kg
    );

    EDIT_BODY_VALUE(
        "Cou cm",
        input.has_neck,
        input.neck_cm
    );

    EDIT_BODY_VALUE(
        "Épaules cm",
        input.has_shoulders,
        input.shoulders_cm
    );

    EDIT_BODY_VALUE(
        "Poitrine cm",
        input.has_chest,
        input.chest_cm
    );

    EDIT_BODY_VALUE(
        "Tour de taille cm",
        input.has_waist,
        input.waist_cm
    );

    EDIT_BODY_VALUE(
        "Hanches cm",
        input.has_hips,
        input.hips_cm
    );

    draw_shell(
        "Modifier relevé — 2/2",
        "Entrée garde · - efface une mesure · Échap annule tout"
    );

    row = 4;

    EDIT_BODY_VALUE(
        "Bras gauche cm",
        input.has_left_arm,
        input.left_arm_cm
    );

    EDIT_BODY_VALUE(
        "Bras droit cm",
        input.has_right_arm,
        input.right_arm_cm
    );

    EDIT_BODY_VALUE(
        "Avant-bras gauche cm",
        input.has_left_forearm,
        input.left_forearm_cm
    );

    EDIT_BODY_VALUE(
        "Avant-bras droit cm",
        input.has_right_forearm,
        input.right_forearm_cm
    );

    EDIT_BODY_VALUE(
        "Cuisse gauche cm",
        input.has_left_thigh,
        input.left_thigh_cm
    );

    EDIT_BODY_VALUE(
        "Cuisse droite cm",
        input.has_right_thigh,
        input.right_thigh_cm
    );

    EDIT_BODY_VALUE(
        "Mollet gauche cm",
        input.has_left_calf,
        input.left_calf_cm
    );

    EDIT_BODY_VALUE(
        "Mollet droit cm",
        input.has_right_calf,
        input.right_calf_cm
    );

#undef EDIT_BODY_VALUE

    if (trainlog_database_update_body_observation(
            database,
            &input
        ) == TRAINLOG_STATUS_OK) {
        status_line(
            "✓ Relevé corrigé.",
            TRAINLOG_COLOR_SUCCESS
        );

        wait_key();
        return true;
    }

    status_line(
        "Correction impossible. Au moins une mesure doit rester présente.",
        TRAINLOG_COLOR_ERROR
    );

    wait_key();
    return false;
}

static void body_detail_value(
    int row,
    int column,
    const char *label,
    bool present,
    double value,
    const char *unit
)
{
    if (present) {
        trainlog_terminal_printf(tui_terminal,
            row,
            column,
            "%-24s %7.1f %s",
            label,
            value,
            unit
        );
    } else {
        trainlog_terminal_printf(tui_terminal,
            row,
            column,
            "%-24s %7s",
            label,
            "—"
        );
    }
}

static void screen_body_observation_detail(
    TrainlogDatabase *database,
    const char *observation_id
)
{
    int page = 0;

    for (;;) {
        TrainlogBodyObservationRecord record;
        char date[9];
        bool decorated =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 34;
        int frame_top =
            decorated ? 8 : 3;
        int key;

        if (trainlog_database_get_body_observation(
                database,
                observation_id,
                &record
            ) != TRAINLOG_STATUS_OK) {
            status_line(
                "Impossible de charger le relevé.",
                TRAINLOG_COLOR_ERROR
            );

            wait_key();
            return;
        }

        body_short_date(
            record.observed_at,
            date
        );

        if (decorated) {
            trainlog_terminal_erase(tui_terminal);
            trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

            section_ascii_header(
                ":: D E T A I L   C O R P S ::"
            );

            focused_panel(
                frame_top,
                2,
                trainlog_terminal_rows(tui_terminal) - 4,
                trainlog_terminal_columns(tui_terminal) - 3,
                page == 0
                    ? "RELEVE — GENERAL"
                    : "RELEVE — MEMBRES",
                true
            );

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                "←→ page  e Modifier  b/Échap retour"
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Détail relevé",
                "←→ page  e Modifier  b/Échap retour"
            );

            body_panel(
                3,
                2,
                trainlog_terminal_rows(tui_terminal) - 4,
                trainlog_terminal_columns(tui_terminal) - 3,
                page == 0
                    ? "RELEVE — GENERAL"
                    : "RELEVE — MEMBRES"
            );
        }

        trainlog_terminal_style_on(tui_terminal,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        trainlog_terminal_printf(tui_terminal,
            frame_top + 2,
            5,
            "%s   page %d/2",
            date,
            page + 1
        );

        trainlog_terminal_style_off(tui_terminal,
            TRAINLOG_TEXT_BOLD |
            trainlog_theme_style(
                TRAINLOG_COLOR_ACCENT
            )
        );

        if (page == 0) {
            body_detail_value(
                frame_top + 5,
                6,
                "Poids",
                record.has_body_weight,
                record.body_weight_kg,
                "kg"
            );

            body_detail_value(
                frame_top + 7,
                6,
                "Cou",
                record.has_neck,
                record.neck_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 9,
                6,
                "Épaules",
                record.has_shoulders,
                record.shoulders_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 11,
                6,
                "Poitrine",
                record.has_chest,
                record.chest_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 13,
                6,
                "Tour de taille",
                record.has_waist,
                record.waist_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 15,
                6,
                "Hanches",
                record.has_hips,
                record.hips_cm,
                "cm"
            );
        } else {
            body_detail_value(
                frame_top + 5,
                6,
                "Bras gauche",
                record.has_left_arm,
                record.left_arm_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 7,
                6,
                "Bras droit",
                record.has_right_arm,
                record.right_arm_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 9,
                6,
                "Avant-bras gauche",
                record.has_left_forearm,
                record.left_forearm_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 11,
                6,
                "Avant-bras droit",
                record.has_right_forearm,
                record.right_forearm_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 13,
                6,
                "Cuisse gauche",
                record.has_left_thigh,
                record.left_thigh_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 15,
                6,
                "Cuisse droite",
                record.has_right_thigh,
                record.right_thigh_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 17,
                6,
                "Mollet gauche",
                record.has_left_calf,
                record.left_calf_cm,
                "cm"
            );

            body_detail_value(
                frame_top + 19,
                6,
                "Mollet droit",
                record.has_right_calf,
                record.right_calf_cm,
                "cm"
            );
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (key == 'b' ||
            key == 'B' ||
            key == 27) {
            return;
        }

        if (key == TRAINLOG_KEY_LEFT ||
            key == TRAINLOG_KEY_RIGHT) {
            page =
                page == 0
                    ? 1
                    : 0;
            continue;
        }

        if (key == 'e' ||
            key == 'E') {
            (void)body_edit_record(
                database,
                observation_id
            );
        }
    }
}

/* TRAINLOG_BODY_ANALYTICS_TUI_V1 */

static bool body_analytics_profile_path(
    char *output,
    size_t output_size,
    bool ensure_directory
)
{
    const char *config_home =
        getenv(
            "XDG_CONFIG_HOME"
        );

    const char *home =
        getenv(
            "HOME"
        );

    char base[
        PATH_MAX + 1U
    ];

    char directory[
        PATH_MAX + 1U
    ];

    int written;

    if (
        output == NULL ||
        output_size == 0U
    ) {
        return false;
    }

    if (
        config_home != NULL &&
        config_home[0] != '\0'
    ) {
        written =
            snprintf(
                base,
                sizeof(base),
                "%s",
                config_home
            );
    } else if (
        home != NULL &&
        home[0] != '\0'
    ) {
        written =
            snprintf(
                base,
                sizeof(base),
                "%s/.config",
                home
            );

        if (
            written >= 0 &&
            (size_t)written <
                sizeof(base) &&
            ensure_directory
        ) {
            (void)mkdir(
                base,
                0700
            );
        }
    } else {
        return false;
    }

    if (
        written < 0 ||
        (size_t)written >=
            sizeof(base)
    ) {
        return false;
    }

    written =
        snprintf(
            directory,
            sizeof(directory),
            "%s/trainlog",
            base
        );

    if (
        written < 0 ||
        (size_t)written >=
            sizeof(directory)
    ) {
        return false;
    }

    if (ensure_directory) {
        (void)mkdir(
            directory,
            0700
        );
    }

    written =
        snprintf(
            output,
            output_size,
            "%s/body_analytics.conf",
            directory
        );

    return
        written >= 0 &&
        (size_t)written <
            output_size;
}

static bool body_analytics_profile_load(
    TrainlogBodyAnalyticsProfile *output
)
{
    char path[
        PATH_MAX + 1U
    ];

    char line[128];
    int version = 0;
    int formula = -1;
    double height_cm = 0.0;
    FILE *file;

    if (
        output == NULL ||
        !body_analytics_profile_path(
            path,
            sizeof(path),
            false
        )
    ) {
        return false;
    }

    file =
        fopen(
            path,
            "rb"
        );

    if (file == NULL) {
        return false;
    }

    while (
        fgets(
            line,
            sizeof(line),
            file
        ) != NULL
    ) {
        if (
            sscanf(
                line,
                "version=%d",
                &version
            ) == 1
        ) {
            continue;
        }

        if (
            strcmp(
                line,
                "formula=male\n"
            ) == 0 ||
            strcmp(
                line,
                "formula=male"
            ) == 0
        ) {
            formula =
                (int)
                TRAINLOG_BODY_ANALYTICS_FORMULA_MALE;

            continue;
        }

        if (
            strcmp(
                line,
                "formula=female\n"
            ) == 0 ||
            strcmp(
                line,
                "formula=female"
            ) == 0
        ) {
            formula =
                (int)
                TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE;

            continue;
        }

        (void)sscanf(
            line,
            "height_cm=%lf",
            &height_cm
        );
    }

    (void)fclose(
        file
    );

    if (
        version != 1 ||
        formula < 0
    ) {
        return false;
    }

    output->formula =
        (TrainlogBodyAnalyticsFormula)
        formula;

    output->height_cm =
        height_cm;

    return
        trainlog_body_analytics_profile_valid(
            output
        );
}

static bool body_analytics_profile_save(
    const TrainlogBodyAnalyticsProfile *profile
)
{
    char path[
        PATH_MAX + 1U
    ];

    FILE *file;

    if (
        !trainlog_body_analytics_profile_valid(
            profile
        ) ||
        !body_analytics_profile_path(
            path,
            sizeof(path),
            true
        )
    ) {
        return false;
    }

    file =
        fopen(
            path,
            "wb"
        );

    if (file == NULL) {
        return false;
    }

    if (
        fprintf(
            file,
            "version=1\n"
            "formula=%s\n"
            "height_cm=%.2f\n",
            profile->formula ==
                TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
                ? "female"
                : "male",
            profile->height_cm
        ) < 0
    ) {
        (void)fclose(
            file
        );

        return false;
    }

    return
        fclose(
            file
        ) == 0;
}

static bool body_analytics_profile_prompt(
    TrainlogBodyAnalyticsProfile *profile
)
{
    char height_text[64];
    int formula;
    double height_cm;

    if (profile == NULL) {
        return false;
    }

    formula =
        trainlog_body_analytics_profile_valid(
            profile
        )
            ? (
                profile->formula ==
                    TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
                    ? 2
                    : 1
            )
            : 1;

    height_cm =
        trainlog_body_analytics_profile_valid(
            profile
        )
            ? profile->height_cm
            : 170.0;

    draw_shell(
        "TRAINLOG — Profil d'estimation",
        "Entrée valide · Échap annule"
    );

    if (
        !prompt_int_value(
            4,
            "Formule 1=homme 2=femme",
            1,
            2,
            formula,
            &formula
        )
    ) {
        return false;
    }

    for (;;) {
        char label[128];

        (void)snprintf(
            label,
            sizeof(label),
            "Taille cm [%.1f] : ",
            height_cm
        );

        if (
            !prompt_text(
                6,
                label,
                height_text,
                sizeof(height_text),
                true
            )
        ) {
            return false;
        }

        if (
            height_text[0] == '\0'
        ) {
            break;
        }

        if (
            parse_double_positive(
                height_text,
                &height_cm
            ) &&
            height_cm >= 100.0 &&
            height_cm <= 250.0
        ) {
            break;
        }

        status_line(
            "Taille attendue entre 100 et 250 cm.",
            TRAINLOG_COLOR_ERROR
        );

        trainlog_terminal_render(tui_terminal);
    }

    profile->formula =
        formula == 2
            ? TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
            : TRAINLOG_BODY_ANALYTICS_FORMULA_MALE;

    profile->height_cm =
        height_cm;

    return
        body_analytics_profile_save(
            profile
        );
}

static const TrainlogBodyObservationRecord *
body_analytics_oldest_weight(
    const TrainlogBodyObservationRecord *records,
    size_t count
)
{
    size_t index;

    if (
        records == NULL ||
        count == 0U
    ) {
        return NULL;
    }

    for (
        index = count;
        index > 0U;
        --index
    ) {
        if (
            records[index - 1U]
                .has_body_weight
        ) {
            return
                &records[index - 1U];
        }
    }

    return NULL;
}

static const TrainlogBodyObservationRecord *
body_analytics_oldest_waist(
    const TrainlogBodyObservationRecord *records,
    size_t count
)
{
    size_t index;

    if (
        records == NULL ||
        count == 0U
    ) {
        return NULL;
    }

    for (
        index = count;
        index > 0U;
        --index
    ) {
        if (
            records[index - 1U]
                .has_waist
        ) {
            return
                &records[index - 1U];
        }
    }

    return NULL;
}

static bool body_analytics_oldest_estimate(
    const TrainlogBodyAnalyticsProfile *profile,
    const TrainlogBodyObservationRecord *records,
    size_t count,
    TrainlogBodyAnalyticsResult *output
)
{
    size_t index;

    if (
        profile == NULL ||
        records == NULL ||
        output == NULL
    ) {
        return false;
    }

    for (
        index = count;
        index > 0U;
        --index
    ) {
        TrainlogBodyAnalyticsResult
            candidate;

        if (
            trainlog_body_analytics_calculate(
                profile,
                &records[index - 1U],
                &candidate
            ) ==
                TRAINLOG_STATUS_OK &&
            candidate
                .has_body_fat_estimate
        ) {
            *output =
                candidate;

            return true;
        }
    }

    return false;
}

static void body_analytics_value(
    int row,
    const char *label,
    bool present,
    double value,
    const char *suffix
)
{
    if (present) {
        trainlog_terminal_printf(tui_terminal,
            row,
            6,
            "%-25s %8.2f %s",
            label,
            value,
            suffix != NULL
                ? suffix
                : ""
        );
    } else {
        trainlog_terminal_printf(tui_terminal,
            row,
            6,
            "%-25s %8s",
            label,
            "—"
        );
    }
}

static void screen_body_analytics(
    TrainlogDatabase *database
)
{
    TrainlogBodyObservationRecord
        records[MAX_BODY_OBSERVATIONS];

    size_t count = 0U;
    int page = 0;

    TrainlogBodyAnalyticsProfile profile;

    bool has_profile =
        body_analytics_profile_load(
            &profile
        );

    if (
        database == NULL ||
        trainlog_database_list_body_observations(
            database,
            records,
            MAX_BODY_OBSERVATIONS,
            &count
        ) != TRAINLOG_STATUS_OK
    ) {
        return;
    }

    for (;;) {
        TrainlogBodyAnalyticsResult
            current;

        TrainlogBodyAnalyticsResult
            oldest_estimate;

        const TrainlogBodyObservationRecord *latest =
            count > 0U
                ? &records[0]
                : NULL;

        const TrainlogBodyObservationRecord *oldest_weight =
            body_analytics_oldest_weight(
                records,
                count
            );

        const TrainlogBodyObservationRecord *oldest_waist =
            body_analytics_oldest_waist(
                records,
                count
            );

        bool has_current =
            latest != NULL &&
            trainlog_body_analytics_calculate(
                has_profile
                    ? &profile
                    : NULL,
                latest,
                &current
            ) ==
                TRAINLOG_STATUS_OK;

        bool has_oldest_estimate =
            has_profile &&
            body_analytics_oldest_estimate(
                &profile,
                records,
                count,
                &oldest_estimate
            );

        bool decorated =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int panel_top =
            decorated ? 8 : 3;

        int panel_bottom =
            trainlog_terminal_rows(tui_terminal) - 4;

        int key;

        trainlog_terminal_erase(tui_terminal);
        trainlog_terminal_box(tui_terminal, 0, 0,
                              trainlog_terminal_rows(tui_terminal) - 1,
                              trainlog_terminal_columns(tui_terminal) - 1);

        if (decorated) {
            section_ascii_header(
                ":: A N A L Y S E   C O R P O R E L L E ::"
            );

            focused_panel(
                panel_top,
                2,
                panel_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                page == 0
                    ? "COMPOSITION ET TENDANCE"
                    : "PROPORTIONS ET SYMETRIE",
                true
            );
        } else {
            draw_shell(
                "TRAINLOG — Analyse corporelle",
                "←→ page  p profil  b/Échap retour"
            );

            body_panel(
                panel_top,
                2,
                panel_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                page == 0
                    ? "COMPOSITION"
                    : "PROPORTIONS"
            );
        }

        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_printf(tui_terminal,
            trainlog_terminal_rows(tui_terminal) - 2,
            2,
            "%.*s",
            trainlog_terminal_columns(tui_terminal) - 4,
            "←→ page  p profil estimation  b/Échap retour"
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        if (count == 0U) {
            trainlog_terminal_printf(tui_terminal,
                panel_top + 2,
                6,
                "Aucun relevé corporel."
            );

            trainlog_terminal_printf(tui_terminal,
                panel_top + 4,
                6,
                "Ajoutez d'abord un relevé réel."
            );
        } else if (page == 0) {
            char date[9];

            body_short_date(
                latest->observed_at,
                date
            );

            trainlog_terminal_style_on(tui_terminal,
                TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );

            trainlog_terminal_printf(tui_terminal,
                panel_top + 2,
                6,
                "Dernier relevé : %s",
                date
            );

            trainlog_terminal_style_off(tui_terminal,
                TRAINLOG_TEXT_BOLD |
                trainlog_theme_style(
                    TRAINLOG_COLOR_ACCENT
                )
            );

            if (has_profile) {
                trainlog_terminal_printf(tui_terminal,
                    panel_top + 4,
                    6,
                    "Profil estimation : %s · %.1f cm",
                    profile.formula ==
                        TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
                        ? "formule femme"
                        : "formule homme",
                    profile.height_cm
                );
            } else {
                trainlog_terminal_style_on(tui_terminal,
                    trainlog_theme_style(
                        TRAINLOG_COLOR_WARNING
                    )
                );

                trainlog_terminal_printf(tui_terminal,
                    panel_top + 4,
                    6,
                    "Profil estimation non configuré · p pour configurer."
                );

                trainlog_terminal_style_off(tui_terminal,
                    trainlog_theme_style(
                        TRAINLOG_COLOR_WARNING
                    )
                );
            }

            body_analytics_value(
                panel_top + 6,
                "Graisse estimée",
                has_current &&
                    current
                        .has_body_fat_estimate,
                has_current
                    ? current
                        .body_fat_percent
                    : 0.0,
                "%"
            );

            body_analytics_value(
                panel_top + 7,
                "Masse grasse estimée",
                has_current &&
                    current
                        .has_fat_mass_estimate,
                has_current
                    ? current
                        .fat_mass_kg
                    : 0.0,
                "kg"
            );

            body_analytics_value(
                panel_top + 8,
                "Masse maigre estimée",
                has_current &&
                    current
                        .has_lean_mass_estimate,
                has_current
                    ? current
                        .lean_mass_kg
                    : 0.0,
                "kg"
            );

            if (
                latest->has_body_weight
            ) {
                double delta =
                    oldest_weight != NULL
                        ? latest->body_weight_kg -
                            oldest_weight
                                ->body_weight_kg
                        : 0.0;

                trainlog_terminal_printf(tui_terminal,
                    panel_top + 10,
                    6,
                    "Poids : %.1f kg  · variation depuis 1er poids : %+.1f kg",
                    latest->body_weight_kg,
                    delta
                );
            } else {
                trainlog_terminal_printf(tui_terminal,
                    panel_top + 10,
                    6,
                    "Poids : —"
                );
            }

            if (
                latest->has_waist
            ) {
                double delta =
                    oldest_waist != NULL
                        ? latest->waist_cm -
                            oldest_waist
                                ->waist_cm
                        : 0.0;

                trainlog_terminal_printf(tui_terminal,
                    panel_top + 11,
                    6,
                    "Tour de taille : %.1f cm  · variation : %+.1f cm",
                    latest->waist_cm,
                    delta
                );
            } else {
                trainlog_terminal_printf(tui_terminal,
                    panel_top + 11,
                    6,
                    "Tour de taille : —"
                );
            }

            if (
                has_current &&
                current
                    .has_body_fat_estimate &&
                has_oldest_estimate
            ) {
                trainlog_terminal_printf(tui_terminal,
                    panel_top + 12,
                    6,
                    "Variation graisse estimée : %+.2f point(s)",
                    current
                        .body_fat_percent -
                    oldest_estimate
                        .body_fat_percent
                );
            }

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_WARNING
                )
            );

            trainlog_terminal_printf(tui_terminal,
                panel_top + 14,
                6,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 14,
                "Estimation anthropométrique : tendance utile, pas mesure directe de composition."
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_WARNING
                )
            );
        } else {
            body_analytics_value(
                panel_top + 3,
                "Taille / hanches",
                has_current &&
                    current
                        .has_waist_hip_ratio,
                has_current
                    ? current
                        .waist_hip_ratio
                    : 0.0,
                ""
            );

            body_analytics_value(
                panel_top + 4,
                "Épaules / taille",
                has_current &&
                    current
                        .has_shoulder_waist_ratio,
                has_current
                    ? current
                        .shoulder_waist_ratio
                    : 0.0,
                ""
            );

            body_analytics_value(
                panel_top + 5,
                "Poitrine / taille",
                has_current &&
                    current
                        .has_chest_waist_ratio,
                has_current
                    ? current
                        .chest_waist_ratio
                    : 0.0,
                ""
            );

            trainlog_terminal_printf(tui_terminal,
                panel_top + 8,
                6,
                "ASYMETRIE GAUCHE / DROITE"
            );

            body_analytics_value(
                panel_top + 10,
                "Bras",
                has_current &&
                    current
                        .has_arm_asymmetry,
                has_current
                    ? current
                        .arm_asymmetry_percent
                    : 0.0,
                "%"
            );

            body_analytics_value(
                panel_top + 11,
                "Avant-bras",
                has_current &&
                    current
                        .has_forearm_asymmetry,
                has_current
                    ? current
                        .forearm_asymmetry_percent
                    : 0.0,
                "%"
            );

            body_analytics_value(
                panel_top + 12,
                "Cuisses",
                has_current &&
                    current
                        .has_thigh_asymmetry,
                has_current
                    ? current
                        .thigh_asymmetry_percent
                    : 0.0,
                "%"
            );

            body_analytics_value(
                panel_top + 13,
                "Mollets",
                has_current &&
                    current
                        .has_calf_asymmetry,
                has_current
                    ? current
                        .calf_asymmetry_percent
                    : 0.0,
                "%"
            );

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                panel_top + 16,
                6,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 14,
                "Ratios et asymétries sont descriptifs : Trainlog ne les transforme pas en diagnostic."
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (
            key == 'b' ||
            key == 'B' ||
            key == 27
        ) {
            return;
        }

        if (
            key == TRAINLOG_KEY_LEFT ||
            key == TRAINLOG_KEY_RIGHT
        ) {
            page =
                page == 0
                    ? 1
                    : 0;

            continue;
        }

        if (
            key == 'p' ||
            key == 'P'
        ) {
            TrainlogBodyAnalyticsProfile
                edited;

            if (has_profile) {
                edited =
                    profile;
            } else {
                edited.formula =
                    TRAINLOG_BODY_ANALYTICS_FORMULA_MALE;

                edited.height_cm =
                    170.0;
            }

            if (
                body_analytics_profile_prompt(
                    &edited
                )
            ) {
                profile =
                    edited;

                has_profile = true;

                status_line(
                    "✓ Profil d'estimation enregistré localement.",
                    TRAINLOG_COLOR_SUCCESS
                );

                trainlog_terminal_render(tui_terminal);
                (void)trainlog_terminal_get_key(tui_terminal);
            }
        }
    }
}

static void screen_body(
    TrainlogDatabase *database
)
{
    TrainlogBodyObservationRecord
        records[MAX_BODY_OBSERVATIONS];

    size_t selected = 0U;
    int nav_selected = 5;
    int focus = 1;

    for (;;) {
        TrainlogBodyMetricPoint
            weight_points[MAX_BODY_METRIC_POINTS];

        size_t count = 0U;
        size_t weight_count = 0U;
        size_t top = 0U;
        size_t index;

        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int graph_top =
            large_layout ? 11 : 3;

        int graph_bottom =
            large_layout ? 19 : 11;

        int list_top =
            graph_bottom + 1;

        int list_bottom =
            trainlog_terminal_rows(tui_terminal) - 4;

        int first_row =
            list_top + 1;

        int visible_rows =
            list_bottom -
            first_row;

        int key;

        if (visible_rows < 1) {
            return;
        }

        if (trainlog_database_list_body_observations(
                database,
                records,
                MAX_BODY_OBSERVATIONS,
                &count
            ) != TRAINLOG_STATUS_OK) {
            return;
        }

        (void)trainlog_database_list_body_metric_points(
            database,
            TRAINLOG_BODY_METRIC_WEIGHT,
            weight_points,
            MAX_BODY_METRIC_POINTS,
            &weight_count
        );

        if (count > 0U &&
            selected >= count) {
            selected =
                count - 1U;
        }

        if (count > 0U &&
            selected >=
                (size_t)visible_rows) {
            top =
                selected -
                (size_t)visible_rows +
                1U;
        }

        if (large_layout) {
            trainlog_terminal_erase(tui_terminal);
            trainlog_terminal_box(tui_terminal, 0, 0, trainlog_terminal_rows(tui_terminal) - 1, trainlog_terminal_columns(tui_terminal) - 1);

            section_ascii_header(
                ":: C O R P S ::"
            );

            primary_top_navbar(
                5,
                nav_selected,
                focus == 0
            );

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                "Tab zone  ↑↓/PgUp/PgDn relevés  ←→ menu  Entrée détail  e Modifier  a ajouter  v analyse  g vue globale  0/Home accueil  F1-F5 direct"
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Corps",
                "↑↓ choisir  Entrée détail  e Modifier  a ajouter  v analyse  g vue globale  b/Échap retour"
            );
        }

        body_panel(
            graph_top,
            2,
            graph_bottom,
            trainlog_terminal_columns(tui_terminal) - 3,
            "EVOLUTION DU POIDS"
        );

        if (weight_count == 0U) {
            trainlog_terminal_printf(tui_terminal,
                graph_top + 3,
                6,
                "Aucune donnée de poids."
            );
        } else {
            draw_body_metric_graph(
                graph_top + 1,
                graph_bottom -
                    graph_top -
                    2,
                weight_points,
                weight_count,
                "kg"
            );
        }

        if (large_layout) {
            focused_panel(
                list_top,
                2,
                list_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "RELEVES ENREGISTRES",
                focus == 1
            );
        } else {
            body_panel(
                list_top,
                2,
                list_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "RELEVES ENREGISTRES"
            );
        }

        if (count == 0U) {
            trainlog_terminal_printf(tui_terminal,
                first_row + 1,
                6,
                "Aucun relevé. Appuyez sur a pour en ajouter un."
            );
        }

        for (index = 0U;
             index < (size_t)visible_rows &&
             top + index < count;
             ++index) {
            size_t absolute =
                top + index;

            char date[9];
            char summary[128];

            int row =
                first_row +
                (int)index;

            body_short_date(
                records[absolute].observed_at,
                date
            );

            body_summary_text(
                &records[absolute],
                summary,
                sizeof(summary)
            );

            if (focus == 1 &&
                absolute == selected) {
                trainlog_terminal_style_on(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            trainlog_terminal_printf(tui_terminal,
                row,
                5,
                " %-8s  %.*s ",
                date,
                trainlog_terminal_columns(tui_terminal) - 20,
                summary
            );

            if (focus == 1 &&
                absolute == selected) {
                trainlog_terminal_style_off(tui_terminal,
                    TRAINLOG_TEXT_REVERSE |
                    trainlog_theme_style(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        body_draw_scrollbar(
            first_row,
            list_bottom - 1,
            trainlog_terminal_columns(tui_terminal) - 5,
            selected,
            count,
            (size_t)visible_rows
        );

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (large_layout &&
            (key == TRAINLOG_KEY_TAB ||
             key == TRAINLOG_KEY_SHIFT_TAB)) {
            focus =
                focus == 0
                    ? 1
                    : 0;
            continue;
        }

        if (large_layout &&
            primary_top_nav_forward(key)) {
            return;
        }

        if (key == 'b' ||
            key == 'B' ||
            key == 27) {
            return;
        }

        if (large_layout &&
            focus == 0) {
            if (key == TRAINLOG_KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : PRIMARY_NAV_COUNT - 1;
            } else if (key == TRAINLOG_KEY_RIGHT) {
                nav_selected =
                    nav_selected < PRIMARY_NAV_COUNT - 1
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == TRAINLOG_KEY_ENTER
            ) {
                if (primary_top_nav_activate(
                        nav_selected
                    )) {
                    return;
                }
            }

            continue;
        }

        if (key == TRAINLOG_KEY_UP &&
            count > 0U) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : 0U;

            continue;
        }

        if (key == TRAINLOG_KEY_DOWN &&
            count > 0U) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : count - 1U;

            continue;
        }

        if (key == TRAINLOG_KEY_PAGE_UP &&
            count > 0U) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected > jump
                    ? selected - jump
                    : 0U;

            continue;
        }

        if (key == TRAINLOG_KEY_PAGE_DOWN &&
            count > 0U) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected + jump < count
                    ? selected + jump
                    : count - 1U;

            continue;
        }

        if ((key == '\n' ||
             key == TRAINLOG_KEY_ENTER) &&
            count > 0U) {
            screen_body_observation_detail(
                database,
                records[selected].observation_id
            );

            continue;
        }

        if ((key == 'e' ||
             key == 'E') &&
            count > 0U) {
            (void)body_edit_record(
                database,
                records[selected].observation_id
            );

            continue;
        }

        if (key == 'a' ||
            key == 'A') {
            add_body_observation(database);
            selected = 0U;
            continue;
        }

        if (key == 'v' ||
            key == 'V') {
            screen_body_analytics(
                database
            );

            continue;
        }

        if (key == 'g' ||
            key == 'G') {
            draw_shell(
                "TRAINLOG — Corps — Vue globale",
                "Une touche pour revenir"
            );

            draw_global_body_overlay(
                database
            );

            trainlog_terminal_render(tui_terminal);
            (void)trainlog_terminal_get_key(tui_terminal);
        }
    }
}

/* TRAINLOG_SYNC_TUI */
/* TRAINLOG_SHARED_SYNC_ENGINE_V1 */

#define SYNC_HISTORY_CAPACITY 64U
#define SYNC_DETAIL_LINE_CAPACITY 160U
#define SYNC_DETAIL_TEXT_CAPACITY 16384U

static void session_history_datetime(
    const char *timestamp,
    char output[17]
)
{
    if (
        timestamp == NULL ||
        strlen(timestamp) < 16U
    ) {
        (void)snprintf(
            output,
            17U,
            "%s",
            "--/--/---- --:--"
        );

        return;
    }

    (void)snprintf(
        output,
        17U,
        "%c%c/%c%c/%c%c%c%c %c%c:%c%c",
        timestamp[8],
        timestamp[9],
        timestamp[5],
        timestamp[6],
        timestamp[0],
        timestamp[1],
        timestamp[2],
        timestamp[3],
        timestamp[11],
        timestamp[12],
        timestamp[14],
        timestamp[15]
    );
}

static bool sync_history_path(
    char *output,
    size_t output_size
)
{
    const char *data_home =
        getenv(
            "XDG_DATA_HOME"
        );

    const char *home =
        getenv(
            "HOME"
        );

    int written;

    if (
        output == NULL ||
        output_size == 0U
    ) {
        return false;
    }

    if (
        data_home != NULL &&
        data_home[0] != '\0'
    ) {
        written =
            snprintf(
                output,
                output_size,
                "%s/trainlog/sync_history.log",
                data_home
            );
    } else if (
        home != NULL &&
        home[0] != '\0'
    ) {
        written =
            snprintf(
                output,
                output_size,
                "%s/.local/share/trainlog/sync_history.log",
                home
            );
    } else {
        return false;
    }

    return
        written >= 0 &&
        (size_t)written <
            output_size;
}

static bool sync_runs_path(
    const char *sync_id,
    char *output,
    size_t output_size
)
{
    const char *data_home =
        getenv(
            "XDG_DATA_HOME"
        );

    const char *home =
        getenv(
            "HOME"
        );

    int written;

    if (
        sync_id == NULL ||
        sync_id[0] == '\0' ||
        output == NULL ||
        output_size == 0U
    ) {
        return false;
    }

    if (
        data_home != NULL &&
        data_home[0] != '\0'
    ) {
        written =
            snprintf(
                output,
                output_size,
                "%s/trainlog/sync_runs/%s.txt",
                data_home,
                sync_id
            );
    } else if (
        home != NULL &&
        home[0] != '\0'
    ) {
        written =
            snprintf(
                output,
                output_size,
                "%s/.local/share/trainlog/sync_runs/%s.txt",
                home,
                sync_id
            );
    } else {
        return false;
    }

    return
        written >= 0 &&
        (size_t)written <
            output_size;
}

static void sync_history_load(
    TrainlogSyncHistoryEntry *output,
    size_t capacity,
    size_t *output_count
)
{
    char path[
        PATH_MAX + 1U
    ];

    TrainlogSyncHistoryEntry
        ring[SYNC_HISTORY_CAPACITY];

    size_t count = 0U;
    size_t next = 0U;
    size_t copied;
    FILE *file;
    char line[768];

    if (
        output_count == NULL ||
        (
            capacity > 0U &&
            output == NULL
        )
    ) {
        return;
    }

    *output_count = 0U;

    if (
        !sync_history_path(
            path,
            sizeof(path)
        )
    ) {
        return;
    }

    file =
        fopen(
            path,
            "rb"
        );

    if (file == NULL) {
        return;
    }

    (void)memset(
        ring,
        0,
        sizeof(ring)
    );

    while (
        fgets(
            line,
            sizeof(line),
            file
        ) != NULL
    ) {
        TrainlogSyncHistoryEntry *entry;

        if (
            strchr(line, '\n') == NULL &&
            !feof(file)
        ) {
            int discarded;

            do {
                discarded = fgetc(file);
            } while (discarded != '\n' && discarded != EOF);

            continue;
        }

        entry =
            &ring[next];

        if (
            !trainlog_sync_history_parse_line(
                line,
                entry
            )
        ) {
            continue;
        }

        next =
            (
                next + 1U
            ) %
            SYNC_HISTORY_CAPACITY;

        if (
            count <
            SYNC_HISTORY_CAPACITY
        ) {
            ++count;
        }
    }

    (void)fclose(
        file
    );

    copied =
        count < capacity
            ? count
            : capacity;

    for (
        size_t index = 0U;
        index < copied;
        ++index
    ) {
        size_t source =
            (
                next +
                SYNC_HISTORY_CAPACITY -
                1U -
                index
            ) %
            SYNC_HISTORY_CAPACITY;

        output[index] =
            ring[source];
    }

    *output_count =
        copied;
}

static void screen_sync_run_detail(
    const char *sync_id
)
{
    char path[
        PATH_MAX + 1U
    ];

    char text[
        SYNC_DETAIL_TEXT_CAPACITY
    ];

    char *lines[
        SYNC_DETAIL_LINE_CAPACITY
    ];

    size_t line_count = 0U;
    size_t offset = 0U;
    FILE *file;
    size_t used;

    if (
        !sync_runs_path(
            sync_id,
            path,
            sizeof(path)
        )
    ) {
        return;
    }

    file =
        fopen(
            path,
            "rb"
        );

    if (file == NULL) {
        return;
    }

    used =
        fread(
            text,
            1U,
            sizeof(text) - 1U,
            file
        );

    text[used] = '\0';

    (void)fclose(
        file
    );

    if (used > 0U) {
        char *cursor = text;

        lines[line_count] =
            cursor;

        ++line_count;

        while (
            *cursor != '\0' &&
            line_count <
                SYNC_DETAIL_LINE_CAPACITY
        ) {
            if (*cursor == '\n') {
                *cursor = '\0';

                if (
                    cursor[1] != '\0'
                ) {
                    lines[line_count] =
                        cursor + 1;

                    ++line_count;
                }
            }

            ++cursor;
        }
    }

    for (;;) {
        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int panel_top =
            large_layout
                ? 8
                : 3;

        int panel_bottom =
            trainlog_terminal_rows(tui_terminal) - 4;

        int first_row =
            panel_top + 2;

        int visible =
            panel_bottom -
            first_row;

        int key;
        size_t index;

        if (visible < 1) {
            return;
        }

        trainlog_terminal_erase(tui_terminal);
        trainlog_terminal_box(tui_terminal, 0, 0,
                              trainlog_terminal_rows(tui_terminal) - 1,
                              trainlog_terminal_columns(tui_terminal) - 1);

        if (large_layout) {
            section_ascii_header(
                ":: S Y N C   S H O W ::"
            );

            focused_panel(
                panel_top,
                2,
                panel_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "DETAIL SYNCHRONISATION",
                true
            );
        } else {
            draw_shell(
                "TRAINLOG — Sync show",
                "↑↓ défiler  PgUp/PgDn page  b/Échap retour"
            );
        }

        for (
            index = 0U;
            index < (size_t)visible &&
            offset + index <
                line_count;
            ++index
        ) {
            trainlog_terminal_printf(tui_terminal,
                first_row +
                    (int)index,
                large_layout
                    ? 5
                    : 4,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) -
                    (
                        large_layout
                            ? 10
                            : 8
                    ),
                lines[
                    offset +
                    index
                ]
            );
        }

        trainlog_terminal_style_on(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_printf(tui_terminal,
            trainlog_terminal_rows(tui_terminal) - 2,
            2,
            "%.*s",
            trainlog_terminal_columns(tui_terminal) - 4,
            "↑↓ défiler  PgUp/PgDn page  b/Échap retour"
        );

        trainlog_terminal_style_off(tui_terminal,
            trainlog_theme_style(
                TRAINLOG_COLOR_MUTED
            )
        );

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        if (
            key == 'b' ||
            key == 'B' ||
            key == 27 ||
            key == '\n' ||
            key == TRAINLOG_KEY_ENTER
        ) {
            return;
        }

        if (
            key == TRAINLOG_KEY_UP &&
            offset > 0U
        ) {
            --offset;
        } else if (
            key == TRAINLOG_KEY_DOWN &&
            offset +
                (size_t)visible <
                line_count
        ) {
            ++offset;
        } else if (
            key == TRAINLOG_KEY_PAGE_UP
        ) {
            size_t jump =
                (size_t)visible;

            offset =
                offset > jump
                    ? offset - jump
                    : 0U;
        } else if (
            key == TRAINLOG_KEY_PAGE_DOWN
        ) {
            size_t jump =
                (size_t)visible;

            if (
                offset + jump <
                line_count
            ) {
                offset += jump;
            }

            if (
                line_count >
                    (size_t)visible &&
                offset +
                    (size_t)visible >
                    line_count
            ) {
                offset =
                    line_count -
                    (size_t)visible;
            }
        }
    }
}

static double sync_bytes_to_gib(
    uint64_t bytes
)
{
    return
        (double)bytes /
        (
            1024.0 *
            1024.0 *
            1024.0
        );
}

static TrainlogStatus sync_screen_run(
    TrainlogSyncDirection direction,
    TrainlogSyncReport *report,
    void *context
)
{
    (void)context;

    return trainlog_sync_run(
        TRAINLOG_SYNC_TRIGGER_TUI,
        false,
        direction,
        report
    );
}

static void screen_sync(
    TrainlogDatabase *database
)
{
    size_t selected = 0U;
    int nav_selected = 6;
    int focus = 1;
    TrainlogSyncScreenState action_state;

    TrainlogSyncDeviceInfo device;
    TrainlogStatus probe_status =
        TRAINLOG_STATUS_NOT_FOUND;

    bool refresh_device = true;

    (void)database;

    (void)memset(
        &device,
        0,
        sizeof(device)
    );

    trainlog_sync_screen_state_init(
        &action_state
    );

    for (;;) {
        TrainlogSyncHistoryEntry
            history[SYNC_HISTORY_CAPACITY];

        size_t history_count = 0U;

        bool large_layout =
            trainlog_terminal_columns(tui_terminal) >= 100 &&
            trainlog_terminal_rows(tui_terminal) >= 30;

        int key;

        if (refresh_device) {
            probe_status =
                trainlog_sync_probe(
                    &device
                );

            refresh_device = false;
        }

        sync_history_load(
            history,
            SYNC_HISTORY_CAPACITY,
            &history_count
        );

        if (
            history_count > 0U &&
            selected >= history_count
        ) {
            selected =
                history_count - 1U;
        }

        trainlog_terminal_erase(tui_terminal);
        trainlog_terminal_box(tui_terminal, 0, 0,
                              trainlog_terminal_rows(tui_terminal) - 1,
                              trainlog_terminal_columns(tui_terminal) - 1);

        if (large_layout) {
            size_t index;
            size_t top = 0U;

            int history_top = 18;

            int history_bottom =
                trainlog_terminal_rows(tui_terminal) - 4;

            int visible_rows =
                history_bottom -
                history_top -
                2;

            section_ascii_header(
                ":: S Y N C ::"
            );

            primary_top_navbar(
                6,
                nav_selected,
                focus == 0
            );

            focused_panel(
                11,
                2,
                16,
                trainlog_terminal_columns(tui_terminal) - 3,
                "APPAREIL CONNECTE",
                false
            );

            trainlog_terminal_printf(tui_terminal, 15, 5,
                "Actions : a Android→PC | p PC→Android | b PC↔Android");

            if (
                probe_status ==
                    TRAINLOG_STATUS_OK &&
                device.connected &&
                device.storage_ready
            ) {
                trainlog_terminal_printf(tui_terminal,
                    12,
                    5,
                    "✓ MTP direct connecté"
                );

                trainlog_terminal_printf(tui_terminal,
                    13,
                    5,
                    "%s %s",
                    device.device.vendor,
                    device.device.model
                );

                trainlog_terminal_printf(tui_terminal,
                    14,
                    5,
                    "Stockage interne : %.2f GiB libres / %.2f GiB",
                    sync_bytes_to_gib(
                        device.storage
                            .free_space_bytes
                    ),
                    sync_bytes_to_gib(
                        device.storage
                            .max_capacity_bytes
                    )
                );
            } else if (
                probe_status ==
                TRAINLOG_STATUS_CONFLICT
            ) {
                trainlog_terminal_printf(tui_terminal,
                    13,
                    5,
                    "Service de synchronisation occupé."
                );
            } else {
                trainlog_terminal_printf(tui_terminal,
                    13,
                    5,
                    "Aucun appareil MTP Trainlog détecté."
                );
            }

            focused_panel(
                history_top,
                2,
                history_bottom,
                trainlog_terminal_columns(tui_terminal) - 3,
                "HISTORIQUE DES SYNCHRONISATIONS",
                focus == 1
            );

            if (
                history_count == 0U
            ) {
                trainlog_terminal_printf(tui_terminal,
                    history_top + 2,
                    5,
                    "Aucune synchronisation enregistrée."
                );
            } else {
                if (visible_rows < 1) {
                    visible_rows = 1;
                }

                if (
                    selected >=
                    (size_t)visible_rows
                ) {
                    top =
                        selected -
                        (size_t)visible_rows +
                        1U;
                }

                for (
                    index = 0U;
                    index <
                        (size_t)visible_rows &&
                    top + index <
                        history_count;
                    ++index
                ) {
                    size_t absolute =
                        top + index;

                    int row =
                        history_top +
                        2 +
                        (int)index;

                    if (
                        focus == 1 &&
                        absolute == selected
                    ) {
                        trainlog_terminal_style_on(tui_terminal,
                            TRAINLOG_TEXT_REVERSE |
                            trainlog_theme_style(
                                TRAINLOG_COLOR_ACCENT
                            )
                        );
                    }

                    trainlog_terminal_printf(tui_terminal,
                        row,
                        5,
                        " %-16s  %c  %-17.17s  %-*.*s ",
                        history[absolute]
                            .timestamp,
                        history[absolute]
                            .success
                            ? '+'
                            : '!',
                        trainlog_sync_history_direction_label(
                            &history[absolute]
                        ),
                        trainlog_terminal_columns(tui_terminal) - 47,
                        trainlog_terminal_columns(tui_terminal) - 47,
                        history[absolute]
                            .summary
                    );

                    if (
                        focus == 1 &&
                        absolute == selected
                    ) {
                        trainlog_terminal_style_off(tui_terminal,
                            TRAINLOG_TEXT_REVERSE |
                            trainlog_theme_style(
                                TRAINLOG_COLOR_ACCENT
                            )
                        );
                    }
                }
            }

            trainlog_terminal_style_on(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );

            trainlog_terminal_printf(tui_terminal,
                trainlog_terminal_rows(tui_terminal) - 2,
                2,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 4,
                trainlog_sync_screen_footer(
                    trainlog_terminal_columns(tui_terminal)
                )
            );

            trainlog_terminal_style_off(tui_terminal,
                trainlog_theme_style(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Sync",
                trainlog_sync_screen_footer(
                    trainlog_terminal_columns(tui_terminal)
                )
            );

            if (
                probe_status ==
                    TRAINLOG_STATUS_OK &&
                device.connected
            ) {
                trainlog_terminal_printf(tui_terminal,
                    4,
                    4,
                    "✓ %s %s",
                    device.device.vendor,
                    device.device.model
                );
            } else {
                trainlog_terminal_printf(tui_terminal,
                    4,
                    4,
                    "Aucun appareil MTP."
                );
            }

            trainlog_terminal_printf(tui_terminal, 5, 4,
                "a Android→PC | p PC→Android | b PC↔Android");

            if (
                history_count == 0U
            ) {
                trainlog_terminal_printf(tui_terminal,
                    7,
                    4,
                    "Aucune synchronisation."
                );
            } else {
                size_t index;

                size_t limit =
                    history_count < 8U
                        ? history_count
                        : 8U;

                for (
                    index = 0U;
                    index < limit;
                    ++index
                ) {
                    if (
                        index == selected
                    ) {
                        trainlog_terminal_style_on(tui_terminal,
                            TRAINLOG_TEXT_REVERSE |
                            trainlog_theme_style(
                                TRAINLOG_COLOR_ACCENT
                            )
                        );
                    }

                    trainlog_terminal_printf(tui_terminal,
                        7 + (int)index,
                        4,
                        "%-16s %c %-17.17s %.*s",
                        history[index]
                            .timestamp,
                        history[index]
                            .success
                            ? '+'
                            : '!',
                        trainlog_sync_history_direction_label(
                            &history[index]
                        ),
                        trainlog_terminal_columns(tui_terminal) - 43,
                        history[index]
                            .summary
                    );

                    if (
                        index == selected
                    ) {
                        trainlog_terminal_style_off(tui_terminal,
                            TRAINLOG_TEXT_REVERSE |
                            trainlog_theme_style(
                                TRAINLOG_COLOR_ACCENT
                            )
                        );
                    }
                }
            }
        }

        if (action_state.confirming) {
            int middle =
                trainlog_terminal_rows(tui_terminal) / 2;

            focused_panel(
                middle - 2,
                4,
                middle + 2,
                trainlog_terminal_columns(tui_terminal) - 5,
                "CONFIRMATION",
                true
            );
            trainlog_terminal_printf(tui_terminal,
                middle,
                7,
                "%.*s",
                trainlog_terminal_columns(tui_terminal) - 14,
                trainlog_sync_screen_confirmation(
                    action_state.direction
                )
            );
            trainlog_terminal_printf(tui_terminal,
                middle + 1,
                7,
                "Entrée confirmer · Échap annuler"
            );
        }

        trainlog_terminal_render(tui_terminal);
        key = trainlog_terminal_get_key(tui_terminal);

        {
            bool was_confirming =
                action_state.confirming;

            TrainlogSyncScreenAction action =
                trainlog_sync_screen_dispatch(
                    &action_state,
                    key
                );

            if (action.effect == TRAINLOG_SYNC_SCREEN_EXIT) {
                return;
            }

            if (action.effect == TRAINLOG_SYNC_SCREEN_REFRESH) {
                refresh_device = true;
                continue;
            }

            if (
                action.effect == TRAINLOG_SYNC_SCREEN_CONFIRM ||
                action.effect == TRAINLOG_SYNC_SCREEN_CANCEL
            ) {
                continue;
            }

            if (action.effect == TRAINLOG_SYNC_SCREEN_RUN) {
                TrainlogSyncReport report;
                TrainlogStatus status;

                (void)memset(
                    &report,
                    0,
                    sizeof(report)
                );

                status_line(
                    action.direction == TRAINLOG_SYNC_ANDROID_TO_PC
                        ? "Étape Android→PC : import en cours..."
                        : action.direction == TRAINLOG_SYNC_PC_TO_ANDROID
                            ? "Étape PC→Android : publication en cours..."
                            : "Étape PC↔Android : import puis publication...",
                    TRAINLOG_COLOR_WARNING
                );

                trainlog_terminal_render(tui_terminal);

                status =
                    trainlog_sync_screen_execute(
                        &action,
                        sync_screen_run,
                        NULL,
                        &report
                    );

                if (
                    status ==
                        TRAINLOG_STATUS_OK &&
                    report.success
                ) {
                    status_line(
                        report.summary,
                        TRAINLOG_COLOR_SUCCESS
                    );
                } else {
                    char error_summary[
                        TRAINLOG_SYNC_ERROR_MAX + 32U
                    ];

                    (void)snprintf(
                        error_summary,
                        sizeof(error_summary),
                        "%s — %s",
                        trainlog_sync_screen_direction_label(
                            action.direction
                        ),
                        report.error[0] != '\0'
                            ? report.error
                            : "Synchronisation échouée."
                    );

                    status_line(
                        error_summary,
                        TRAINLOG_COLOR_ERROR
                    );
                }

                trainlog_terminal_render(tui_terminal);
                (void)trainlog_terminal_get_key(tui_terminal);

                refresh_device = true;
                continue;
            }

            /* While confirmation is visible, unrelated keys (including the
             * retired s shortcut) cannot leak into history/navigation. */
            if (was_confirming) {
                continue;
            }
        }

        if (
            large_layout &&
            (
                key == TRAINLOG_KEY_TAB ||
                key == TRAINLOG_KEY_SHIFT_TAB
            )
        ) {
            focus =
                focus == 0
                    ? 1
                    : 0;

            continue;
        }

        if (
            large_layout &&
            focus == 0
        ) {
            if (
                key == TRAINLOG_KEY_LEFT
            ) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : PRIMARY_NAV_COUNT - 1;
            } else if (
                key == TRAINLOG_KEY_RIGHT
            ) {
                nav_selected =
                    nav_selected < PRIMARY_NAV_COUNT - 1
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == TRAINLOG_KEY_ENTER
            ) {
                if (nav_selected == 6) {
                    focus = 1;
                } else if (
                    primary_top_nav_activate(
                        nav_selected
                    )
                ) {
                    return;
                }
            }

            continue;
        }

        if (
            history_count > 0U &&
            (
                key == '\n' ||
                key == TRAINLOG_KEY_ENTER
            )
        ) {
            if (
                history[selected]
                    .sync_id[0] != '\0'
            ) {
                screen_sync_run_detail(
                    history[selected]
                        .sync_id
                );
            }

            continue;
        }

        if (
            key == TRAINLOG_KEY_UP &&
            history_count > 0U
        ) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : history_count - 1U;
        } else if (
            key == TRAINLOG_KEY_DOWN &&
            history_count > 0U
        ) {
            selected =
                selected + 1U <
                    history_count
                    ? selected + 1U
                    : 0U;
        } else if (
            key == '0' ||
            key == TRAINLOG_KEY_HOME
        ) {
            return;
        } else if (
            key == '1' ||
            key == TRAINLOG_KEY_F1 ||
            key == '2' ||
            key == TRAINLOG_KEY_F2 ||
            key == '3' ||
            key == TRAINLOG_KEY_F3 ||
            key == '4' ||
            key == TRAINLOG_KEY_F4 ||
            key == '5' ||
            key == TRAINLOG_KEY_F5 ||
            key == '6'
        ) {
            if (
                primary_top_nav_forward(
                    key
                )
            ) {
                return;
            }
        }
    }
}

int trainlog_tui_run(TrainlogDatabase *database)
{
    if (database == NULL) {
        return 1;
    }

    (void)setlocale(LC_ALL, "");

    tui_terminal = trainlog_terminal_create();
    if (tui_terminal == NULL) {
        return 1;
    }

    trainlog_terminal_cursor_visible(tui_terminal, false);

    for (;;) {
        DashboardAction action;

        if (trainlog_terminal_rows(tui_terminal) < 20 || trainlog_terminal_columns(tui_terminal) < 72) {
            int key;

            trainlog_terminal_erase(tui_terminal);
            trainlog_terminal_printf(tui_terminal,
                1,
                2,
                "Terminal trop petit — minimum 72x20."
            );
            trainlog_terminal_printf(tui_terminal, 3, 2, "q pour quitter");
            trainlog_terminal_render(tui_terminal);

            key = trainlog_terminal_get_key(tui_terminal);
            if (key == 'q' || key == 'Q') {
                break;
            }
            continue;
        }

        action = screen_dashboard(database);

        switch (action) {
        case DASHBOARD_NEW_SESSION:
            screen_new_session(database);
            break;
        case DASHBOARD_HISTORY:
            screen_history(database);
            break;
        case DASHBOARD_EXERCISES:
            screen_exercises(database);
            break;
        case DASHBOARD_EQUIPMENT:
            screen_equipment(database);
            break;
        case DASHBOARD_BODY:
            screen_body(database);
            break;
        case DASHBOARD_SYNC:
            screen_sync(database);
            break;
        case DASHBOARD_QUIT:
            trainlog_terminal_destroy(tui_terminal);
            tui_terminal = NULL;
            return 0;
        default:
            break;
        }
    }

    trainlog_terminal_destroy(tui_terminal);
    tui_terminal = NULL;
    return 0;
}
