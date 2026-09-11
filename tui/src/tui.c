/**
 * @file tui.c
 * @brief Trainlog Notcurses terminal interface.
 */

#include "trainlog/tui.h"

#include <ctype.h>
#include <fcntl.h>
#include <locale.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <wchar.h>
#include <unistd.h>

#include <utf8proc.h>
#include <uuid/uuid.h>

#include "trainlog/terminal.h"
#include "trainlog/app_shell.h"

#include "trainlog/bodyviz.h"
#include "trainlog/body_analytics.h"
#include "trainlog/body_zone_catalog.h"
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
#include "trainlog/session_generation.h"
#include "trainlog/session_generation_policy_internal.h"
#include "trainlog/theme.h"
#include "trainlog/timeutil.h"
#include "trainlog/training_knowledge.h"
#include "trainlog/usb.h"

#define MAX_EXERCISES 128U
#define MAX_SESSION_EXERCISES 32U
#define MAX_SETS_PER_EXERCISE 64U
#define MAX_SESSIONS 128U
#define MAX_WEIGHT_POINTS 256U
#define MAX_BODY_ZONES 16U
#define MAX_BODY_OBSERVATIONS 512U

#define MAX_BODY_METRIC_POINTS 256U
#define EXERCISE_GRAPH_POINTS 12U

/* TRAINLOG_TUI_V02_POLISH */
/* TRAINLOG_TUI_PROFILED_EXERCISE_CREATION */
/* TRAINLOG_VARIABLE_SET_REPS_V1 */
/* TRAINLOG_SYNC_RESPONSIVE_CACHE */
/* TRAINLOG_SYNC_LARGE_LAYOUT_S_FIX */
/* TRAINLOG_SYNC_HISTORY_BIDIRECTIONAL_V1 */
/* TRAINLOG_SYNC_PC_TO_ANDROID_DIAGNOSTICS */
/* TRAINLOG_SYNC_FULL_MTP_SILENCE */

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
    TrainlogResolvedEquipment *output, size_t capacity, bool *truncated,
    bool *failed)
{
    TrainlogCustomEquipment customs[64];
    const TrainlogEquipment **supplied_matches = NULL;
    size_t supplied_capacity = trainlog_equipment_catalog_count();
    size_t supplied_count;
    size_t custom_offset = 0U;
    bool custom_more = true;
    size_t count = 0U;
    size_t index;
    if (truncated != NULL) *truncated = false;
    if (failed != NULL) *failed = false;
    if (supplied_capacity > 0U && query != NULL && query[0] != '\0') {
        supplied_matches = calloc(supplied_capacity, sizeof(*supplied_matches));
        if (supplied_matches == NULL) {
            if (failed != NULL) *failed = true;
            return 0U;
        }
    }
    supplied_count = query != NULL && query[0] != '\0'
        ? trainlog_equipment_catalog_search(query, supplied_matches,
            supplied_capacity) : supplied_capacity;
    for (index = 0U; index < supplied_count; ++index) {
        const TrainlogEquipment *item = query != NULL && query[0] != '\0'
            ? supplied_matches[index] : trainlog_equipment_catalog_at(index);
        if (item == NULL) continue;
        if (count >= capacity) {
            if (truncated != NULL) *truncated = true;
            continue;
        }
        if (trainlog_database_resolve_equipment(database, item->equipment_id,
                &output[count]) == TRAINLOG_STATUS_OK) ++count;
    }
    free(supplied_matches);
    while (custom_more) {
        size_t custom_count = 0U;
        if (trainlog_database_list_custom_equipment_page(database,
                custom_offset, customs, 64U, &custom_count,
                &custom_more) != TRAINLOG_STATUS_OK) {
            if (failed != NULL) *failed = true;
            return count;
        }
        for (index = 0U; index < custom_count; ++index) {
            if (!equipment_text_matches(customs[index].display_name, query) &&
                !equipment_text_matches(customs[index].label_name, query) &&
                !equipment_text_matches(customs[index].equipment_type, query))
                continue;
            if (count >= capacity) {
                if (truncated != NULL) *truncated = true;
                return count;
            }
            if (trainlog_database_resolve_equipment(database,
                    customs[index].equipment_id,
                    &output[count]) == TRAINLOG_STATUS_OK) ++count;
        }
        if (custom_count > SIZE_MAX - custom_offset ||
            (custom_more && custom_count == 0U)) {
            if (failed != NULL) *failed = true;
            return count;
        }
        custom_offset += custom_count;
    }
    return count;
}

static const char *equipment_origin_label(TrainlogEquipmentOrigin origin)
{
    if (origin == TRAINLOG_EQUIPMENT_SUPPLIED) return "fourni";
    if (origin == TRAINLOG_EQUIPMENT_CUSTOM) return "personnalisé";
    return "inconnu";
}

static void generate_custom_equipment_uuid(
    char output[TRAINLOG_UUID_TEXT_LENGTH + 1U])
{
    uuid_t value;
    uuid_generate_random(value);
    uuid_unparse_lower(value, output);
}

static const char *session_type_label(TrainlogSessionType type)
{
    return type == TRAINLOG_SESSION_MAX_TEST ? "Test de max" : "Entraînement";
}

static const char *session_type_history_label(TrainlogSessionType type)
{
    return type == TRAINLOG_SESSION_MAX_TEST ? "[MAX]" : "[ENTRAINEMENT]";
}

static bool parse_double_positive(const char *text, double *output)
{
    char *end = NULL;
    double value;

    if (text == NULL || output == NULL || text[0] == '\0') {
        return false;
    }

    value = strtod(text, &end);
    if (end == text || *end != '\0' || !isfinite(value) || value <= 0.0) {
        return false;
    }

    *output = value;
    return true;
}

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
)
{
    size_t length;
    if (output == NULL || output_size == 0U) return;
    (void)snprintf(output, output_size, "%.2f", value);
    length = strlen(output);
    while (length > 0U && output[length - 1U] == '0')
        output[--length] = '\0';
    if (length > 0U && output[length - 1U] == '.')
        output[--length] = '\0';
}

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

/* TRAINLOG_EXERCISE_FRAMES */

static bool secondary_zone_contains(
    char secondary[][TRAINLOG_ZONE_ID_MAX + 1U],
    size_t count,
    const char *zone_id,
    size_t *output_index
)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (strcmp(secondary[index], zone_id) == 0) {
            if (output_index != NULL) *output_index = index;
            return true;
        }
    }
    return false;
}

/* WHY: terminal users select translated catalogue rows; accepting raw IDs
 * would leak wire identity into product behavior and permit unknown values. */
static bool load_exercise_body_zones(
    TrainlogDatabase *database,
    const char *exercise_id,
    char primary[TRAINLOG_ZONE_ID_MAX + 1U],
    char secondary[][TRAINLOG_ZONE_ID_MAX + 1U],
    size_t *secondary_count
)
{
    TrainlogExerciseBodyZone relations[MAX_BODY_ZONES];
    size_t count = 0U;
    size_t index;
    primary[0] = '\0';
    *secondary_count = 0U;
    if (trainlog_database_list_exercise_body_zones(database, exercise_id,
            relations, MAX_BODY_ZONES, &count) != TRAINLOG_STATUS_OK) return false;
    for (index = 0U; index < count; ++index) {
        if (relations[index].role == TRAINLOG_BODY_ZONE_PRIMARY) {
            (void)snprintf(primary, TRAINLOG_ZONE_ID_MAX + 1U, "%s",
                relations[index].zone_id);
        } else if (*secondary_count < MAX_BODY_ZONES) {
            (void)snprintf(secondary[*secondary_count], TRAINLOG_ZONE_ID_MAX + 1U,
                "%s", relations[index].zone_id);
            ++*secondary_count;
        }
    }
    return true;
}

/* CONTRACT: knowledge text is read-only catalogue data.  The screen stores
 * display lines locally so navigation never changes the scientific record. */
#define KNOWLEDGE_LINES_MAX 128U
#define KNOWLEDGE_LINE_MAX 256U

static void knowledge_add_wrapped(char lines[][KNOWLEDGE_LINE_MAX], size_t *count,
                                  const char *text, int width)
{
    const char *at = text == NULL ? "" : text;
    size_t used = 0U;
    int cells = 0;

    /* WHY: terminal columns count display cells, while French labels are UTF-8;
     * utf8proc prevents a wrap from splitting an accented character or from
     * writing past the right border for a wide code point. */
    while (*at != '\0' && *count < KNOWLEDGE_LINES_MAX) {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t bytes = utf8proc_iterate((const utf8proc_uint8_t *)at,
            -1, &codepoint);
        int codepoint_cells;
        if (bytes <= 0) { codepoint = (unsigned char)*at; bytes = 1; }
        codepoint_cells = utf8proc_charwidth(codepoint);
        if (codepoint_cells < 0) codepoint_cells = 1;
        if (cells > 0 && cells + codepoint_cells > width) {
            lines[*count][used] = '\0';
            ++*count;
            used = 0U;
            cells = 0;
            continue;
        }
        if (used + (size_t)bytes >= KNOWLEDGE_LINE_MAX - 1U) break;
        (void)memcpy(lines[*count] + used, at, (size_t)bytes);
        used += (size_t)bytes;
        cells += codepoint_cells;
        at += bytes;
    }
    /* INVARIANT: every entry is NUL-terminated before rendering, and the fixed
     * line capacity bounds catalogue display independently of terminal size. */
    if (*count < KNOWLEDGE_LINES_MAX) {
        lines[*count][used] = '\0';
        ++*count;
    }
}

static void knowledge_add_ids(char lines[][KNOWLEDGE_LINE_MAX], size_t *count,
                              const char *heading, const char *ids, bool muscles,
                              int width)
{
    const char *at = ids;
    knowledge_add_wrapped(lines, count, heading, width);
    while (at != NULL && *at != '\0' && *count < KNOWLEDGE_LINES_MAX) {
        const char *end = strchr(at, '\n');
        size_t length = end == NULL ? strlen(at) : (size_t)(end - at);
        char id[96];
        const char *label = NULL;
        if (length >= sizeof(id)) break;
        (void)memcpy(id, at, length);
        id[length] = '\0';
        if (muscles) {
            const TrainlogKnowledgeMuscle *item = trainlog_knowledge_muscle_lookup(id);
            if (item != NULL) label = item->display_name_fr;
        } else {
            const TrainlogKnowledgeMovementPattern *item =
                trainlog_knowledge_movement_pattern_lookup(id);
            if (item != NULL) label = item->display_name_fr;
        }
        knowledge_add_wrapped(lines, count, label == NULL ? id : label, width);
        if (end == NULL) break;
        at = end + 1;
    }
}

static void knowledge_add_plain_ids(char lines[][KNOWLEDGE_LINE_MAX], size_t *count,
                                    const char *heading, const char *ids, int width)
{
    const char *at = ids;
    knowledge_add_wrapped(lines, count, heading, width);
    while (at != NULL && *at != '\0' && *count < KNOWLEDGE_LINES_MAX) {
        const char *end = strchr(at, '\n');
        size_t length = end == NULL ? strlen(at) : (size_t)(end - at);
        char id[96];
        if (length >= sizeof(id)) break;
        (void)memcpy(id, at, length);
        id[length] = '\0';
        knowledge_add_wrapped(lines, count, id, width);
        if (end == NULL) break;
        at = end + 1;
    }
}

static void knowledge_add_zones(char lines[][KNOWLEDGE_LINE_MAX], size_t *count,
                                const TrainlogKnowledgeInterpretation *value, int width)
{
    const TrainlogBodyZone *zone;
    knowledge_add_wrapped(lines, count, "Zones scientifiques :", width);
    zone = trainlog_body_zone_catalog_lookup(value->primary_zone_id);
    knowledge_add_wrapped(lines, count,
        zone == NULL ? value->primary_zone_id : zone->display_name, width);
    knowledge_add_plain_ids(lines, count, "Zones secondaires :",
        value->secondary_zone_ids, width);
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

typedef struct TrainlogSessionDraftExercise {
    TrainlogSessionExerciseInput input;
    TrainlogSetInput sets[MAX_SETS_PER_EXERCISE];
    char name[TRAINLOG_NAME_MAX + 1U];
    TrainlogTrackingMode tracking_mode;
    char notes[TRAINLOG_NOTE_MAX + 1U];
    /* Transient provenance only; the persisted plan stores the resulting kg. */
    bool target_from_percent_max;
} TrainlogSessionDraftExercise;

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

static void draft_format_set_metric(
    const TrainlogSessionDraftExercise *draft,
    const TrainlogSetInput *set,
    char *output,
    size_t output_size
)
{
    if (draft->tracking_mode == TRAINLOG_TRACKING_DURATION) {
        if (trainlog_duration_format(
                set->duration_seconds,
                output,
                output_size
            ) != TRAINLOG_STATUS_OK) {
            (void)snprintf(output, output_size, "%d s",
                set->duration_seconds);
        }
    } else {
        (void)snprintf(output, output_size, "%d", set->reps);
    }
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
    size_t index;
    size_t used = 0U;

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
    } else if (draft->input.recording_mode ==
        TRAINLOG_RECORDING_CONTINUOUS) {
        char duration[64];
        (void)trainlog_duration_format(
            draft->input.continuous_duration_seconds,
            duration,
            sizeof(duration)
        );
        (void)snprintf(output, output_size, "Continu · %s", duration);
    } else if (draft->input.set_count == 0U &&
        draft->input.target_sets > 0 && draft->input.target_reps > 0) {
        if (draft->input.target_has_weight) {
            (void)snprintf(output, output_size,
                "Plan %d×%d · %.2f kg · repos %d s · 0 réalisée",
                draft->input.target_sets, draft->input.target_reps,
                draft->input.target_weight_kg, draft->input.rest_seconds);
        } else {
            (void)snprintf(output, output_size,
                "Plan %d×%d · charge absente · repos %d s · 0 réalisée",
                draft->input.target_sets, draft->input.target_reps,
                draft->input.rest_seconds);
        }
    } else {
        int written = snprintf(output, output_size, "%zu séries · ",
            draft->input.set_count);
        if (written < 0) {
            output[0] = '\0';
            return;
        }
        used = (size_t)written < output_size
            ? (size_t)written : output_size - 1U;

        /* CONTRACT: the in-progress summary reflects each actual row. It
         * must not collapse heterogeneous loads back to the target weight. */
        for (index = 0U; index < draft->input.set_count && used + 1U < output_size;
             ++index) {
            char metric[64];
            char fragment[96];
            draft_format_set_metric(draft, &draft->sets[index], metric,
                sizeof(metric));
            if (draft->input.load_mode == TRAINLOG_LOAD_NONE) {
                written = snprintf(fragment, sizeof(fragment), "%s%s",
                    index > 0U ? " / " : "", metric);
            } else {
                written = snprintf(fragment, sizeof(fragment), "%s%s×%s%s",
                    index > 0U ? " / " : "", metric,
                    draft->sets[index].has_weight ? "" : "—",
                    draft->sets[index].has_weight ? "kg" : "");
                if (draft->sets[index].has_weight) {
                    written = snprintf(fragment, sizeof(fragment),
                        "%s%s×%.2fkg%s",
                        index > 0U ? " / " : "", metric,
                        draft->sets[index].weight_kg,
                        draft->input.load_mode == TRAINLOG_LOAD_ASSISTANCE
                            ? " aide" : "");
                }
            }
            if (written < 0) {
                break;
            }
            (void)snprintf(output + used, output_size - used, "%s", fragment);
            used = strlen(output);
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

typedef struct TrainlogGeneratorPreviewItem {
    char exercise_name[TRAINLOG_NAME_MAX + 1U];
    char equipment_name[TRAINLOG_NAME_MAX + 1U];
    char zone_name[TRAINLOG_NAME_MAX + 1U];
    char movement_name[TRAINLOG_NAME_MAX + 1U];
} TrainlogGeneratorPreviewItem;

static const char *generator_goal_label(const char *goal_id)
{
    if (strcmp(goal_id, "general") == 0) return "Général";
    if (strcmp(goal_id, "strength") == 0) return "Force";
    if (strcmp(goal_id, "hypertrophy") == 0) return "Hypertrophie";
    if (strcmp(goal_id, "endurance") == 0) return "Endurance";
    return goal_id;
}

static bool generator_prepare_preview(TrainlogDatabase *database,
    const TrainlogGeneratedSession *session, TrainlogGeneratorPreviewItem *items)
{
    size_t index;
    for (index = 0U; index < session->exercise_count; ++index) {
        TrainlogExercise exercise;
        TrainlogResolvedEquipment equipment;
        const TrainlogBodyZone *zone;
        const TrainlogKnowledgeMovementPattern *pattern = NULL;
        if (trainlog_database_get_exercise_profile(database,
                session->exercises[index].exercise_id, &exercise) != TRAINLOG_STATUS_OK ||
            trainlog_database_resolve_equipment(database,
                session->exercises[index].equipment_id, &equipment) != TRAINLOG_STATUS_OK) return false;
        zone = trainlog_body_zone_catalog_lookup(session->exercises[index].primary_zone_id);
        if (session->exercises[index].pattern_count > 0U)
            pattern = trainlog_knowledge_movement_pattern_lookup(session->exercises[index].pattern_ids[0]);
        (void)snprintf(items[index].exercise_name, sizeof(items[index].exercise_name), "%s", exercise.name);
        (void)snprintf(items[index].equipment_name, sizeof(items[index].equipment_name), "%s", equipment.display_name);
        (void)snprintf(items[index].zone_name, sizeof(items[index].zone_name), "%s",
            zone != NULL ? zone->display_name : session->exercises[index].primary_zone_id);
        (void)snprintf(items[index].movement_name, sizeof(items[index].movement_name), "%s",
            pattern != NULL ? pattern->display_name_fr : "mouvement non classé");
    }
    return true;
}

static bool generator_build_drafts(TrainlogDatabase *database,
    const TrainlogGeneratedSession *session, TrainlogSessionDraftExercise *drafts)
{
    size_t index;
    for (index = 0U; index < session->exercise_count; ++index) {
        const TrainlogGeneratedExercise *generated = &session->exercises[index];
        TrainlogExercise exercise;
        if (trainlog_database_get_exercise_profile(database, generated->exercise_id,
                &exercise) != TRAINLOG_STATUS_OK) return false;
        (void)memset(&drafts[index], 0, sizeof(drafts[index]));
        (void)snprintf(drafts[index].input.exercise_id,
            sizeof(drafts[index].input.exercise_id), "%s", generated->exercise_id);
        (void)snprintf(drafts[index].input.equipment_id,
            sizeof(drafts[index].input.equipment_id), "%s", generated->equipment_id);
        (void)snprintf(drafts[index].name, sizeof(drafts[index].name), "%s", exercise.name);
        drafts[index].tracking_mode = TRAINLOG_TRACKING_REPS;
        drafts[index].input.recording_mode = TRAINLOG_RECORDING_SETS;
        drafts[index].input.load_mode = generated->planned_load_mode;
        drafts[index].input.target_sets = generated->target_sets;
        drafts[index].input.target_reps = generated->target_repetitions;
        drafts[index].input.rest_seconds = generated->rest_seconds;
        drafts[index].input.target_has_weight = generated->has_target_weight;
        drafts[index].input.target_weight_kg = generated->target_weight_kg;
        /* INVARIANT: generated dose is a plan. No performed row exists until
         * the normal editor records a real metric supplied by the user. */
        drafts[index].input.set_count = 0U;
        draft_bind_input(&drafts[index]);
    }
    return true;
}

static TrainlogStatus persist_new_session_drafts(TrainlogDatabase *database,
    TrainlogSessionDraftExercise *drafts, size_t exercise_count,
    const char *started_at, TrainlogSessionType session_type)
{
    TrainlogSessionExerciseInput *inputs;
    TrainlogSessionInput session;
    char session_id[TRAINLOG_GENERATED_ID_CAPACITY];
    char ended_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    TrainlogStatus status;
    size_t index;
    if (database == NULL || drafts == NULL || started_at == NULL ||
        exercise_count == 0U ||
        exercise_count > MAX_SESSION_EXERCISES) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    inputs = calloc(exercise_count, sizeof(*inputs));
    if (inputs == NULL) return TRAINLOG_STATUS_SYSTEM_ERROR;
    if (trainlog_id_generate("se", session_id, sizeof(session_id)) != TRAINLOG_STATUS_OK ||
        trainlog_time_now_rfc3339(ended_at, sizeof(ended_at)) != TRAINLOG_STATUS_OK) {
        free(inputs);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    for (index = 0U; index < exercise_count; ++index) {
        draft_bind_input(&drafts[index]);
        inputs[index] = drafts[index].input;
    }
    (void)memset(&session, 0, sizeof(session));
    session.session_type = session_type;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s", ended_at);
    session.exercises = inputs;
    session.exercise_count = exercise_count;
    status = trainlog_database_insert_session(database, &session);
    free(inputs);
    return status;
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

typedef enum TrainlogSessionPhase {
    TRAINLOG_SESSION_IDLE = 0,
    TRAINLOG_SESSION_CHOOSE_TYPE,
    TRAINLOG_SESSION_EXERCISE_PICKER,
    TRAINLOG_SESSION_EQUIPMENT_PICKER,
    TRAINLOG_SESSION_EQUIPMENT_CREATE,
    TRAINLOG_SESSION_PLANNING,
    TRAINLOG_SESSION_DRAFT,
    TRAINLOG_SESSION_ACTUALS,
    TRAINLOG_SESSION_GENERATOR_CONFIG,
    TRAINLOG_SESSION_GENERATOR_WARNING,
    TRAINLOG_SESSION_GENERATOR_PREVIEW,
    TRAINLOG_SESSION_CONFIRM_REMOVE,
    TRAINLOG_SESSION_CONFIRM_ABANDON,
    TRAINLOG_SESSION_CONFIRM_LEAVE,
    TRAINLOG_SESSION_MESSAGE
} TrainlogSessionPhase;

typedef enum TrainlogSessionFormPurpose {
    TRAINLOG_SESSION_FORM_NONE = 0,
    TRAINLOG_SESSION_FORM_SET_METRIC,
    TRAINLOG_SESSION_FORM_SET_WEIGHT,
    TRAINLOG_SESSION_FORM_CONTINUOUS_DURATION,
    TRAINLOG_SESSION_FORM_CONTINUOUS_SPEED,
    TRAINLOG_SESSION_FORM_CONTINUOUS_DISTANCE,
    TRAINLOG_SESSION_FORM_TARGET_SETS,
    TRAINLOG_SESSION_FORM_TARGET_REPS,
    TRAINLOG_SESSION_FORM_TARGET_DURATION,
    TRAINLOG_SESSION_FORM_REST,
    TRAINLOG_SESSION_FORM_TARGET_WEIGHT,
    TRAINLOG_SESSION_FORM_TARGET_PERCENT_MAX,
    TRAINLOG_SESSION_FORM_EQUIPMENT_NAME,
    TRAINLOG_SESSION_FORM_EQUIPMENT_LABEL,
    TRAINLOG_SESSION_FORM_EQUIPMENT_TYPE,
    TRAINLOG_SESSION_FORM_EQUIPMENT_LOAD,
    TRAINLOG_SESSION_FORM_GENERATED_SETS,
    TRAINLOG_SESSION_FORM_GENERATED_REPS,
    TRAINLOG_SESSION_FORM_GENERATED_PERCENT_MAX,
    TRAINLOG_SESSION_FORM_MAX_WEIGHT
} TrainlogSessionFormPurpose;

typedef struct TrainlogSessionController {
    /* CONTRACT: desktop draft durability is exactly one process run. Route
     * changes retain this controller; only explicit discard clears it. */
    bool has_draft;
    bool dirty;
    bool generated_preview;
    bool generation_warning_acknowledged;
    bool correcting;
    bool replacing_occurrence;
    TrainlogSessionDraftExercise drafts[MAX_SESSION_EXERCISES];
    size_t draft_count;
    size_t selected;
    size_t set_selected;
    size_t set_field;
    size_t planning_field;
    TrainlogSessionType session_type;
    TrainlogSessionPhase phase;
    TrainlogSessionPhase return_phase;
    TrainlogSessionFormPurpose form_purpose;
    TrainlogFormField form;
    TrainlogSetInput pending_set;
    int pending_generated_sets;
    TrainlogExercise picker[MAX_EXERCISES];
    size_t picker_count;
    size_t picker_selected;
    size_t equipment_count;
    size_t equipment_selected;
    TrainlogCustomEquipment pending_equipment;
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char generation_reference_time[TRAINLOG_TIMESTAMP_MAX + 1U];
    char persisted_session_id[TRAINLOG_ID_MAX + 1U];
    char message[192];
    TrainlogAppRoute pending_route;
    TrainlogGeneratedSession generated;
    TrainlogGeneratorPreviewItem generated_items[TRAINLOG_GENERATOR_MAX_SELECTED];
    const TrainlogBodyZone *generation_zone;
    const TrainlogSessionGenerationGoalPolicy *generation_goal;
    size_t generation_zone_index;
    size_t generation_goal_index;
    size_t generation_duration_index;
    int generation_duration_minutes;
    TrainlogDurabilityState durability;
} TrainlogSessionController;

typedef enum TrainlogEquipmentPhase {
    TRAINLOG_EQUIPMENT_IDLE = 0,
    TRAINLOG_EQUIPMENT_NAME,
    TRAINLOG_EQUIPMENT_LABEL,
    TRAINLOG_EQUIPMENT_TYPE,
    TRAINLOG_EQUIPMENT_LOAD,
    TRAINLOG_EQUIPMENT_CONFIRM_DISCARD,
    TRAINLOG_EQUIPMENT_MESSAGE
} TrainlogEquipmentPhase;

typedef struct TrainlogEquipmentController {
    /* INVARIANT: this controller is the sole transient owner of a catalogue
     * equipment definition. Invalid submissions retain every raw field. */
    TrainlogEquipmentPhase phase;
    TrainlogEquipmentPhase return_phase;
    TrainlogFormField form;
    TrainlogCustomEquipment pending;
    int load_choice;
    bool dirty;
    bool leaving;
    TrainlogAppRoute pending_route;
    char message[192];
} TrainlogEquipmentController;

typedef enum TrainlogExercisePhase {
    TRAINLOG_EXERCISE_IDLE = 0,
    TRAINLOG_EXERCISE_NAME,
    TRAINLOG_EXERCISE_TRACKING,
    TRAINLOG_EXERCISE_RECORDING,
    TRAINLOG_EXERCISE_SPEED,
    TRAINLOG_EXERCISE_DISTANCE,
    TRAINLOG_EXERCISE_ZONES,
    TRAINLOG_EXERCISE_MERGE_PICKER,
    TRAINLOG_EXERCISE_MERGE_CONFIRM,
    TRAINLOG_EXERCISE_MERGE_RESULT,
    TRAINLOG_EXERCISE_CONFIRM_DISCARD,
    TRAINLOG_EXERCISE_MESSAGE
} TrainlogExercisePhase;

typedef struct TrainlogExerciseController {
    /* CONTRACT: creation/editing owns one transient value set. An edit keeps
     * exercise_id and passes the stored profile back unchanged; referenced
     * profile restrictions remain enforced by the database transaction. */
    TrainlogExercisePhase phase;
    TrainlogExercisePhase return_phase;
    TrainlogFormField form;
    TrainlogExercise pending;
    char primary_zone[TRAINLOG_ZONE_ID_MAX + 1U];
    char secondary_zones[MAX_BODY_ZONES][TRAINLOG_ZONE_ID_MAX + 1U];
    size_t secondary_count;
    size_t zone_selected;
    bool editing;
    bool inline_session;
    bool dirty;
    bool leaving;
    TrainlogAppRoute pending_route;
    char message[192];
    TrainlogExercise merge_candidates[MAX_EXERCISES];
    size_t merge_count;
    size_t merge_selected;
    TrainlogExercise merge_source;
    TrainlogExercise merge_target;
    TrainlogExerciseMergePreview merge_preview;
} TrainlogExerciseController;

typedef enum TrainlogBodyPhase {
    TRAINLOG_BODY_IDLE = 0,
    TRAINLOG_BODY_FORM,
    TRAINLOG_BODY_CONFIRM_DISCARD,
    TRAINLOG_BODY_MESSAGE
} TrainlogBodyPhase;

typedef struct TrainlogBodyController {
    /* CONTRACT: one controller owns all fourteen raw metric fields until one
     * insert/update succeeds. Editing copies the immutable record identity,
     * timestamp and session link and never permits those fields to drift. */
    TrainlogBodyPhase phase;
    TrainlogFormField form;
    TrainlogBodyObservationInput pending;
    char session_id[TRAINLOG_ID_MAX + 1U];
    char notes[TRAINLOG_NOTE_MAX + 1U];
    size_t field;
    bool editing;
    bool dirty;
    bool leaving;
    TrainlogAppRoute pending_route;
    char message[192];
} TrainlogBodyController;

typedef enum TrainlogProfilePhase {
    TRAINLOG_PROFILE_IDLE = 0,
    TRAINLOG_PROFILE_FORMULA,
    TRAINLOG_PROFILE_HEIGHT,
    TRAINLOG_PROFILE_CONFIRM_DISCARD,
    TRAINLOG_PROFILE_MESSAGE
} TrainlogProfilePhase;

typedef struct TrainlogProfileController {
    TrainlogProfilePhase phase;
    TrainlogProfilePhase return_phase;
    TrainlogFormField form;
    TrainlogBodyAnalyticsProfile pending;
    TrainlogAppRoute caller;
    bool dirty;
    bool leaving;
    TrainlogAppRoute pending_route;
    char message[192];
} TrainlogProfileController;

typedef TrainlogStatus (*TrainlogAppSyncProbe)(TrainlogSyncDeviceInfo *output);
typedef TrainlogStatus (*TrainlogAppSyncRun)(TrainlogSyncTrigger trigger,
    bool require_request, TrainlogSyncDirection direction,
    TrainlogSyncReport *output);

typedef struct TrainlogSyncController {
    /* Navigation only reads cached local journal state. Device discovery and
     * the synchronous engine are invoked solely by explicit controller keys. */
    TrainlogSyncScreenState action;
    TrainlogSyncDeviceInfo device;
    TrainlogStatus probe_status;
    bool probe_known;
    TrainlogSyncReport report;
    bool has_report;
    bool running;
    TrainlogSyncHistoryEntry history[SYNC_HISTORY_CAPACITY];
    size_t history_count;
    size_t selected;
    char detail_text[SYNC_DETAIL_TEXT_CAPACITY];
    char *detail_lines[SYNC_DETAIL_LINE_CAPACITY];
    size_t detail_line_count;
    size_t detail_scroll;
    bool showing_detail;
    TrainlogAppSyncProbe probe;
    TrainlogAppSyncRun run;
} TrainlogSyncController;

typedef struct TrainlogAppContext {
    TrainlogDatabase *database;
    TrainlogTerminal *terminal;
    TrainlogNavigationState navigation;
    TrainlogSessionController session;
    TrainlogEquipmentController equipment_controller;
    TrainlogExerciseController exercise_controller;
    TrainlogBodyController body_controller;
    TrainlogProfileController profile_controller;
    TrainlogSyncController sync_controller;
    TrainlogFocusTarget focus;
    TrainlogActionModel actions;
    TrainlogOverlayStack overlays;
    TrainlogShellLayout layout;
    TrainlogSurface *header;
    TrainlogSurface *sidebar;
    TrainlogSurface *content;
    TrainlogSurface *footer;
    size_t navigation_selected;
    TrainlogFocusTarget navigation_restore_focus;
    size_t content_selected;
    size_t content_scroll;
    TrainlogListState list;
    TrainlogSearchState search;
    TrainlogListState saved_lists[TRAINLOG_ROUTE_COUNT];
    TrainlogSearchState saved_searches[TRAINLOG_ROUTE_COUNT];
    TrainlogExercise exercises[MAX_EXERCISES];
    TrainlogResolvedEquipment equipment[256];
    TrainlogSessionSummary sessions[MAX_SESSIONS];
    const char *stable_ids[MAX_BODY_OBSERVATIONS];
    size_t loaded_count;
    int exercise_zone_filter;
    bool list_error;
    TrainlogExercise exercise_detail;
    TrainlogExerciseBodyZone exercise_zones[MAX_BODY_ZONES];
    size_t exercise_zone_count;
    TrainlogResolvedEquipment exercise_explicit_equipment[64];
    size_t exercise_explicit_equipment_count;
    TrainlogResolvedEquipment exercise_historic_equipment[128];
    size_t exercise_historic_equipment_count;
    size_t exercise_equipment_selected;
    bool exercise_detail_metadata_error;
    TrainlogResolvedEquipment equipment_detail;
    TrainlogExercisePerformancePoint performance_points[MAX_SESSIONS];
    size_t performance_count;
    TrainlogMeasuredMaxSummary max_summary;
    size_t max_rounding_index;
    bool performance_error;
    TrainlogSessionSummary session_detail;
    TrainlogPersistedExerciseDetail session_entries[MAX_SESSION_EXERCISES];
    size_t session_entry_count;
    size_t session_entry_selected;
    size_t session_set_scroll;
    bool session_detail_error;
    TrainlogBodyObservationRecord body_records[MAX_BODY_OBSERVATIONS];
    TrainlogBodyObservationRecord body_detail;
    TrainlogBodyMetricPoint body_metric_points[MAX_BODY_METRIC_POINTS];
    size_t body_metric_count;
    bool body_metric_partial;
    size_t body_metric_selected;
    TrainlogGlobalBodySeries body_global_series[14];
    bool body_global_enabled[14];
    bool body_global_initialized;
    size_t body_global_selected;
    char body_global_dates[MAX_GLOBAL_BODY_DATES][TRAINLOG_TIMESTAMP_MAX + 1U];
    size_t body_global_date_count;
    bool body_global_partial;
    bool body_snapshot_error;
    TrainlogBodyAnalyticsProfile body_profile;
    bool body_has_profile;
    bool quit_confirmation;
    bool running;
} TrainlogAppContext;

static void app_shell_open_route(TrainlogAppContext *app,
                                 TrainlogAppRoute route);
static void app_shell_load_exercise_detail_metadata(TrainlogAppContext *app);
static void app_shell_refresh_list(TrainlogAppContext *app);

static void body_metric_slot(TrainlogBodyObservationInput *input, size_t field,
                             bool **present, double **value)
{
    *present = NULL; *value = NULL;
#define BODY_SLOT(index_, has_, value_) case index_: *present = &input->has_; \
    *value = &input->value_; break
    switch (field) {
    BODY_SLOT(0U, has_body_weight, body_weight_kg);
    BODY_SLOT(1U, has_neck, neck_cm);
    BODY_SLOT(2U, has_shoulders, shoulders_cm);
    BODY_SLOT(3U, has_chest, chest_cm);
    BODY_SLOT(4U, has_waist, waist_cm);
    BODY_SLOT(5U, has_hips, hips_cm);
    BODY_SLOT(6U, has_left_arm, left_arm_cm);
    BODY_SLOT(7U, has_right_arm, right_arm_cm);
    BODY_SLOT(8U, has_left_forearm, left_forearm_cm);
    BODY_SLOT(9U, has_right_forearm, right_forearm_cm);
    BODY_SLOT(10U, has_left_thigh, left_thigh_cm);
    BODY_SLOT(11U, has_right_thigh, right_thigh_cm);
    BODY_SLOT(12U, has_left_calf, left_calf_cm);
    BODY_SLOT(13U, has_right_calf, right_calf_cm);
    default: break;
    }
#undef BODY_SLOT
}

static void body_controller_open_field(TrainlogBodyController *controller)
{
    bool *present;
    double *value;
    char text[64] = "";
    body_metric_slot(&controller->pending, controller->field, &present, &value);
    if (controller->editing && present != NULL && *present)
        (void)snprintf(text, sizeof(text), "%.10g", *value);
    trainlog_form_init(&controller->form, text);
    controller->phase = TRAINLOG_BODY_FORM;
}

static void body_controller_start_add(TrainlogAppContext *app)
{
    TrainlogBodyController *controller = &app->body_controller;
    (void)memset(controller, 0, sizeof(*controller));
    if (trainlog_id_generate("bo", controller->pending.observation_id,
            sizeof(controller->pending.observation_id)) != TRAINLOG_STATUS_OK ||
        trainlog_time_now_rfc3339(controller->pending.observed_at,
            sizeof(controller->pending.observed_at)) != TRAINLOG_STATUS_OK) {
        controller->phase = TRAINLOG_BODY_MESSAGE;
        (void)snprintf(controller->message, sizeof(controller->message),
            "Impossible de préparer un identifiant ou une date de relevé.");
        return;
    }
    body_controller_open_field(controller);
    app->focus = TRAINLOG_FOCUS_EDITOR;
}

static bool body_controller_start_edit(TrainlogAppContext *app,
                                       const char *observation_id)
{
    TrainlogBodyController *controller = &app->body_controller;
    TrainlogBodyObservationRecord record;
    (void)memset(controller, 0, sizeof(*controller));
    if (trainlog_database_get_body_observation(app->database, observation_id,
            &record) != TRAINLOG_STATUS_OK) return false;
    body_input_from_record(&record, &controller->pending);
    (void)snprintf(controller->session_id, sizeof(controller->session_id),
        "%s", record.session_id);
    (void)snprintf(controller->notes, sizeof(controller->notes), "%s",
        record.notes);
    controller->pending.session_id = controller->session_id[0] != '\0'
        ? controller->session_id : NULL;
    controller->pending.notes = controller->notes[0] != '\0'
        ? controller->notes : NULL;
    controller->editing = true;
    body_controller_open_field(controller);
    app->focus = TRAINLOG_FOCUS_EDITOR;
    return true;
}

static bool body_controller_persist(TrainlogAppContext *app)
{
    TrainlogBodyController *controller = &app->body_controller;
    TrainlogStatus status = controller->editing
        ? trainlog_database_update_body_observation(app->database,
            &controller->pending)
        : trainlog_database_insert_body_observation(app->database,
            &controller->pending);
    if (status != TRAINLOG_STATUS_OK) {
        (void)snprintf(controller->message, sizeof(controller->message),
            "Enregistrement impossible : au moins une mesure positive est requise.");
        return false;
    }
    controller->dirty = false;
    controller->form.active = false;
    controller->phase = TRAINLOG_BODY_MESSAGE;
    (void)snprintf(controller->message, sizeof(controller->message), "%s",
        controller->editing ? "Relevé corrigé; identité, date et séance conservées."
                            : "Relevé corporel enregistré.");
    if (app->navigation.current.route == TRAINLOG_ROUTE_BODY)
        app_shell_refresh_list(app);
    if (controller->editing &&
        app->navigation.current.route == TRAINLOG_ROUTE_BODY_DETAIL)
        (void)trainlog_database_get_body_observation(app->database,
            controller->pending.observation_id, &app->body_detail);
    return true;
}

static bool app_shell_dispatch_body_controller(TrainlogAppContext *app, int key)
{
    TrainlogBodyController *controller = &app->body_controller;
    TrainlogFormResult result;
    if (controller->phase == TRAINLOG_BODY_IDLE) return false;
    if (controller->phase == TRAINLOG_BODY_MESSAGE) {
        if (key == TRAINLOG_KEY_ENTER || key == '\n' ||
            key == TRAINLOG_KEY_ESCAPE) {
            (void)memset(controller, 0, sizeof(*controller));
            app->focus = TRAINLOG_FOCUS_CONTENT;
        }
        return true;
    }
    if (controller->phase == TRAINLOG_BODY_CONFIRM_DISCARD) {
        if (key == '1') {
            bool leaving = controller->leaving;
            TrainlogAppRoute route = controller->pending_route;
            (void)memset(controller, 0, sizeof(*controller));
            app->focus = TRAINLOG_FOCUS_CONTENT;
            if (leaving) app_shell_open_route(app, route);
        } else if (key == '0' || key == TRAINLOG_KEY_ESCAPE) {
            controller->phase = TRAINLOG_BODY_FORM;
            controller->leaving = false;
        }
        return true;
    }
    result = trainlog_form_handle(&controller->form, key);
    if (result == TRAINLOG_FORM_EDITED) {
        controller->dirty = true;
        controller->message[0] = '\0';
    } else if (result == TRAINLOG_FORM_SUBMIT) {
        bool *present;
        double *value;
        double parsed = 0.0;
        body_metric_slot(&controller->pending, controller->field,
            &present, &value);
        if (present == NULL || value == NULL) return true;
        if (controller->form.text[0] == '\0') {
            if (!controller->editing) { *present = false; *value = 0.0; }
        } else if (controller->editing &&
                   strcmp(controller->form.text, "-") == 0) {
            *present = false; *value = 0.0;
        } else if (!parse_double_positive(controller->form.text, &parsed)) {
            (void)snprintf(controller->message, sizeof(controller->message),
                "Nombre positif invalide; le texte saisi est conservé.");
            return true;
        } else { *present = true; *value = parsed; }
        controller->dirty = true;
        if (controller->field + 1U < 14U) {
            ++controller->field;
            body_controller_open_field(controller);
        } else (void)body_controller_persist(app);
    } else if (result == TRAINLOG_FORM_CANCEL) {
        if (controller->dirty) controller->phase = TRAINLOG_BODY_CONFIRM_DISCARD;
        else { (void)memset(controller, 0, sizeof(*controller));
            app->focus = TRAINLOG_FOCUS_CONTENT; }
    } else if (result == TRAINLOG_FORM_OPEN_NAVIGATION)
        (void)trainlog_overlays_push(&app->overlays,
            TRAINLOG_OVERLAY_NAVIGATION, app->focus,
            app->navigation.current.stable_id);
    else if (result == TRAINLOG_FORM_OPEN_ACTIONS)
        (void)trainlog_overlays_push(&app->overlays,
            TRAINLOG_OVERLAY_ACTIONS, app->focus,
            app->navigation.current.stable_id);
    return true;
}

static void profile_controller_start(TrainlogAppContext *app,
                                     TrainlogAppRoute caller)
{
    TrainlogProfileController *controller = &app->profile_controller;
    char text[8];
    (void)memset(controller, 0, sizeof(*controller));
    controller->caller = caller;
    if (!body_analytics_profile_load(&controller->pending)) {
        controller->pending.formula = TRAINLOG_BODY_ANALYTICS_FORMULA_MALE;
        controller->pending.height_cm = 170.0;
    }
    controller->phase = TRAINLOG_PROFILE_FORMULA;
    controller->return_phase = TRAINLOG_PROFILE_FORMULA;
    (void)snprintf(text, sizeof(text), "%d",
        controller->pending.formula == TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
            ? 2 : 1);
    trainlog_form_init(&controller->form, text);
    app->focus = TRAINLOG_FOCUS_EDITOR;
}

static bool app_shell_dispatch_profile_controller(TrainlogAppContext *app,
                                                  int key)
{
    TrainlogProfileController *controller = &app->profile_controller;
    TrainlogFormResult result;
    if (controller->phase == TRAINLOG_PROFILE_IDLE) return false;
    if (controller->phase == TRAINLOG_PROFILE_MESSAGE) {
        if (key == TRAINLOG_KEY_ENTER || key == '\n' || key == TRAINLOG_KEY_ESCAPE) {
            TrainlogAppRoute caller = controller->caller;
            (void)memset(controller, 0, sizeof(*controller));
            app->focus = TRAINLOG_FOCUS_CONTENT;
            if (app->navigation.current.route == caller)
                app->body_has_profile = body_analytics_profile_load(
                    &app->body_profile);
        }
        return true;
    }
    if (controller->phase == TRAINLOG_PROFILE_CONFIRM_DISCARD) {
        if (key == '1') {
            bool leaving = controller->leaving;
            TrainlogAppRoute route = controller->pending_route;
            (void)memset(controller, 0, sizeof(*controller));
            app->focus = TRAINLOG_FOCUS_CONTENT;
            if (leaving) app_shell_open_route(app, route);
        } else if (key == '0' || key == TRAINLOG_KEY_ESCAPE)
            controller->phase = controller->return_phase;
        return true;
    }
    result = trainlog_form_handle(&controller->form, key);
    if (result == TRAINLOG_FORM_EDITED) controller->dirty = true;
    else if (result == TRAINLOG_FORM_SUBMIT &&
             controller->phase == TRAINLOG_PROFILE_FORMULA) {
        if (strcmp(controller->form.text, "1") != 0 &&
            strcmp(controller->form.text, "2") != 0) {
            (void)snprintf(controller->message, sizeof(controller->message),
                "Formule attendue : 1 homme ou 2 femme.");
            return true;
        }
        controller->pending.formula = strcmp(controller->form.text, "2") == 0
            ? TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
            : TRAINLOG_BODY_ANALYTICS_FORMULA_MALE;
        controller->phase = TRAINLOG_PROFILE_HEIGHT;
        controller->return_phase = TRAINLOG_PROFILE_HEIGHT;
        { char text[64]; (void)snprintf(text, sizeof(text), "%.1f",
              controller->pending.height_cm); trainlog_form_init(&controller->form, text); }
    } else if (result == TRAINLOG_FORM_SUBMIT) {
        double height;
        if (!parse_double_positive(controller->form.text, &height) ||
            height < 100.0 || height > 250.0) {
            (void)snprintf(controller->message, sizeof(controller->message),
                "Taille attendue entre 100 et 250 cm; texte conservé.");
            return true;
        }
        controller->pending.height_cm = height;
        if (!body_analytics_profile_save(&controller->pending)) {
            (void)snprintf(controller->message, sizeof(controller->message),
                "Impossible d’enregistrer le profil local; champs conservés.");
            return true;
        }
        controller->dirty = false;
        controller->phase = TRAINLOG_PROFILE_MESSAGE;
        (void)snprintf(controller->message, sizeof(controller->message),
            "Profil d’estimation corporelle enregistré localement.");
    } else if (result == TRAINLOG_FORM_CANCEL) {
        if (controller->dirty) {
            controller->return_phase = controller->phase;
            controller->phase = TRAINLOG_PROFILE_CONFIRM_DISCARD;
        }
        else { (void)memset(controller, 0, sizeof(*controller));
            app->focus = TRAINLOG_FOCUS_CONTENT; }
    } else if (result == TRAINLOG_FORM_OPEN_NAVIGATION)
        (void)trainlog_overlays_push(&app->overlays,
            TRAINLOG_OVERLAY_NAVIGATION, app->focus,
            app->navigation.current.stable_id);
    else if (result == TRAINLOG_FORM_OPEN_ACTIONS)
        (void)trainlog_overlays_push(&app->overlays,
            TRAINLOG_OVERLAY_ACTIONS, app->focus,
            app->navigation.current.stable_id);
    return true;
}

static void session_controller_sync_durability(TrainlogSessionController *session)
{
    session->durability.session_draft = session->has_draft;
    session->durability.session_dirty = session->dirty;
    session->durability.generator_configuration_dirty =
        session->phase == TRAINLOG_SESSION_GENERATOR_CONFIG &&
        (session->generation_zone != NULL || session->generation_goal != NULL);
    session->durability.generator_preview = session->generated_preview;
    session->durability.generator_preview_dirty = session->generated_preview;
    session->durability.transient_form_dirty = session->form.active &&
        session->form.bytes > 0U;
}

static void session_controller_clear_draft(TrainlogSessionController *session)
{
    /* CONTRACT: this is called only by explicit abandon or a successful save.
     * Route changes retain the complete run-memory draft. */
    (void)memset(session->drafts, 0, sizeof(session->drafts));
    session->has_draft = false;
    session->dirty = false;
    session->correcting = false;
    session->draft_count = 0U;
    session->selected = 0U;
    session->persisted_session_id[0] = '\0';
    session->started_at[0] = '\0';
    session_controller_sync_durability(session);
}

static void session_controller_discard_generator(TrainlogSessionController *session)
{
    /* INVARIANT: no Back/navigation path calls this helper.  Preview edits,
     * raw configuration and warning acknowledgement survive keep-and-leave. */
    (void)memset(&session->generated, 0, sizeof(session->generated));
    (void)memset(session->generated_items, 0, sizeof(session->generated_items));
    session->generated_preview = false;
    session->generation_warning_acknowledged = false;
    session->generation_zone = NULL;
    session->generation_goal = NULL;
    session->generation_duration_minutes = 0;
    session->generation_reference_time[0] = '\0';
    session_controller_sync_durability(session);
}

static bool session_controller_load_picker(TrainlogAppContext *app)
{
    TrainlogSessionController *session = &app->session;
    session->picker_count = 0U;
    session->picker_selected = 0U;
    if (trainlog_database_list_exercises(app->database, session->picker,
        MAX_EXERCISES, &session->picker_count) != TRAINLOG_STATUS_OK) {
        (void)snprintf(session->message, sizeof(session->message),
            "Impossible de lire le catalogue d’exercices.");
        session->phase = TRAINLOG_SESSION_MESSAGE;
        return false;
    }
    if (session->picker_count == 0U) {
        (void)snprintf(session->message, sizeof(session->message),
            "Ajoutez d’abord au moins un exercice au catalogue.");
        session->phase = TRAINLOG_SESSION_MESSAGE;
        return false;
    }
    session->phase = TRAINLOG_SESSION_EXERCISE_PICKER;
    return true;
}

static void session_controller_load_equipment(TrainlogAppContext *app)
{
    TrainlogSessionController *session = &app->session;
    size_t index;
    session->equipment_count = equipment_collect(app->database, "",
        app->equipment, sizeof(app->equipment) / sizeof(app->equipment[0]),
        NULL, NULL);
    session->equipment_selected = 0U; /* zero is the explicit none choice */
    if (session->selected < session->draft_count) {
        const char *current = session->drafts[session->selected].input.equipment_id;
        for (index = 0U; current[0] != '\0' && index < session->equipment_count; ++index)
            if (strcmp(current, app->equipment[index].equipment_id) == 0) {
                session->equipment_selected = index + 1U;
                break;
            }
    }
    session->phase = TRAINLOG_SESSION_EQUIPMENT_PICKER;
}

static bool session_controller_choose_exercise(TrainlogSessionController *session)
{
    TrainlogSessionDraftExercise replacement;
    TrainlogSessionDraftExercise *target;
    const TrainlogExercise *exercise;
    char preserved_id[TRAINLOG_ID_MAX + 1U] = "";
    char preserved_notes[TRAINLOG_NOTE_MAX + 1U] = "";
    if (session->picker_count == 0U ||
        session->picker_selected >= session->picker_count) return false;
    if (!session->replacing_occurrence &&
        session->draft_count >= MAX_SESSION_EXERCISES) return false;
    exercise = &session->picker[session->picker_selected];
    (void)memset(&replacement, 0, sizeof(replacement));
    if (session->replacing_occurrence && session->selected < session->draft_count) {
        target = &session->drafts[session->selected];
        (void)snprintf(preserved_id, sizeof(preserved_id), "%s",
            target->input.entry_id);
        (void)snprintf(preserved_notes, sizeof(preserved_notes), "%s", target->notes);
    }
    (void)snprintf(replacement.input.entry_id,
        sizeof(replacement.input.entry_id), "%s", preserved_id);
    (void)snprintf(replacement.input.exercise_id,
        sizeof(replacement.input.exercise_id), "%s", exercise->exercise_id);
    (void)snprintf(replacement.name, sizeof(replacement.name), "%s", exercise->name);
    (void)snprintf(replacement.notes, sizeof(replacement.notes), "%s", preserved_notes);
    replacement.tracking_mode = exercise->tracking_mode;
    replacement.input.recording_mode = exercise->recording_mode;
    replacement.input.data_fields = exercise->data_fields;
    replacement.input.load_mode = TRAINLOG_LOAD_NONE;
    draft_bind_input(&replacement);
    if (session->replacing_occurrence) session->drafts[session->selected] = replacement;
    else {
        session->selected = session->draft_count;
        session->drafts[session->draft_count++] = replacement;
    }
    draft_bind_input(&session->drafts[session->selected]);
    session->has_draft = true;
    session->dirty = true;
    session->replacing_occurrence = false;
    session->phase = TRAINLOG_SESSION_DRAFT;
    session_controller_sync_durability(session);
    return true;
}

static bool session_controller_actuals_valid(TrainlogSessionController *session)
{
    size_t index;
    if (session->draft_count == 0U) {
        (void)snprintf(session->message, sizeof(session->message),
            "Ajoutez au moins un exercice avant d’enregistrer.");
        return false;
    }
    for (index = 0U; index < session->draft_count; ++index) {
        TrainlogSessionExerciseInput *input = &session->drafts[index].input;
        bool valid = input->has_max_weight ||
            (input->recording_mode == TRAINLOG_RECORDING_CONTINUOUS
                ? input->continuous_duration_seconds > 0 : input->set_count > 0U);
        if (!valid) {
            session->selected = index;
            (void)snprintf(session->message, sizeof(session->message),
                "Saisissez un résultat réel pour %.120s.", session->drafts[index].name);
            return false;
        }
    }
    return true;
}

static TrainlogStatus session_controller_save(TrainlogAppContext *app)
{
    TrainlogSessionController *session = &app->session;
    TrainlogStatus status;
    if (!session_controller_actuals_valid(session)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    if (session->correcting) status = persist_draft_replacement(app->database,
        session->persisted_session_id, session->drafts, session->draft_count)
        ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_DATABASE_ERROR;
    else status = persist_new_session_drafts(app->database, session->drafts,
        session->draft_count, session->started_at, session->session_type);
    if (status == TRAINLOG_STATUS_OK) session_controller_clear_draft(session);
    return status;
}

static bool session_controller_generate(TrainlogAppContext *app)
{
    TrainlogSessionController *session = &app->session;
    TrainlogGenerationDatabaseRequest request;
    char reference_time[TRAINLOG_TIMESTAMP_MAX + 1U];
    TrainlogStatus status;
    if (session->generation_zone == NULL || session->generation_goal == NULL ||
        session->generation_duration_minutes <= 0) return false;
    if (session->generation_reference_time[0] == '\0' &&
        trainlog_time_now_rfc3339(session->generation_reference_time,
            sizeof(session->generation_reference_time)) != TRAINLOG_STATUS_OK) return false;
    (void)snprintf(reference_time, sizeof(reference_time), "%s",
        session->generation_reference_time);
    (void)memset(&request, 0, sizeof(request));
    request.zone_id = session->generation_zone->zone_id;
    request.goal_id = session->generation_goal->id;
    request.duration_minutes = session->generation_duration_minutes;
    request.reference_time = reference_time;
    status = trainlog_session_generate_from_database(app->database, &request,
        &session->generated);
    if (status != TRAINLOG_STATUS_OK) {
        (void)snprintf(session->message, sizeof(session->message),
            "Génération refusée (%d).", (int)status);
        session->return_phase = TRAINLOG_SESSION_GENERATOR_CONFIG;
        session->phase = TRAINLOG_SESSION_MESSAGE;
        return false;
    }
    if (session->generated.exercise_count == 0U) {
        (void)snprintf(session->message, sizeof(session->message),
            "Pas assez d’exercices résolus et compatibles; rien n’a été inventé.");
        session->return_phase = TRAINLOG_SESSION_GENERATOR_CONFIG;
        session->phase = TRAINLOG_SESSION_MESSAGE;
        return false;
    }
    if (!generator_prepare_preview(app->database, &session->generated,
        session->generated_items)) return false;
    session->generated_preview = true;
    session->selected = 0U;
    session->phase = session->generated.exposure.warning_level ==
        TRAINLOG_GENERATION_WARNING_NONE || session->generation_warning_acknowledged
        ? TRAINLOG_SESSION_GENERATOR_PREVIEW : TRAINLOG_SESSION_GENERATOR_WARNING;
    session_controller_sync_durability(session);
    return true;
}

static TrainlogStatus session_requalify_history(void *context,
    const TrainlogGenerationHistoryRow *row)
{
    return trainlog_session_generation_analyzer_accept(
        (TrainlogSessionGenerationAnalyzer *)context, row);
}

static bool session_controller_requalify_generated(TrainlogAppContext *app,
                                                    size_t selected,
                                                    int sets, int repetitions)
{
    TrainlogSessionController *session = &app->session;
    TrainlogGeneratedExercise *current;
    TrainlogGenerationCandidate candidate;
    TrainlogGenerationRequest request;
    TrainlogSessionGenerationAnalyzer *analyzer = NULL;
    TrainlogGeneratedSession result;
    const char *secondary[TRAINLOG_GENERATOR_MAX_ZONES];
    const char *patterns[TRAINLOG_GENERATOR_MAX_PATTERNS];
    const char *sources[TRAINLOG_GENERATOR_MAX_SOURCE_REFS];
    size_t index;
    TrainlogStatus status;
    if (selected >= session->generated.exercise_count ||
        session->generation_zone == NULL || session->generation_goal == NULL ||
        session->generation_reference_time[0] == '\0') return false;
    current = &session->generated.exercises[selected];
    for (index = 0U; index < current->secondary_zone_count; ++index)
        secondary[index] = current->secondary_zone_ids[index];
    for (index = 0U; index < current->pattern_count; ++index)
        patterns[index] = current->pattern_ids[index];
    for (index = 0U; index < current->source_ref_count; ++index)
        sources[index] = current->source_ref_ids[index];
    (void)memset(&candidate, 0, sizeof(candidate));
    candidate.exercise_id = current->exercise_id;
    candidate.equipment_id = current->equipment_id;
    candidate.primary_zone_id = current->primary_zone_id;
    candidate.secondary_zone_ids = secondary;
    candidate.secondary_zone_count = current->secondary_zone_count;
    candidate.pattern_ids = patterns;
    candidate.pattern_count = current->pattern_count;
    candidate.source_ref_ids = sources;
    candidate.source_ref_count = current->source_ref_count;
    candidate.confidence = current->confidence;
    candidate.equipment_load_semantics = current->equipment_load_semantics;
    (void)memset(&request, 0, sizeof(request));
    request.zone_id = session->generation_zone->zone_id;
    request.goal_id = session->generation_goal->id;
    request.duration_minutes = session->generation_duration_minutes;
    request.reference_time = session->generation_reference_time;
    request.candidates = &candidate;
    request.candidate_count = 1U;
    status = trainlog_session_generation_requalify_dose(&request, 0U, sets,
        repetitions, &analyzer);
    if (status == TRAINLOG_STATUS_OK)
        status = trainlog_database_scan_generation_history(app->database,
            session_requalify_history, analyzer);
    if (status == TRAINLOG_STATUS_OK)
        status = trainlog_session_generation_analyzer_finish(analyzer, &result);
    trainlog_session_generation_analyzer_destroy(analyzer);
    if (status != TRAINLOG_STATUS_OK || result.exercise_count != 1U) return false;
    session->generated.estimated_duration_seconds +=
        result.exercises[0].estimated_seconds - current->estimated_seconds;
    *current = result.exercises[0];
    return true;
}

static bool session_controller_accept_generator(TrainlogAppContext *app)
{
    TrainlogSessionController *session = &app->session;
    if (session->has_draft) {
        (void)snprintf(session->message, sizeof(session->message),
            "Une séance est déjà en cours. Reprenez-la ou gardez cet aperçu.");
        session->return_phase = TRAINLOG_SESSION_GENERATOR_PREVIEW;
        session->phase = TRAINLOG_SESSION_MESSAGE;
        return false;
    }
    if (!generator_build_drafts(app->database, &session->generated, session->drafts))
        return false;
    session->draft_count = session->generated.exercise_count;
    session->has_draft = true;
    session->dirty = true;
    session->session_type = TRAINLOG_SESSION_TRAINING;
    session->correcting = false;
    if (trainlog_time_now_rfc3339(session->started_at,
        sizeof(session->started_at)) != TRAINLOG_STATUS_OK) return false;
    /* CONTRACT: acceptance transfers planning metadata into the ordinary
     * draft and creates zero performed sets. */
    session_controller_discard_generator(session);
    session->phase = TRAINLOG_SESSION_DRAFT;
    session->selected = 0U;
    session_controller_sync_durability(session);
    return true;
}

static const TrainlogAppRoute shell_sections[] = {
    TRAINLOG_ROUTE_HOME, TRAINLOG_ROUTE_SESSIONS, TRAINLOG_ROUTE_EXERCISES,
    TRAINLOG_ROUTE_EQUIPMENT, TRAINLOG_ROUTE_STATS, TRAINLOG_ROUTE_SYNC,
    TRAINLOG_ROUTE_SETTINGS
};
static const char *const shell_section_labels[] = {
    "Accueil", "Séances", "Exercices", "Équipements", "Statistiques",
    "Synchronisation", "Paramètres"
};

static void app_shell_destroy_surfaces(TrainlogAppContext *app)
{
    /* INVARIANT: each plane has exactly one owner and destruction proceeds
     * from transient/inner content toward persistent outer chrome. */
    trainlog_surface_destroy(app->content); app->content = NULL;
    trainlog_surface_destroy(app->sidebar); app->sidebar = NULL;
    trainlog_surface_destroy(app->footer); app->footer = NULL;
    trainlog_surface_destroy(app->header); app->header = NULL;
}

static bool app_shell_layout(TrainlogAppContext *app)
{
    trainlog_shell_layout_compute(trainlog_terminal_columns(app->terminal),
        trainlog_terminal_rows(app->terminal), &app->layout);
    if (!app->layout.usable) { app_shell_destroy_surfaces(app); return true; }
    if (app->header == NULL)
        app->header = trainlog_surface_create(app->terminal, "trainlog.header",
            app->layout.header.y, app->layout.header.x,
            app->layout.header.height, app->layout.header.width);
    else if (!trainlog_surface_set_rect(app->header, app->layout.header.y,
        app->layout.header.x, app->layout.header.height, app->layout.header.width))
        return false;
    if (app->footer == NULL)
        app->footer = trainlog_surface_create(app->terminal, "trainlog.footer",
            app->layout.footer.y, app->layout.footer.x,
            app->layout.footer.height, app->layout.footer.width);
    else if (!trainlog_surface_set_rect(app->footer, app->layout.footer.y,
        app->layout.footer.x, app->layout.footer.height, app->layout.footer.width))
        return false;
    if (app->content == NULL)
        app->content = trainlog_surface_create(app->terminal, "trainlog.content",
            app->layout.content.y, app->layout.content.x,
            app->layout.content.height, app->layout.content.width);
    else if (!trainlog_surface_set_rect(app->content, app->layout.content.y,
        app->layout.content.x, app->layout.content.height, app->layout.content.width))
        return false;
    if (app->layout.sidebar_visible && app->sidebar == NULL)
        app->sidebar = trainlog_surface_create(app->terminal, "trainlog.sidebar",
            app->layout.sidebar.y, app->layout.sidebar.x,
            app->layout.sidebar.height, app->layout.sidebar.width);
    else if (app->layout.sidebar_visible && !trainlog_surface_set_rect(app->sidebar,
        app->layout.sidebar.y, app->layout.sidebar.x,
        app->layout.sidebar.height, app->layout.sidebar.width)) return false;
    else if (!app->layout.sidebar_visible && app->sidebar != NULL) {
        trainlog_surface_destroy(app->sidebar); app->sidebar = NULL;
        /* CONTRACT: compact Navigation replaces the disappearing sidebar;
         * retain both its selected section and its owned focus. */
    }
    return app->header != NULL && app->footer != NULL && app->content != NULL &&
        (!app->layout.sidebar_visible || app->sidebar != NULL);
}

static void app_shell_add_action(TrainlogAppContext *app, const char *identifier,
                                 int key, const char *label, unsigned priority,
                                 TrainlogIntent intent, TrainlogAppRoute route)
{
    TrainlogAction action = {identifier, key, label, true, priority, intent, route};
    (void)trainlog_actions_add(&app->actions, &action);
}

static bool app_shell_is_list_route(TrainlogAppRoute route)
{
    return route == TRAINLOG_ROUTE_EXERCISES || route == TRAINLOG_ROUTE_EQUIPMENT ||
        route == TRAINLOG_ROUTE_SESSIONS_COMPLETED ||
        route == TRAINLOG_ROUTE_STATS_EXERCISE || route == TRAINLOG_ROUTE_MAX ||
        route == TRAINLOG_ROUTE_BODY;
}

static void app_shell_refresh_list(TrainlogAppContext *app)
{
    TrainlogAppRoute route = app->navigation.current.route;
    size_t index;
    size_t count = 0U;
    size_t capacity = 0U;
    bool source_capped = false;
    app->list_error = false;
    if (route == TRAINLOG_ROUTE_EXERCISES || route == TRAINLOG_ROUTE_STATS_EXERCISE ||
        route == TRAINLOG_ROUTE_MAX) {
        char normalized[(TRAINLOG_NAME_MAX * 4U) + 1U] = "";
        const char *zone_id = NULL;
        size_t zone_count = trainlog_body_zone_catalog_count();
        bool unclassified = app->exercise_zone_filter == (int)zone_count;
        if (app->search.bytes > 0U && trainlog_catalog_normalize_name(app->search.text,
            normalized, sizeof(normalized)) != TRAINLOG_STATUS_OK) normalized[0] = '\0';
        if (app->exercise_zone_filter >= 0 &&
            app->exercise_zone_filter < (int)zone_count) {
            const TrainlogBodyZone *zone = trainlog_body_zone_catalog_at(
                (size_t)app->exercise_zone_filter);
            if (zone != NULL) zone_id = zone->zone_id;
        }
        if (trainlog_database_list_exercises_filtered(app->database, normalized,
            zone_id, true, false, unclassified, app->exercises,
            MAX_EXERCISES, &count) != TRAINLOG_STATUS_OK) app->list_error = true;
        capacity = MAX_EXERCISES;
        source_capped = count == capacity;
        for (index = 0U; index < count; ++index)
            app->stable_ids[index] = app->exercises[index].exercise_id;
    } else if (route == TRAINLOG_ROUTE_EQUIPMENT) {
        bool truncated = false;
        bool failed = false;
        count = equipment_collect(app->database, app->search.text,
            app->equipment, sizeof(app->equipment) / sizeof(app->equipment[0]),
            &truncated, &failed);
        capacity = sizeof(app->equipment) / sizeof(app->equipment[0]);
        source_capped = truncated;
        app->list_error = failed;
        for (index = 0U; index < count; ++index)
            app->stable_ids[index] = app->equipment[index].equipment_id;
    } else if (route == TRAINLOG_ROUTE_SESSIONS_COMPLETED) {
        TrainlogSessionSummary all[MAX_SESSIONS];
        size_t all_count = 0U;
        if (trainlog_database_list_sessions(app->database, all, MAX_SESSIONS,
            &all_count) != TRAINLOG_STATUS_OK) app->list_error = true;
        capacity = MAX_SESSIONS;
        source_capped = all_count == capacity;
        for (index = 0U; index < all_count && count < MAX_SESSIONS; ++index) {
            char date[17];
            const char *type = session_type_history_label(all[index].session_type);
            session_history_datetime(all[index].started_at, date);
            if (app->search.bytes == 0U || equipment_text_matches(date, app->search.text) ||
                equipment_text_matches(type, app->search.text)) {
                app->sessions[count] = all[index];
                app->stable_ids[count] = app->sessions[count].session_id;
                ++count;
            }
        }
    } else if (route == TRAINLOG_ROUTE_BODY) {
        if (trainlog_database_list_body_observations(app->database,
            app->body_records, MAX_BODY_OBSERVATIONS, &count) != TRAINLOG_STATUS_OK)
            app->list_error = true;
        capacity = MAX_BODY_OBSERVATIONS;
        source_capped = count == capacity;
        for (index = 0U; index < count; ++index)
            app->stable_ids[index] = app->body_records[index].observation_id;
    }
    app->loaded_count = count;
    trainlog_list_set_items(&app->list, app->stable_ids, count,
        app->layout.content.height > 8 ? (size_t)(app->layout.content.height - 8) : 1U,
        !source_capped, source_capped);
}

static void app_shell_load_body_metric(TrainlogAppContext *app)
{
    app->body_metric_count = 0U;
    app->body_snapshot_error =
        trainlog_database_list_body_metric_points(app->database,
            BODY_METRICS[app->body_metric_selected].metric,
            app->body_metric_points, MAX_BODY_METRIC_POINTS,
            &app->body_metric_count) != TRAINLOG_STATUS_OK;
    app->body_metric_partial = app->body_metric_count == MAX_BODY_METRIC_POINTS;
}

static void app_shell_load_body_global(TrainlogAppContext *app)
{
    size_t metric;
    app->body_global_date_count = 0U;
    app->body_snapshot_error = false;
    app->body_global_partial = false;
    if (!app->body_global_initialized) {
        for (metric = 0U; metric < 14U; ++metric)
            app->body_global_enabled[metric] = true;
        app->body_global_initialized = true;
    }
    (void)memset(app->body_global_series, 0,
        sizeof(app->body_global_series));
    (void)memset(app->body_global_dates, 0, sizeof(app->body_global_dates));
    for (metric = 0U; metric < 14U; ++metric) {
        TrainlogGlobalBodySeries *series = &app->body_global_series[metric];
        size_t point;
        series->symbol = GLOBAL_BODY_SYMBOLS[metric];
        series->role = GLOBAL_BODY_ROLES[metric];
        if (trainlog_database_list_body_metric_points(app->database,
                BODY_METRICS[metric].metric, series->points,
                MAX_BODY_METRIC_POINTS, &series->count) != TRAINLOG_STATUS_OK) {
            app->body_snapshot_error = true;
            series->count = 0U;
            continue;
        }
        if (series->count == MAX_BODY_METRIC_POINTS)
            app->body_global_partial = true;
        if (series->count == 0U) continue;
        series->baseline = series->points[0].value;
        (void)trainlog_body_percent_change(series->baseline,
            series->points[series->count - 1U].value,
            &series->latest_percent);
        for (point = 0U; point < series->count; ++point)
            if (!global_date_add(app->body_global_dates,
                    &app->body_global_date_count,
                    series->points[point].observed_at))
                app->body_snapshot_error = true;
    }
    if (app->body_global_date_count > 0U)
        qsort(app->body_global_dates, app->body_global_date_count,
            sizeof(app->body_global_dates[0]), global_date_compare);
}

static void app_shell_load_body_analytics(TrainlogAppContext *app)
{
    size_t count = 0U;
    app->body_snapshot_error =
        trainlog_database_list_body_observations(app->database,
            app->body_records, MAX_BODY_OBSERVATIONS,
            &count) != TRAINLOG_STATUS_OK;
    app->loaded_count = count;
    app->body_has_profile = body_analytics_profile_load(&app->body_profile);
}

static void sync_controller_load_history(TrainlogSyncController *sync)
{
    char selected_id[TRAINLOG_ID_MAX + 1U] = "";
    size_t index;
    if (sync->selected < sync->history_count)
        (void)snprintf(selected_id, sizeof(selected_id), "%s",
            sync->history[sync->selected].sync_id);
    sync_history_load(sync->history, SYNC_HISTORY_CAPACITY,
        &sync->history_count);
    if (sync->history_count == 0U) sync->selected = 0U;
    else if (selected_id[0] != '\0') {
        for (index = 0U; index < sync->history_count; ++index)
            if (strcmp(sync->history[index].sync_id, selected_id) == 0) {
                sync->selected = index;
                break;
            }
        if (index == sync->history_count && sync->selected >= sync->history_count)
            sync->selected = sync->history_count - 1U;
    } else if (sync->selected >= sync->history_count)
        sync->selected = sync->history_count - 1U;
}

static void sync_controller_open_detail(TrainlogSyncController *sync)
{
    const TrainlogSyncHistoryEntry *entry;
    char path[PATH_MAX + 1U];
    FILE *file;
    size_t used;
    char *cursor;
    sync->detail_line_count = 0U;
    sync->detail_scroll = 0U;
    sync->showing_detail = false;
    if (sync->history_count == 0U || sync->selected >= sync->history_count)
        return;
    entry = &sync->history[sync->selected];
    if (entry->sync_id[0] == '\0') return;
    /* CONTRACT: known legacy one-way records remain readable internal history,
     * but their stored diagnostic prose predates the unified PC↔Android TUI and
     * may expose prohibited directional modes. Present only parsed neutral
     * metadata; never rewrite or reinterpret the retained artifact. */
    if (entry->direction_known &&
        entry->direction != TRAINLOG_SYNC_BIDIRECTIONAL) {
        (void)snprintf(sync->detail_text, sizeof(sync->detail_text),
            "Synchronisation historique PC↔Android\n"
            "Horodatage : %s\n"
            "État : %s\n"
            "Détail hérité masqué (entrée historique)",
            entry->timestamp, entry->success ? "réussie" : "échouée");
        sync->detail_lines[sync->detail_line_count++] = sync->detail_text;
        cursor = sync->detail_text;
        while (*cursor != '\0' &&
               sync->detail_line_count < SYNC_DETAIL_LINE_CAPACITY) {
            if (*cursor == '\n') {
                *cursor = '\0';
                if (cursor[1] != '\0')
                    sync->detail_lines[sync->detail_line_count++] = cursor + 1;
            }
            ++cursor;
        }
        sync->showing_detail = true;
        return;
    }
    if (!sync_runs_path(entry->sync_id,
            path, sizeof(path))) return;
    file = fopen(path, "rb");
    if (file == NULL) return;
    used = fread(sync->detail_text, 1U,
        sizeof(sync->detail_text) - 1U, file);
    sync->detail_text[used] = '\0';
    (void)fclose(file);
    if (used == 0U) return;
    cursor = sync->detail_text;
    sync->detail_lines[sync->detail_line_count++] = cursor;
    while (*cursor != '\0' &&
           sync->detail_line_count < SYNC_DETAIL_LINE_CAPACITY) {
        if (*cursor == '\n') {
            *cursor = '\0';
            if (cursor[1] != '\0')
                sync->detail_lines[sync->detail_line_count++] = cursor + 1;
        }
        ++cursor;
    }
    sync->showing_detail = true;
}

static const char *sync_controller_status_error(TrainlogStatus status)
{
    switch (status) {
    case TRAINLOG_STATUS_CONFLICT:
        return "Synchronisation refusée : une autre exécution est active.";
    case TRAINLOG_STATUS_NOT_FOUND:
        return "Synchronisation impossible : aucun appareil MTP Trainlog détecté.";
    case TRAINLOG_STATUS_INVALID_ARGUMENT:
        return "Synchronisation refusée : données ou demande invalides.";
    case TRAINLOG_STATUS_DATABASE_ERROR:
        return "Synchronisation interrompue : erreur de base de données locale.";
    case TRAINLOG_STATUS_SCHEMA_UNSUPPORTED:
        return "Synchronisation refusée : version de données non prise en charge.";
    case TRAINLOG_STATUS_SYSTEM_ERROR:
        return "Synchronisation interrompue : erreur d’accès appareil ou fichier.";
    case TRAINLOG_STATUS_OK:
    default:
        return "Synchronisation interrompue sans rapport de réussite.";
    }
}

static void app_shell_open_exercise_analytics(TrainlogAppContext *app,
                                              TrainlogAppRoute route)
{
    app->performance_count = 0U;
    app->performance_error = trainlog_database_list_exercise_performance(
        app->database, app->exercise_detail.exercise_id,
        app->performance_points, MAX_SESSIONS, &app->performance_count) !=
        TRAINLOG_STATUS_OK;
    if (!app->performance_error && route == TRAINLOG_ROUTE_EXERCISE_MAX)
        app->performance_error = trainlog_measured_max_summarize(
            app->performance_points, app->performance_count,
            &app->max_summary) != TRAINLOG_STATUS_OK;
    (void)trainlog_navigation_open(&app->navigation, route,
        app->exercise_detail.exercise_id);
    app->content_scroll = 0U;
}

static void equipment_controller_start(TrainlogAppContext *app)
{
    TrainlogEquipmentController *controller = &app->equipment_controller;
    (void)memset(controller, 0, sizeof(*controller));
    controller->phase = TRAINLOG_EQUIPMENT_NAME;
    controller->return_phase = TRAINLOG_EQUIPMENT_NAME;
    controller->load_choice = 2;
    trainlog_form_init(&controller->form, "");
    app->focus = TRAINLOG_FOCUS_EDITOR;
}

static void equipment_controller_open_field(TrainlogEquipmentController *controller,
                                            TrainlogEquipmentPhase phase,
                                            const char *value)
{
    controller->phase = phase;
    controller->return_phase = phase;
    controller->message[0] = '\0';
    trainlog_form_init(&controller->form, value);
}

static bool equipment_controller_save(TrainlogAppContext *app)
{
    TrainlogEquipmentController *controller = &app->equipment_controller;
    TrainlogStatus status;
    generate_custom_equipment_uuid(controller->pending.equipment_id);
    (void)snprintf(controller->pending.load_semantics,
        sizeof(controller->pending.load_semantics), "%s",
        controller->load_choice == 1 ? "none" :
        controller->load_choice == 2 ? "external" : "assistance");
    status = trainlog_database_create_custom_equipment(app->database,
        &controller->pending);
    if (status != TRAINLOG_STATUS_OK) {
        (void)snprintf(controller->message, sizeof(controller->message), "%s",
            status == TRAINLOG_STATUS_CONFLICT
                ? "Conflit de définition; les champs sont conservés."
                : "Équipement non créé : définition invalide ou conflit.");
        controller->phase = TRAINLOG_EQUIPMENT_LOAD;
        return false;
    }
    controller->dirty = false;
    controller->form.active = false;
    controller->phase = TRAINLOG_EQUIPMENT_MESSAGE;
    (void)snprintf(controller->message, sizeof(controller->message),
        "Équipement personnel créé : %.120s", controller->pending.display_name);
    app_shell_refresh_list(app);
    (void)snprintf(app->list.selected_id, sizeof(app->list.selected_id), "%s",
        controller->pending.equipment_id);
    trainlog_list_set_items(&app->list, app->stable_ids, app->loaded_count,
        app->list.visible_rows, app->list.total_known, app->list.more_available);
    return true;
}

static bool app_shell_dispatch_equipment_controller(TrainlogAppContext *app,
                                                     int key)
{
    TrainlogEquipmentController *controller = &app->equipment_controller;
    TrainlogFormResult result;
    if (controller->phase == TRAINLOG_EQUIPMENT_IDLE) return false;
    if (controller->phase == TRAINLOG_EQUIPMENT_MESSAGE) {
        if (key == TRAINLOG_KEY_ENTER || key == '\n' ||
            key == TRAINLOG_KEY_ESCAPE) {
            controller->phase = TRAINLOG_EQUIPMENT_IDLE;
            controller->message[0] = '\0';
            app->focus = TRAINLOG_FOCUS_CONTENT;
        }
        return true;
    }
    if (controller->phase == TRAINLOG_EQUIPMENT_CONFIRM_DISCARD) {
        if (key == '1') {
            bool leaving = controller->leaving;
            TrainlogAppRoute pending_route = controller->pending_route;
            (void)memset(controller, 0, sizeof(*controller));
            app->focus = TRAINLOG_FOCUS_CONTENT;
            if (leaving) app_shell_open_route(app, pending_route);
        } else if (key == '0' || key == TRAINLOG_KEY_ESCAPE)
            controller->phase = controller->return_phase;
        return true;
    }
    if (controller->phase == TRAINLOG_EQUIPMENT_LOAD) {
        if (key == TRAINLOG_KEY_UP || key == TRAINLOG_KEY_LEFT)
            controller->load_choice = controller->load_choice > 1
                ? controller->load_choice - 1 : 3;
        else if (key == TRAINLOG_KEY_DOWN || key == TRAINLOG_KEY_RIGHT)
            controller->load_choice = controller->load_choice < 3
                ? controller->load_choice + 1 : 1;
        else if (key >= '1' && key <= '3')
            controller->load_choice = key - '0';
        else if (key == TRAINLOG_KEY_ENTER || key == '\n')
            (void)equipment_controller_save(app);
        else if (key == TRAINLOG_KEY_ESCAPE) {
            controller->leaving = false;
            controller->return_phase = TRAINLOG_EQUIPMENT_LOAD;
            controller->phase = TRAINLOG_EQUIPMENT_CONFIRM_DISCARD;
        } else if (key == TRAINLOG_KEY_F6)
            (void)trainlog_overlays_push(&app->overlays,
                TRAINLOG_OVERLAY_NAVIGATION, app->focus,
                app->navigation.current.stable_id);
        else if (key == TRAINLOG_KEY_F7)
            (void)trainlog_overlays_push(&app->overlays,
                TRAINLOG_OVERLAY_ACTIONS, app->focus,
                app->navigation.current.stable_id);
        return true;
    }
    result = trainlog_form_handle(&controller->form, key);
    if (result == TRAINLOG_FORM_OPEN_NAVIGATION) {
        (void)trainlog_overlays_push(&app->overlays,
            TRAINLOG_OVERLAY_NAVIGATION, app->focus,
            app->navigation.current.stable_id);
        return true;
    }
    if (result == TRAINLOG_FORM_OPEN_ACTIONS) {
        (void)trainlog_overlays_push(&app->overlays,
            TRAINLOG_OVERLAY_ACTIONS, app->focus,
            app->navigation.current.stable_id);
        return true;
    }
    if (result == TRAINLOG_FORM_CANCEL) {
        controller->leaving = false;
        controller->return_phase = controller->phase;
        controller->phase = TRAINLOG_EQUIPMENT_CONFIRM_DISCARD;
        return true;
    }
    if (result == TRAINLOG_FORM_EDITED) {
        controller->dirty = true;
        controller->message[0] = '\0';
        return true;
    }
    if (result != TRAINLOG_FORM_SUBMIT && result != TRAINLOG_FORM_NEXT)
        return true;
    if ((controller->phase == TRAINLOG_EQUIPMENT_NAME ||
         controller->phase == TRAINLOG_EQUIPMENT_TYPE) &&
        controller->form.text[0] == '\0') {
        (void)snprintf(controller->message, sizeof(controller->message),
            "Ce champ est obligatoire; la saisie est conservée.");
        return true;
    }
    if (controller->phase == TRAINLOG_EQUIPMENT_NAME) {
        (void)snprintf(controller->pending.display_name,
            sizeof(controller->pending.display_name), "%s", controller->form.text);
        equipment_controller_open_field(controller, TRAINLOG_EQUIPMENT_LABEL,
            controller->pending.label_name);
    } else if (controller->phase == TRAINLOG_EQUIPMENT_LABEL) {
        (void)snprintf(controller->pending.label_name,
            sizeof(controller->pending.label_name), "%s", controller->form.text);
        equipment_controller_open_field(controller, TRAINLOG_EQUIPMENT_TYPE,
            controller->pending.equipment_type);
    } else {
        (void)snprintf(controller->pending.equipment_type,
            sizeof(controller->pending.equipment_type), "%s", controller->form.text);
        controller->form.active = false;
        controller->phase = TRAINLOG_EQUIPMENT_LOAD;
    }
    return true;
}

static void exercise_controller_start_create(TrainlogAppContext *app,
                                             bool inline_session)
{
    TrainlogExerciseController *controller = &app->exercise_controller;
    (void)memset(controller, 0, sizeof(*controller));
    controller->phase = TRAINLOG_EXERCISE_NAME;
    controller->return_phase = TRAINLOG_EXERCISE_NAME;
    controller->pending.tracking_mode = TRAINLOG_TRACKING_REPS;
    controller->pending.recording_mode = TRAINLOG_RECORDING_SETS;
    controller->inline_session = inline_session;
    trainlog_form_init(&controller->form, "");
    app->focus = TRAINLOG_FOCUS_EDITOR;
}

static void exercise_controller_start_edit(TrainlogAppContext *app)
{
    TrainlogExerciseController *controller = &app->exercise_controller;
    (void)memset(controller, 0, sizeof(*controller));
    controller->pending = app->exercise_detail;
    controller->editing = true;
    controller->phase = TRAINLOG_EXERCISE_NAME;
    controller->return_phase = TRAINLOG_EXERCISE_NAME;
    if (!load_exercise_body_zones(app->database,
            controller->pending.exercise_id, controller->primary_zone,
            controller->secondary_zones, &controller->secondary_count)) {
        controller->phase = TRAINLOG_EXERCISE_MESSAGE;
        (void)snprintf(controller->message, sizeof(controller->message),
            "Impossible de lire les zones actuelles.");
    } else trainlog_form_init(&controller->form, controller->pending.name);
    app->focus = TRAINLOG_FOCUS_EDITOR;
}

static bool exercise_merge_matches(const TrainlogExercise *exercise,
                                   const char *query)
{
    char normalized_name[(TRAINLOG_NAME_MAX * 4U) + 1U];
    char normalized_query[(TRAINLOG_NAME_MAX * 4U) + 1U];
    if (query == NULL || query[0] == '\0') return true;
    if (trainlog_catalog_normalize_name(exercise->name, normalized_name,
            sizeof(normalized_name)) != TRAINLOG_STATUS_OK ||
        trainlog_catalog_normalize_name(query, normalized_query,
            sizeof(normalized_query)) != TRAINLOG_STATUS_OK) return false;
    return strstr(normalized_name, normalized_query) != NULL;
}

static size_t exercise_merge_match_count(const TrainlogExerciseController *controller)
{
    size_t index, count = 0U;
    for (index = 0U; index < controller->merge_count; ++index)
        if (exercise_merge_matches(&controller->merge_candidates[index],
                controller->form.text)) ++count;
    return count;
}

static TrainlogExercise *exercise_merge_match_at(TrainlogExerciseController *controller,
                                                  size_t selected)
{
    size_t index, match = 0U;
    for (index = 0U; index < controller->merge_count; ++index) {
        if (!exercise_merge_matches(&controller->merge_candidates[index],
                controller->form.text)) continue;
        if (match == selected) return &controller->merge_candidates[index];
        ++match;
    }
    return NULL;
}

static bool exercise_merge_candidate_is_compatible(
    TrainlogDatabase *database,
    const TrainlogExercise *source,
    const TrainlogExercise *target
)
{
    TrainlogExerciseBodyZone source_zones[MAX_BODY_ZONES];
    TrainlogExerciseBodyZone target_zones[MAX_BODY_ZONES];
    size_t source_count = 0U, target_count = 0U, index;
    const char *source_primary = NULL, *target_primary = NULL;
    if (source->tracking_mode != target->tracking_mode ||
        source->recording_mode != target->recording_mode ||
        source->data_fields != target->data_fields) return false;
    if (trainlog_database_list_exercise_body_zones(database,
            source->exercise_id, source_zones, MAX_BODY_ZONES,
            &source_count) != TRAINLOG_STATUS_OK ||
        trainlog_database_list_exercise_body_zones(database,
            target->exercise_id, target_zones, MAX_BODY_ZONES,
            &target_count) != TRAINLOG_STATUS_OK) return false;
    for (index = 0U; index < source_count; ++index)
        if (source_zones[index].role == TRAINLOG_BODY_ZONE_PRIMARY)
            source_primary = source_zones[index].zone_id;
    for (index = 0U; index < target_count; ++index)
        if (target_zones[index].role == TRAINLOG_BODY_ZONE_PRIMARY)
            target_primary = target_zones[index].zone_id;
    return source_primary == NULL || target_primary == NULL ||
        strcmp(source_primary, target_primary) == 0;
}

static void exercise_controller_start_merge(TrainlogAppContext *app)
{
    TrainlogExerciseController *controller = &app->exercise_controller;
    TrainlogExercise all[MAX_EXERCISES];
    size_t index, count = 0U;
    TrainlogStatus status = trainlog_database_list_exercises(app->database, all,
        MAX_EXERCISES, &count);
    (void)memset(controller, 0, sizeof(*controller));
    controller->merge_source = app->exercise_detail;
    if (status != TRAINLOG_STATUS_OK) {
        controller->phase = TRAINLOG_EXERCISE_MERGE_RESULT;
        (void)snprintf(controller->message, sizeof(controller->message),
            "Impossible de charger les cibles de fusion.");
    } else {
        for (index = 0U; index < count; ++index)
            if (strcmp(all[index].exercise_id,
                    controller->merge_source.exercise_id) != 0 &&
                exercise_merge_candidate_is_compatible(app->database,
                    &controller->merge_source, &all[index]))
                controller->merge_candidates[controller->merge_count++] = all[index];
        controller->phase = TRAINLOG_EXERCISE_MERGE_PICKER;
        trainlog_form_init(&controller->form, "");
    }
    (void)trainlog_overlays_push(&app->overlays, TRAINLOG_OVERLAY_EXERCISE_MERGE,
        TRAINLOG_FOCUS_CONTENT, controller->merge_source.exercise_id);
    app->focus = TRAINLOG_FOCUS_OVERLAY;
}

static bool exercise_controller_save(TrainlogAppContext *app)
{
    TrainlogExerciseController *controller = &app->exercise_controller;
    const char *secondary_ids[MAX_BODY_ZONES];
    TrainlogExercise created;
    TrainlogStatus status;
    char normalized[(TRAINLOG_NAME_MAX * 4U) + 1U];
    size_t index;
    if (!controller->editing &&
        controller->pending.recording_mode == TRAINLOG_RECORDING_SETS &&
        controller->primary_zone[0] == '\0') {
        (void)snprintf(controller->message, sizeof(controller->message),
            "Une zone principale est requise pour un exercice par séries.");
        return false;
    }
    for (index = 0U; index < controller->secondary_count; ++index)
        secondary_ids[index] = controller->secondary_zones[index];
    if (controller->editing) {
        status = trainlog_catalog_normalize_name(controller->pending.name,
            normalized, sizeof(normalized));
        if (status == TRAINLOG_STATUS_OK)
            status = trainlog_database_update_exercise_profiled(app->database,
                controller->pending.exercise_id, controller->pending.name,
                normalized, controller->pending.tracking_mode,
                controller->pending.recording_mode,
                controller->pending.data_fields,
                controller->primary_zone[0] != '\0'
                    ? controller->primary_zone : NULL,
                secondary_ids, controller->secondary_count);
        created = controller->pending;
    } else status = trainlog_catalog_create_exercise_profiled_with_zones(
        app->database, controller->pending.name,
        controller->pending.tracking_mode,
        controller->pending.recording_mode,
        controller->pending.data_fields,
        controller->primary_zone[0] != '\0' ? controller->primary_zone : NULL,
        secondary_ids, controller->secondary_count, &created);
    if (status != TRAINLOG_STATUS_OK) {
        (void)snprintf(controller->message, sizeof(controller->message), "%s",
            status == TRAINLOG_STATUS_CONFLICT
                ? "Conflit de nom ou de profil; tous les champs sont conservés."
                : "Exercice non enregistré; tous les champs sont conservés.");
        return false;
    }
    controller->dirty = false;
    controller->form.active = false;
    controller->pending = created;
    if (controller->inline_session) {
        TrainlogSessionController *session = &app->session;
        if (!session_controller_load_picker(app)) {
            controller->phase = TRAINLOG_EXERCISE_MESSAGE;
            (void)snprintf(controller->message, sizeof(controller->message),
                "Exercice créé, mais le sélecteur n’a pas pu être rechargé.");
            return true;
        }
        for (index = 0U; index < session->picker_count; ++index)
            if (strcmp(session->picker[index].exercise_id,
                    created.exercise_id) == 0) {
                session->picker_selected = index;
                break;
            }
        (void)memset(controller, 0, sizeof(*controller));
        app->focus = TRAINLOG_FOCUS_CONTENT;
        return true;
    }
    if (controller->editing) {
        app->exercise_detail = created;
        app_shell_load_exercise_detail_metadata(app);
    }
    else {
        app_shell_refresh_list(app);
        (void)snprintf(app->list.selected_id, sizeof(app->list.selected_id),
            "%s", created.exercise_id);
        trainlog_list_set_items(&app->list, app->stable_ids, app->loaded_count,
            app->list.visible_rows, app->list.total_known,
            app->list.more_available);
    }
    controller->phase = TRAINLOG_EXERCISE_MESSAGE;
    (void)snprintf(controller->message, sizeof(controller->message),
        "Exercice %s : %.120s", controller->editing ? "modifié" : "créé",
        created.name);
    return true;
}

static bool app_shell_dispatch_exercise_controller(TrainlogAppContext *app,
                                                    int key)
{
    TrainlogExerciseController *controller = &app->exercise_controller;
    TrainlogFormResult result;
    size_t zone_count = trainlog_body_zone_catalog_count();
    if (controller->phase == TRAINLOG_EXERCISE_IDLE) return false;
    if (controller->phase == TRAINLOG_EXERCISE_MESSAGE) {
        if (key == TRAINLOG_KEY_ENTER || key == '\n' ||
            key == TRAINLOG_KEY_ESCAPE) {
            (void)memset(controller, 0, sizeof(*controller));
            app->focus = TRAINLOG_FOCUS_CONTENT;
        }
        return true;
    }
    if (controller->phase == TRAINLOG_EXERCISE_CONFIRM_DISCARD) {
        if (key == '1') {
            TrainlogSessionController *session = &app->session;
            bool inline_session = controller->inline_session;
            bool leaving = controller->leaving;
            TrainlogAppRoute pending_route = controller->pending_route;
            (void)memset(controller, 0, sizeof(*controller));
            if (inline_session && !leaving)
                session->phase = TRAINLOG_SESSION_EXERCISE_PICKER;
            app->focus = TRAINLOG_FOCUS_CONTENT;
            if (leaving) app_shell_open_route(app, pending_route);
        } else if (key == '0' || key == TRAINLOG_KEY_ESCAPE)
            controller->phase = controller->return_phase;
        return true;
    }
    if (controller->phase == TRAINLOG_EXERCISE_ZONES) {
        const TrainlogBodyZone *zone = zone_count > 0U
            ? trainlog_body_zone_catalog_at(controller->zone_selected) : NULL;
        size_t secondary_index;
        if (key == TRAINLOG_KEY_UP && controller->zone_selected > 0U)
            --controller->zone_selected;
        else if (key == TRAINLOG_KEY_DOWN &&
                 controller->zone_selected + 1U < zone_count)
            ++controller->zone_selected;
        else if ((key == 'n' || key == 'N')) {
            controller->primary_zone[0] = '\0';
            controller->secondary_count = 0U;
            controller->dirty = true;
        } else if ((key == 'p' || key == 'P') && zone != NULL && !zone->is_group) {
            (void)snprintf(controller->primary_zone,
                sizeof(controller->primary_zone), "%s", zone->zone_id);
            if (secondary_zone_contains(controller->secondary_zones,
                    controller->secondary_count, zone->zone_id,
                    &secondary_index)) {
                for (size_t move = secondary_index;
                     move + 1U < controller->secondary_count; ++move)
                    (void)memcpy(controller->secondary_zones[move],
                        controller->secondary_zones[move + 1U],
                        sizeof(controller->secondary_zones[move]));
                --controller->secondary_count;
            }
            controller->dirty = true;
        } else if (key == ' ' && zone != NULL && !zone->is_group &&
                   controller->primary_zone[0] != '\0' &&
                   strcmp(controller->primary_zone, zone->zone_id) != 0) {
            if (secondary_zone_contains(controller->secondary_zones,
                    controller->secondary_count, zone->zone_id,
                    &secondary_index)) {
                for (size_t move = secondary_index;
                     move + 1U < controller->secondary_count; ++move)
                    (void)memcpy(controller->secondary_zones[move],
                        controller->secondary_zones[move + 1U],
                        sizeof(controller->secondary_zones[move]));
                --controller->secondary_count;
            } else if (controller->secondary_count < MAX_BODY_ZONES) {
                (void)snprintf(controller->secondary_zones[
                    controller->secondary_count], TRAINLOG_ZONE_ID_MAX + 1U,
                    "%s", zone->zone_id);
                ++controller->secondary_count;
            }
            controller->dirty = true;
        } else if (key == TRAINLOG_KEY_ENTER || key == '\n')
            (void)exercise_controller_save(app);
        else if (key == TRAINLOG_KEY_ESCAPE) {
            controller->leaving = false;
            controller->return_phase = TRAINLOG_EXERCISE_ZONES;
            controller->phase = TRAINLOG_EXERCISE_CONFIRM_DISCARD;
        } else if (key == TRAINLOG_KEY_F6)
            (void)trainlog_overlays_push(&app->overlays,
                TRAINLOG_OVERLAY_NAVIGATION, app->focus,
                app->navigation.current.stable_id);
        else if (key == TRAINLOG_KEY_F7)
            (void)trainlog_overlays_push(&app->overlays,
                TRAINLOG_OVERLAY_ACTIONS, app->focus,
                app->navigation.current.stable_id);
        return true;
    }
    if (controller->phase == TRAINLOG_EXERCISE_TRACKING ||
        controller->phase == TRAINLOG_EXERCISE_RECORDING ||
        controller->phase == TRAINLOG_EXERCISE_SPEED ||
        controller->phase == TRAINLOG_EXERCISE_DISTANCE) {
        bool directional = key == TRAINLOG_KEY_UP || key == TRAINLOG_KEY_DOWN ||
            key == TRAINLOG_KEY_LEFT || key == TRAINLOG_KEY_RIGHT;
        if (directional || key == '0' || key == '1' || key == '2') {
            if (controller->phase == TRAINLOG_EXERCISE_TRACKING) {
                if (key == '1') controller->pending.tracking_mode = TRAINLOG_TRACKING_REPS;
                else if (key == '2') controller->pending.tracking_mode = TRAINLOG_TRACKING_DURATION;
                else controller->pending.tracking_mode =
                    controller->pending.tracking_mode == TRAINLOG_TRACKING_REPS
                        ? TRAINLOG_TRACKING_DURATION : TRAINLOG_TRACKING_REPS;
            } else if (controller->phase == TRAINLOG_EXERCISE_RECORDING) {
                if (key == '1') controller->pending.recording_mode = TRAINLOG_RECORDING_SETS;
                else if (key == '2') controller->pending.recording_mode = TRAINLOG_RECORDING_CONTINUOUS;
                else controller->pending.recording_mode =
                    controller->pending.recording_mode == TRAINLOG_RECORDING_SETS
                        ? TRAINLOG_RECORDING_CONTINUOUS : TRAINLOG_RECORDING_SETS;
            } else if (controller->phase == TRAINLOG_EXERCISE_SPEED) {
                if (key == '0') controller->pending.data_fields &=
                    ~TRAINLOG_EXERCISE_DATA_SPEED_KMH;
                else if (key == '1') controller->pending.data_fields |=
                    TRAINLOG_EXERCISE_DATA_SPEED_KMH;
                else controller->pending.data_fields ^=
                    TRAINLOG_EXERCISE_DATA_SPEED_KMH;
            } else {
                if (key == '0') controller->pending.data_fields &=
                    ~TRAINLOG_EXERCISE_DATA_DISTANCE_KM;
                else if (key == '1') controller->pending.data_fields |=
                    TRAINLOG_EXERCISE_DATA_DISTANCE_KM;
                else controller->pending.data_fields ^=
                    TRAINLOG_EXERCISE_DATA_DISTANCE_KM;
            }
            controller->dirty = true;
        } else if (key == TRAINLOG_KEY_ENTER || key == '\n') {
            if (controller->phase == TRAINLOG_EXERCISE_TRACKING)
                controller->phase = TRAINLOG_EXERCISE_RECORDING;
            else if (controller->phase == TRAINLOG_EXERCISE_RECORDING) {
                if (controller->pending.recording_mode == TRAINLOG_RECORDING_CONTINUOUS) {
                    controller->pending.tracking_mode = TRAINLOG_TRACKING_DURATION;
                    controller->phase = TRAINLOG_EXERCISE_SPEED;
                } else {
                    controller->pending.data_fields = 0U;
                    controller->phase = TRAINLOG_EXERCISE_ZONES;
                }
            } else if (controller->phase == TRAINLOG_EXERCISE_SPEED)
                controller->phase = TRAINLOG_EXERCISE_DISTANCE;
            else controller->phase = TRAINLOG_EXERCISE_ZONES;
        } else if (key == TRAINLOG_KEY_ESCAPE) {
            controller->return_phase = controller->phase;
            controller->phase = TRAINLOG_EXERCISE_CONFIRM_DISCARD;
        } else if (key == TRAINLOG_KEY_F6)
            (void)trainlog_overlays_push(&app->overlays,
                TRAINLOG_OVERLAY_NAVIGATION, app->focus,
                app->navigation.current.stable_id);
        else if (key == TRAINLOG_KEY_F7)
            (void)trainlog_overlays_push(&app->overlays,
                TRAINLOG_OVERLAY_ACTIONS, app->focus,
                app->navigation.current.stable_id);
        return true;
    }
    result = trainlog_form_handle(&controller->form, key);
    if (result == TRAINLOG_FORM_OPEN_NAVIGATION ||
        result == TRAINLOG_FORM_OPEN_ACTIONS) {
        (void)trainlog_overlays_push(&app->overlays,
            result == TRAINLOG_FORM_OPEN_NAVIGATION
                ? TRAINLOG_OVERLAY_NAVIGATION : TRAINLOG_OVERLAY_ACTIONS,
            app->focus, app->navigation.current.stable_id);
    } else if (result == TRAINLOG_FORM_CANCEL) {
        controller->leaving = false;
        controller->return_phase = TRAINLOG_EXERCISE_NAME;
        controller->phase = TRAINLOG_EXERCISE_CONFIRM_DISCARD;
    } else if (result == TRAINLOG_FORM_EDITED) {
        controller->dirty = true;
        controller->message[0] = '\0';
    } else if (result == TRAINLOG_FORM_SUBMIT || result == TRAINLOG_FORM_NEXT) {
        if (controller->form.text[0] == '\0')
            (void)snprintf(controller->message, sizeof(controller->message),
                "Le nom est obligatoire; la saisie est conservée.");
        else {
            (void)snprintf(controller->pending.name,
                sizeof(controller->pending.name), "%s", controller->form.text);
            controller->form.active = false;
            controller->phase = controller->editing
                ? TRAINLOG_EXERCISE_ZONES : TRAINLOG_EXERCISE_TRACKING;
        }
    }
    return true;
}

static void app_shell_release_session_detail(TrainlogAppContext *app)
{
    trainlog_database_free_session_details(app->session_entries,
        app->session_entry_count);
    (void)memset(app->session_entries, 0, sizeof(app->session_entries));
    app->session_entry_count = 0U;
}

static void app_shell_open_session_detail(TrainlogAppContext *app,
                                          const char *session_id)
{
    app_shell_release_session_detail(app);
    app->session_detail_error = trainlog_database_get_session_details(app->database,
        session_id, &app->session_detail, app->session_entries,
        MAX_SESSION_EXERCISES, &app->session_entry_count) != TRAINLOG_STATUS_OK;
    app->session_entry_selected = 0U;
    app->session_set_scroll = 0U;
    (void)trainlog_navigation_open(&app->navigation, TRAINLOG_ROUTE_SESSION_DETAIL,
        session_id);
}

static void app_shell_actions(TrainlogAppContext *app)
{
    TrainlogAppRoute route = app->navigation.current.route;
    TrainlogSessionController *session = &app->session;
    trainlog_actions_clear(&app->actions);
    if (app->body_controller.phase != TRAINLOG_BODY_IDLE) {
        TrainlogBodyPhase phase = app->body_controller.phase;
        if (phase == TRAINLOG_BODY_CONFIRM_DISCARD) {
            app_shell_add_action(app, "body.form.discard", '1',
                "1 Abandonner", 1U, TRAINLOG_INTENT_DISCARD, route);
            app_shell_add_action(app, "body.form.resume", '0',
                "0 Reprendre", 2U, TRAINLOG_INTENT_BACK, route);
        } else if (phase == TRAINLOG_BODY_MESSAGE)
            app_shell_add_action(app, "body.form.close", TRAINLOG_KEY_ENTER,
                "Entrée Retour", 1U, TRAINLOG_INTENT_BACK, route);
        else {
            app_shell_add_action(app, "body.form.next", TRAINLOG_KEY_ENTER,
                app->body_controller.field == 13U ? "Entrée Enregistrer"
                                                  : "Entrée Champ suivant",
                1U, TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "body.form.cancel", TRAINLOG_KEY_ESCAPE,
                "Échap Abandonner…", 2U, TRAINLOG_INTENT_DISCARD, route);
        }
        app_shell_add_action(app, "navigation", TRAINLOG_KEY_F6,
            "F6 Navigation", 80U, TRAINLOG_INTENT_OPEN_NAVIGATION, route);
        app_shell_add_action(app, "actions", TRAINLOG_KEY_F7,
            "F7 Actions", 81U, TRAINLOG_INTENT_OPEN_ACTIONS, route);
        app_shell_add_action(app, "help", '?', "? Aide", 82U,
            TRAINLOG_INTENT_OPEN_HELP, route);
        return;
    }
    if (app->profile_controller.phase != TRAINLOG_PROFILE_IDLE) {
        TrainlogProfilePhase phase = app->profile_controller.phase;
        if (phase == TRAINLOG_PROFILE_CONFIRM_DISCARD) {
            app_shell_add_action(app, "profile.discard", '1', "1 Abandonner",
                1U, TRAINLOG_INTENT_DISCARD, route);
            app_shell_add_action(app, "profile.resume", '0', "0 Reprendre",
                2U, TRAINLOG_INTENT_BACK, route);
        } else if (phase == TRAINLOG_PROFILE_MESSAGE)
            app_shell_add_action(app, "profile.close", TRAINLOG_KEY_ENTER,
                "Entrée Retour", 1U, TRAINLOG_INTENT_BACK, route);
        else {
            app_shell_add_action(app, "profile.next", TRAINLOG_KEY_ENTER,
                phase == TRAINLOG_PROFILE_HEIGHT ? "Entrée Enregistrer"
                                                 : "Entrée Continuer",
                1U, TRAINLOG_INTENT_SAVE, route);
            app_shell_add_action(app, "profile.cancel", TRAINLOG_KEY_ESCAPE,
                "Échap Abandonner…", 2U, TRAINLOG_INTENT_DISCARD, route);
        }
        app_shell_add_action(app, "navigation", TRAINLOG_KEY_F6,
            "F6 Navigation", 80U, TRAINLOG_INTENT_OPEN_NAVIGATION, route);
        app_shell_add_action(app, "actions", TRAINLOG_KEY_F7,
            "F7 Actions", 81U, TRAINLOG_INTENT_OPEN_ACTIONS, route);
        app_shell_add_action(app, "help", '?', "? Aide", 82U,
            TRAINLOG_INTENT_OPEN_HELP, route);
        return;
    }
    if (app->exercise_controller.phase != TRAINLOG_EXERCISE_IDLE) {
        TrainlogExercisePhase phase = app->exercise_controller.phase;
        if (phase == TRAINLOG_EXERCISE_CONFIRM_DISCARD) {
            app_shell_add_action(app, "exercise.form.discard", '1',
                "1 Abandonner", 1U, TRAINLOG_INTENT_DISCARD, route);
            app_shell_add_action(app, "exercise.form.resume", '0',
                "0 Reprendre", 2U, TRAINLOG_INTENT_BACK, route);
        } else if (phase == TRAINLOG_EXERCISE_MESSAGE)
            app_shell_add_action(app, "exercise.form.close", TRAINLOG_KEY_ENTER,
                "Entrée Retour", 1U, TRAINLOG_INTENT_BACK, route);
        else {
            app_shell_add_action(app, "exercise.form.next", TRAINLOG_KEY_ENTER,
                phase == TRAINLOG_EXERCISE_ZONES ? "Entrée Enregistrer"
                                                 : "Entrée Continuer",
                1U, phase == TRAINLOG_EXERCISE_ZONES ? TRAINLOG_INTENT_SAVE
                                                     : TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "exercise.form.cancel", TRAINLOG_KEY_ESCAPE,
                "Échap Abandonner…", 2U, TRAINLOG_INTENT_DISCARD, route);
        }
        app_shell_add_action(app, "navigation", TRAINLOG_KEY_F6,
            "F6 Navigation", 80U, TRAINLOG_INTENT_OPEN_NAVIGATION, route);
        app_shell_add_action(app, "actions", TRAINLOG_KEY_F7,
            "F7 Actions", 81U, TRAINLOG_INTENT_OPEN_ACTIONS, route);
        app_shell_add_action(app, "help", '?', "? Aide", 82U,
            TRAINLOG_INTENT_OPEN_HELP, route);
        return;
    }
    if (app->equipment_controller.phase != TRAINLOG_EQUIPMENT_IDLE) {
        TrainlogEquipmentPhase phase = app->equipment_controller.phase;
        if (phase == TRAINLOG_EQUIPMENT_CONFIRM_DISCARD) {
            app_shell_add_action(app, "equipment.create.discard", '1',
                "1 Abandonner", 1U, TRAINLOG_INTENT_DISCARD, route);
            app_shell_add_action(app, "equipment.create.resume", '0',
                "0 Reprendre", 2U, TRAINLOG_INTENT_BACK, route);
        } else if (phase == TRAINLOG_EQUIPMENT_MESSAGE)
            app_shell_add_action(app, "equipment.create.close", TRAINLOG_KEY_ENTER,
                "Entrée Catalogue", 1U, TRAINLOG_INTENT_BACK, route);
        else {
            app_shell_add_action(app, "equipment.create.next", TRAINLOG_KEY_ENTER,
                phase == TRAINLOG_EQUIPMENT_LOAD ? "Entrée Enregistrer"
                                                 : "Entrée Champ suivant",
                1U, phase == TRAINLOG_EQUIPMENT_LOAD ? TRAINLOG_INTENT_SAVE
                                                     : TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "equipment.create.cancel", TRAINLOG_KEY_ESCAPE,
                "Échap Abandonner…", 2U, TRAINLOG_INTENT_DISCARD, route);
        }
        app_shell_add_action(app, "navigation", TRAINLOG_KEY_F6,
            "F6 Navigation", 80U, TRAINLOG_INTENT_OPEN_NAVIGATION, route);
        app_shell_add_action(app, "actions", TRAINLOG_KEY_F7,
            "F7 Actions", 81U, TRAINLOG_INTENT_OPEN_ACTIONS, route);
        app_shell_add_action(app, "help", '?', "? Aide", 82U,
            TRAINLOG_INTENT_OPEN_HELP, route);
        return;
    }
    if (route == TRAINLOG_ROUTE_HOME)
        app_shell_add_action(app, "session", '1', "1 Séance", 1U,
            TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_SESSION_MANUAL);
    if (route == TRAINLOG_ROUTE_BODY ||
        route == TRAINLOG_ROUTE_SYNC || route == TRAINLOG_ROUTE_SETTINGS)
        app_shell_add_action(app, "open", TRAINLOG_KEY_ENTER, "Entrée Ouvrir", 1U,
            TRAINLOG_INTENT_PRIMARY, route);
    if ((route == TRAINLOG_ROUTE_SESSION_MANUAL ||
         route == TRAINLOG_ROUTE_SESSION_CURRENT) &&
        session->phase == TRAINLOG_SESSION_DRAFT) {
        app_shell_add_action(app, "session.occurrence.add", 'a', "a Ajouter", 1U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "session.occurrence.edit", 'e', "e Séries/résultat", 2U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "session.occurrence.replace", 'r', "r Remplacer", 3U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "session.occurrence.equipment", 'i', "i Équipement", 4U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "session.occurrence.plan", 'p', "p Objectif prévu", 5U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "session.occurrence.remove", 'd', "d Retirer", 6U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "session.finish", 'f', "f Enregistrer", 7U,
            TRAINLOG_INTENT_SAVE, route);
        app_shell_add_action(app, "session.abandon", 'q', "q Abandonner", 8U,
            TRAINLOG_INTENT_DISCARD, route);
    }
    if ((route == TRAINLOG_ROUTE_SESSION_MANUAL ||
         route == TRAINLOG_ROUTE_SESSION_CURRENT) &&
        session->phase == TRAINLOG_SESSION_EXERCISE_PICKER)
        app_shell_add_action(app, "session.exercise.create", 'n',
            "n Créer un exercice", 1U, TRAINLOG_INTENT_NONE, route);
    if ((route == TRAINLOG_ROUTE_SESSION_MANUAL ||
         route == TRAINLOG_ROUTE_SESSION_CURRENT) &&
        session->phase == TRAINLOG_SESSION_ACTUALS) {
        TrainlogSessionDraftExercise *draft = session->selected < session->draft_count
            ? &session->drafts[session->selected] : NULL;
        app_shell_add_action(app, "session.actual.edit", 'e', "e Modifier", 1U,
            TRAINLOG_INTENT_NONE, route);
        if (draft != NULL && draft->input.recording_mode == TRAINLOG_RECORDING_SETS &&
            !draft->input.has_max_weight) {
            app_shell_add_action(app, "session.set.add", 'a', "a Ajouter série", 2U,
                TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "session.set.remove", 'd', "d Supprimer série", 3U,
                TRAINLOG_INTENT_NONE, route);
        }
        if (draft != NULL && draft->input.recording_mode == TRAINLOG_RECORDING_CONTINUOUS &&
            (draft->input.data_fields & TRAINLOG_EXERCISE_DATA_SPEED_KMH) != 0U)
            app_shell_add_action(app, "session.continuous.speed", 's', "s Vitesse", 2U,
                TRAINLOG_INTENT_NONE, route);
        if (draft != NULL && draft->input.recording_mode == TRAINLOG_RECORDING_CONTINUOUS &&
            (draft->input.data_fields & TRAINLOG_EXERCISE_DATA_DISTANCE_KM) != 0U)
            app_shell_add_action(app, "session.continuous.distance", 'k', "k Distance", 3U,
                TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "session.actual.done", 'f', "f Terminer édition", 4U,
            TRAINLOG_INTENT_BACK, route);
    }
    if (route == TRAINLOG_ROUTE_SESSION_GENERATOR) {
        if (session->phase == TRAINLOG_SESSION_GENERATOR_CONFIG)
            app_shell_add_action(app, "generator.generate", 'g', "g Générer", 1U,
                TRAINLOG_INTENT_PRIMARY, route);
        if (session->phase == TRAINLOG_SESSION_GENERATOR_WARNING) {
            app_shell_add_action(app, "generator.warning.continue", 'c',
                "c Continuer", 1U, TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "generator.warning.zone", 'z',
                "z Autre zone", 2U, TRAINLOG_INTENT_NONE, route);
        }
        if (session->phase == TRAINLOG_SESSION_GENERATOR_PREVIEW) {
            app_shell_add_action(app, "generator.accept", 'a', "a Accepter", 1U,
                TRAINLOG_INTENT_PRIMARY, route);
            app_shell_add_action(app, "generator.remove", 'd', "d Retirer", 2U,
                TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "generator.edit.dose", 'e', "e Modifier dose", 3U,
                TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "generator.load.percent", '%', "% %MAX", 4U,
                TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "generator.load.auto", 'u', "u Charge auto", 5U,
                TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "generator.load.none", 'x', "x Sans charge", 6U,
                TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "generator.regenerate", 'g', "g Régénérer", 4U,
                TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "generator.move.up", '<', "< Monter", 5U,
                TRAINLOG_INTENT_NONE, route);
            app_shell_add_action(app, "generator.move.down", '>', "> Descendre", 6U,
                TRAINLOG_INTENT_NONE, route);
        }
        if (session->generated_preview)
            app_shell_add_action(app, "generator.discard", 'q',
                "q Abandonner la proposition", 6U, TRAINLOG_INTENT_DISCARD, route);
    }
    if (app_shell_is_list_route(route)) {
        app_shell_add_action(app, "list.open", TRAINLOG_KEY_ENTER, "Entrée Ouvrir", 1U,
            TRAINLOG_INTENT_PRIMARY, route);
        if (route != TRAINLOG_ROUTE_BODY)
            app_shell_add_action(app, "list.search", '/', "/ Rechercher", 2U,
                TRAINLOG_INTENT_OPEN_SEARCH, route);
    }
    if (route == TRAINLOG_ROUTE_BODY) {
        app_shell_add_action(app, "body.add", 'a', "a Ajouter", 1U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "body.edit", 'e', "e Modifier", 2U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "body.analytics", 'v', "v Analyse", 3U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "body.metric", 'm', "m Historique mesure", 4U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "body.graph", 'g', "g Vue 12 mois", 5U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "body.global", 'o', "o Vue globale", 6U,
            TRAINLOG_INTENT_NONE, route);
    }
    if (route == TRAINLOG_ROUTE_BODY_DETAIL)
        app_shell_add_action(app, "body.detail.edit", 'e', "e Modifier", 1U,
            TRAINLOG_INTENT_NONE, route);
    if (route == TRAINLOG_ROUTE_BODY_METRIC) {
        app_shell_add_action(app, "body.metric.previous", TRAINLOG_KEY_LEFT,
            "← Mesure précédente", 1U, TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "body.metric.next", TRAINLOG_KEY_RIGHT,
            "→ Mesure suivante", 2U, TRAINLOG_INTENT_NONE, route);
    }
    if (route == TRAINLOG_ROUTE_BODY_GLOBAL)
        app_shell_add_action(app, "body.global.toggle", ' ',
            "Espace Afficher/masquer", 1U, TRAINLOG_INTENT_NONE, route);
    if (route == TRAINLOG_ROUTE_BODY_ANALYTICS)
        app_shell_add_action(app, "body.profile", 'p', "p Profil estimation",
            1U, TRAINLOG_INTENT_NONE, route);
    if (route == TRAINLOG_ROUTE_SYNC) {
        /* WHY: one registry entry is the source of truth for both the footer
         * and F7 palette, preventing directional or duplicate launch affordances. */
        app_shell_add_action(app, "sync.now", 's',
            "s Synchroniser maintenant · PC↔Android", 1U,
            TRAINLOG_INTENT_NONE, route);
        app_shell_add_action(app, "sync.refresh", 'r', "r Actualiser appareil",
            2U, TRAINLOG_INTENT_REFRESH, route);
    }
    if (route == TRAINLOG_ROUTE_EQUIPMENT)
        app_shell_add_action(app, "equipment.create", 'n', "n Créer", 1U,
            TRAINLOG_INTENT_NONE, route);
    if (route == TRAINLOG_ROUTE_EXERCISE_DETAIL) {
        app_shell_add_action(app, "exercise.performance", 'p', "p Performances", 1U,
            TRAINLOG_INTENT_PRIMARY, route);
        app_shell_add_action(app, "exercise.max", 'm', "m MAX", 2U,
            TRAINLOG_INTENT_PRIMARY, route);
        app_shell_add_action(app, "exercise.knowledge", 'k', "k Connaissances", 3U,
            TRAINLOG_INTENT_PRIMARY, route);
        app_shell_add_action(app, "exercise.edit", 'e', "e Modifier", 4U,
            TRAINLOG_INTENT_PRIMARY, route);
        app_shell_add_action(app, "exercise.merge", 'u', "u Fusionner avec…", 5U,
            TRAINLOG_INTENT_NONE, route);
    }
    if (route == TRAINLOG_ROUTE_EXERCISES)
        app_shell_add_action(app, "exercise.create", 'a', "a Créer", 1U,
            TRAINLOG_INTENT_NONE, route);
    if (route == TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE)
        app_shell_add_action(app, "knowledge.close", 'k', "k Fermer", 1U,
            TRAINLOG_INTENT_BACK, TRAINLOG_ROUTE_EXERCISE_DETAIL);
    if (route == TRAINLOG_ROUTE_EXERCISE_MAX)
        app_shell_add_action(app, "max.rounding", 'r', "r Arrondi", 1U,
            TRAINLOG_INTENT_NONE, route);
    if (route == TRAINLOG_ROUTE_SESSION_DETAIL)
        app_shell_add_action(app, "session.edit", 'e', "e Modifier", 1U,
            TRAINLOG_INTENT_NONE, route);
    /* CONTRACT: destination aliases live in the same model as contextual
     * actions. Dispatcher, footer selection and F7 therefore cannot drift. */
    app_shell_add_action(app, "home", '0', "0 Accueil", 100U,
        TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_HOME);
    app_shell_add_action(app, "session.alias", '1', "1 Séance", 101U,
        TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_SESSION_MANUAL);
    app_shell_add_action(app, "completed.alias", '2', "2 Effectuées", 102U,
        TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_SESSIONS_COMPLETED);
    app_shell_add_action(app, "exercises.alias", '3', "3 Exercices", 103U,
        TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_EXERCISES);
    app_shell_add_action(app, "equipment.alias", '4', "4 Équipements", 104U,
        TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_EQUIPMENT);
    app_shell_add_action(app, "body.alias", '5', "5 Mensurations", 105U,
        TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_BODY);
    app_shell_add_action(app, "sync.alias", '6', "6 Synchronisation", 106U,
        TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_SYNC);
    app_shell_add_action(app, "navigation", TRAINLOG_KEY_F6, "F6 Navigation", 80U,
        TRAINLOG_INTENT_OPEN_NAVIGATION, route);
    app_shell_add_action(app, "actions", TRAINLOG_KEY_F7, "F7 Actions", 81U,
        TRAINLOG_INTENT_OPEN_ACTIONS, route);
    app_shell_add_action(app, "help", '?', "? Aide", 82U,
        TRAINLOG_INTENT_OPEN_HELP, route);
    app_shell_add_action(app, "back", TRAINLOG_KEY_ESCAPE,
        route == TRAINLOG_ROUTE_HOME ? "q Quitter" : "Échap Retour", 83U,
        route == TRAINLOG_ROUTE_HOME ? TRAINLOG_INTENT_QUIT : TRAINLOG_INTENT_BACK,
        route);
}

static void app_shell_render_footer(TrainlogAppContext *app)
{
    const TrainlogOverlay *overlay = trainlog_overlays_top(&app->overlays);
    const TrainlogAction *first;
    const TrainlogAction *second;
    trainlog_surface_erase(app->footer);
    trainlog_surface_set_role(app->footer, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_MANTLE, TRAINLOG_TEXT_NORMAL);
    if (overlay != NULL) {
        if (overlay->type == TRAINLOG_OVERLAY_CONFIRMATION && app->quit_confirmation) {
            trainlog_surface_printf(app->footer, 0, 2,
                "↑↓ choisir   Entrée valider");
            trainlog_surface_printf(app->footer, 1, 2,
                "Annuler conserve l’état de cette exécution");
            return;
        }
        trainlog_surface_printf(app->footer, 0, 2, "↑↓ choisir   Entrée valider");
        trainlog_surface_printf(app->footer, 1, 2, "Échap fermer   F7 Actions   ? Aide");
        return;
    }
    first = trainlog_actions_at_priority(&app->actions, 0U);
    second = trainlog_actions_at_priority(&app->actions, 1U);
    if (first != NULL) trainlog_surface_printf(app->footer, 0, 2, "%s", first->label);
    if (second != NULL) trainlog_surface_printf(app->footer, 0, 24, "%s", second->label);
    trainlog_surface_printf(app->footer, 1, 2,
        "F6 Navigation   F7 Actions   ? Aide   %s",
        app->navigation.current.route == TRAINLOG_ROUTE_HOME ? "q Quitter" : "Échap Retour");
}

static void app_shell_render_sidebar(TrainlogAppContext *app)
{
    size_t index;
    if (app->sidebar == NULL) return;
    trainlog_surface_erase(app->sidebar);
    trainlog_surface_set_role(app->sidebar, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_MANTLE, TRAINLOG_TEXT_NORMAL);
    for (index = 0U; index < sizeof(shell_sections) / sizeof(shell_sections[0]); ++index) {
        bool active = trainlog_route_section(app->navigation.current.route) == shell_sections[index];
        bool focused = app->focus == TRAINLOG_FOCUS_NAVIGATION &&
            app->navigation_selected == index;
        trainlog_surface_set_role(app->sidebar,
            focused || active ? TRAINLOG_COLOR_ACCENT : TRAINLOG_COLOR_DEFAULT,
            focused ? TRAINLOG_RGB_SURFACE0 : TRAINLOG_RGB_MANTLE,
            focused ? TRAINLOG_TEXT_BOLD : TRAINLOG_TEXT_NORMAL);
        trainlog_surface_printf(app->sidebar, 1 + (int)index * 2, 1,
            "%c%c %-17s", focused ? '>' : ' ', active ? '[' : ' ',
            shell_section_labels[index]);
        if (active && app->layout.sidebar_expanded) {
            const char *subroute = NULL;
            switch (app->navigation.current.route) {
            case TRAINLOG_ROUTE_SESSION_CURRENT: subroute = "· Séance en cours"; break;
            case TRAINLOG_ROUTE_SESSION_GENERATOR: subroute = "· Programmer"; break;
            case TRAINLOG_ROUTE_SESSION_MANUAL: subroute = "· Nouvelle séance"; break;
            case TRAINLOG_ROUTE_SESSIONS_COMPLETED: subroute = "· Effectuées"; break;
            case TRAINLOG_ROUTE_SESSION_DETAIL: subroute = "· Détail séance"; break;
            case TRAINLOG_ROUTE_EXERCISE_DETAIL: subroute = "· Fiche exercice"; break;
            case TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE: subroute = "· Connaissances"; break;
            case TRAINLOG_ROUTE_EXERCISE_PERFORMANCE: subroute = "· Performances"; break;
            case TRAINLOG_ROUTE_EXERCISE_MAX: subroute = "· MAX exercice"; break;
            case TRAINLOG_ROUTE_EQUIPMENT_DETAIL: subroute = "· Fiche équipement"; break;
            case TRAINLOG_ROUTE_STATS_EXERCISE: subroute = "· Par exercice"; break;
            case TRAINLOG_ROUTE_BODY: subroute = "· Mensurations"; break;
            case TRAINLOG_ROUTE_BODY_DETAIL: subroute = "· Relevé corporel"; break;
            case TRAINLOG_ROUTE_BODY_METRIC: subroute = "· Historique mesure"; break;
            case TRAINLOG_ROUTE_BODY_TRENDS: subroute = "· Évolution 12 mois"; break;
            case TRAINLOG_ROUTE_BODY_GLOBAL: subroute = "· Vue globale"; break;
            case TRAINLOG_ROUTE_BODY_ANALYTICS: subroute = "· Analyse corporelle"; break;
            case TRAINLOG_ROUTE_MAX: subroute = "· Capacités / MAX"; break;
            default: break;
            }
            if (subroute != NULL)
                trainlog_surface_printf(app->sidebar,
                    2 + (int)index * 2, 3, "%s", subroute);
        }
    }
}

static void app_shell_render_session_controller(TrainlogAppContext *app)
{
    TrainlogSessionController *session = &app->session;
    int row = 3;
    size_t index;
    if (session->phase == TRAINLOG_SESSION_CHOOSE_TYPE) {
        trainlog_surface_printf(app->content, row++, 2, "Choisissez le type de séance :");
        trainlog_surface_printf(app->content, row++, 4, "%c Entraînement",
            session->session_type == TRAINLOG_SESSION_TRAINING ? '>' : ' ');
        trainlog_surface_printf(app->content, row, 4, "%c Test max explicite",
            session->session_type == TRAINLOG_SESSION_MAX_TEST ? '>' : ' ');
    } else if (session->phase == TRAINLOG_SESSION_EXERCISE_PICKER) {
        trainlog_surface_printf(app->content, row++, 2,
            "%s — choisir un exercice", session->replacing_occurrence
                ? "Remplacer l’occurrence" : "Ajouter une occurrence");
        for (index = 0U; index < session->picker_count &&
             row < app->layout.content.height; ++index)
            trainlog_surface_printf(app->content, row++, 4, "%c %.52s",
                index == session->picker_selected ? '>' : ' ',
                session->picker[index].name);
    } else if (session->phase == TRAINLOG_SESSION_EQUIPMENT_PICKER) {
        trainlog_surface_printf(app->content, row++, 2,
            "Équipement de l’occurrence — ID stable de l’occurrence inchangé");
        trainlog_surface_printf(app->content, row++, 4, "%c Aucun équipement",
            session->equipment_selected == 0U ? '>' : ' ');
        for (index = 0U; index < session->equipment_count &&
             row < app->layout.content.height; ++index)
            trainlog_surface_printf(app->content, row++, 4, "%c %.46s  %s",
                session->equipment_selected == index + 1U ? '>' : ' ',
                app->equipment[index].display_name,
                equipment_origin_label(app->equipment[index].origin));
        trainlog_surface_printf(app->content, app->layout.content.height - 2, 2,
            "Entrée choisir · x aucun · n créer puis reprendre (phase catalogue)");
    } else if (session->phase == TRAINLOG_SESSION_EQUIPMENT_CREATE) {
        const char *label = session->form_purpose == TRAINLOG_SESSION_FORM_EQUIPMENT_NAME
            ? "Nom convivial" : session->form_purpose == TRAINLOG_SESSION_FORM_EQUIPMENT_LABEL
            ? "Nom étiquette (optionnel)" :
            session->form_purpose == TRAINLOG_SESSION_FORM_EQUIPMENT_TYPE
            ? "Type" : "Charge : 1 aucune · 2 externe · 3 assistance";
        trainlog_surface_printf(app->content, row++, 2,
            "Créer un équipement personnel — retour à l’occurrence");
        trainlog_surface_printf(app->content, row++, 4, "%s", label);
        trainlog_surface_printf(app->content, row++, 4, "> %s", session->form.text);
        if (session->message[0] != '\0')
            trainlog_surface_printf(app->content, row + 1, 4, "%s", session->message);
    } else if (session->phase == TRAINLOG_SESSION_PLANNING &&
               session->selected < session->draft_count) {
        TrainlogSessionDraftExercise *draft = &session->drafts[session->selected];
        static const char *const labels[] = {"Séries cibles", "Répétitions cibles",
            "Durée cible (s)", "Repos (s)", "Charge/assistance cible (kg)"};
        char values[5][48];
        (void)snprintf(values[0], sizeof(values[0]), "%d", draft->input.target_sets);
        (void)snprintf(values[1], sizeof(values[1]), "%d", draft->input.target_reps);
        (void)snprintf(values[2], sizeof(values[2]), "%d",
            draft->input.target_duration_seconds);
        (void)snprintf(values[3], sizeof(values[3]), "%d", draft->input.rest_seconds);
        if (draft->input.target_has_weight)
            (void)snprintf(values[4], sizeof(values[4]), "%.2f",
                draft->input.target_weight_kg);
        else (void)snprintf(values[4], sizeof(values[4]), "—");
        if (draft->input.recording_mode != TRAINLOG_RECORDING_SETS) {
            (void)snprintf(values[0], sizeof(values[0]), "non applicable");
            (void)snprintf(values[3], sizeof(values[3]), "non applicable");
        }
        if (draft->tracking_mode != TRAINLOG_TRACKING_REPS)
            (void)snprintf(values[1], sizeof(values[1]), "non applicable");
        if (draft->tracking_mode != TRAINLOG_TRACKING_DURATION)
            (void)snprintf(values[2], sizeof(values[2]), "non applicable");
        if (draft->input.load_mode == TRAINLOG_LOAD_NONE)
            (void)snprintf(values[4], sizeof(values[4]), "non applicable");
        trainlog_surface_printf(app->content, row++, 2,
            "Objectif prévu — séparé des valeurs réalisées");
        for (index = 0U; index < 5U; ++index)
            trainlog_surface_printf(app->content, row++, 4, "%c %-32s %s",
                index == session->planning_field ? '>' : ' ', labels[index], values[index]);
        if (row < app->layout.content.height - 2)
            trainlog_surface_printf(app->content, row++, 4,
                "%% sur la charge : calcul utilisateur 1..100%% du dernier MAX compatible");
        if (session->form.active)
            trainlog_surface_printf(app->content, app->layout.content.height - 3, 2,
                "> %s", session->form.text);
    } else if (session->phase == TRAINLOG_SESSION_ACTUALS &&
               session->selected < session->draft_count) {
        TrainlogSessionDraftExercise *draft = &session->drafts[session->selected];
        trainlog_surface_printf(app->content, row++, 2, "%s", draft->name);
        if (draft->input.has_max_weight || session->session_type == TRAINLOG_SESSION_MAX_TEST)
            trainlog_surface_printf(app->content, row++, 4,
                "MAX explicite : %s%.2f kg", draft->input.has_max_weight ? "" : "à saisir · ",
                draft->input.has_max_weight ? draft->input.max_weight_kg : 0.0);
        else if (draft->input.recording_mode == TRAINLOG_RECORDING_CONTINUOUS)
        {
            trainlog_surface_printf(app->content, row++, 4,
                "Activité continue : %d s · aucune série fictive",
                draft->input.continuous_duration_seconds);
            if ((draft->input.data_fields & TRAINLOG_EXERCISE_DATA_SPEED_KMH) != 0U)
                trainlog_surface_printf(app->content, row++, 4, "Vitesse : %s%.2f km/h",
                    draft->input.continuous_has_speed ? "" : "— ",
                    draft->input.continuous_has_speed
                        ? draft->input.continuous_speed_kmh : 0.0);
            if ((draft->input.data_fields & TRAINLOG_EXERCISE_DATA_DISTANCE_KM) != 0U)
                trainlog_surface_printf(app->content, row++, 4, "Distance : %s%.2f km",
                    draft->input.continuous_has_distance ? "" : "— ",
                    draft->input.continuous_has_distance
                        ? draft->input.continuous_distance_km : 0.0);
            trainlog_surface_printf(app->content, row++, 4,
                "e durée · s vitesse · k distance");
        }
        else {
            trainlog_surface_printf(app->content, row++, 2,
                "Séries réalisées — cibles séparées : %d×%d, repos %d s",
                draft->input.target_sets, draft->input.target_reps,
                draft->input.rest_seconds);
            for (index = 0U; index < draft->input.set_count &&
                 row < app->layout.content.height; ++index) {
                char metric[64];
                draft_format_set_metric(draft, &draft->sets[index], metric,
                    sizeof(metric));
                trainlog_surface_printf(app->content, row++, 4,
                    "%c %zu. %-16s  %s%.2f kg", index == session->set_selected ? '>' : ' ',
                    index + 1U, metric, draft->sets[index].has_weight ? "" : "— ",
                    draft->sets[index].has_weight ? draft->sets[index].weight_kg : 0.0);
            }
            if (draft->input.set_count == 0U)
                trainlog_surface_printf(app->content, row++, 4,
                    "Aucune série réelle. a pour saisir la première.");
        }
        if (session->form.active) {
            trainlog_surface_printf(app->content, app->layout.content.height - 3, 2,
                "> %s%s", session->form.text,
                session->form.capacity_error ? "  [200 octets maximum]" : "");
        }
    } else if (session->phase == TRAINLOG_SESSION_GENERATOR_CONFIG) {
        trainlog_surface_printf(app->content, row++, 2, "Paramètres de la proposition");
        trainlog_surface_printf(app->content, row++, 4, "Zone : %s",
            session->generation_zone != NULL ? session->generation_zone->display_name : "—");
        trainlog_surface_printf(app->content, row++, 4, "Objectif : %s",
            session->generation_goal != NULL ?
                generator_goal_label(session->generation_goal->id) : "—");
        trainlog_surface_printf(app->content, row++, 4, "Durée : %d min",
            session->generation_duration_minutes);
        trainlog_surface_printf(app->content, row + 1, 2,
            "z zone · o objectif · t durée · g générer");
    } else if (session->phase == TRAINLOG_SESSION_GENERATOR_WARNING) {
        TrainlogBodyZoneRecentExposure *exposure = &session->generated.exposure;
        trainlog_surface_printf(app->content, row++, 2, "Zone travaillée récemment");
        trainlog_surface_printf(app->content, row++, 4,
            "24 h : %zu séries principales, %zu secondaires (%zu séances)",
            exposure->within_24h.primary_set_count,
            exposure->within_24h.secondary_set_count,
            exposure->within_24h.session_count);
        trainlog_surface_printf(app->content, row++, 4,
            "72 h : %zu séries principales, %zu secondaires (%zu séances)",
            exposure->within_72h.primary_set_count,
            exposure->within_72h.secondary_set_count,
            exposure->within_72h.session_count);
        trainlog_surface_printf(app->content, row + 1, 2,
            "c/Entrée continuer · z changer de zone · Échap conserver et revenir");
    } else if (session->phase == TRAINLOG_SESSION_GENERATOR_PREVIEW) {
        trainlog_surface_printf(app->content, row++, 2,
            "%zu exercice(s) · cible %d min · estimation %d min",
            session->generated.exercise_count,
            session->generation_duration_minutes,
            session->generated.estimated_duration_seconds / 60);
        if (session->generation_duration_minutes * 60 -
                session->generated.estimated_duration_seconds >= 300)
            trainlog_surface_printf(app->content, row++, 2,
                "Proposition plus courte : aucun remplissage artificiel.");
        trainlog_surface_printf(app->content, row++, 2,
            "Échauffement et retour au calme absents en V1.");
        for (index = 0U; index < session->generated.exercise_count &&
             row + 1 < app->layout.content.height; ++index) {
            TrainlogGeneratedExercise *item = &session->generated.exercises[index];
            trainlog_surface_printf(app->content, row++, 4, "%c %zu. %.38s",
                index == session->selected ? '>' : ' ', index + 1U,
                session->generated_items[index].exercise_name);
            trainlog_surface_printf(app->content, row++, 7,
                "%d×%d · repos %d s · %.30s", item->target_sets,
                item->target_repetitions, item->rest_seconds,
                session->generated_items[index].equipment_name);
            if (index == session->selected && row < app->layout.content.height) {
                if (item->has_target_weight)
                    trainlog_surface_printf(app->content, row++, 7,
                        "%.2f kg · %s", item->target_weight_kg,
                        item->rationale_count > 0U && strcmp(item->rationale_codes[0],
                            "user_selected_max_percentage") == 0
                            ? "pourcentage MAX choisi" : "charge automatique observée");
                else trainlog_surface_printf(app->content, row++, 7,
                    item->rationale_count > 0U && strcmp(item->rationale_codes[0],
                        "compatible_max_unavailable") == 0
                        ? "MAX compatible indisponible"
                        : "Aucune charge numérique qualifiée");
            }
        }
    } else if (session->phase == TRAINLOG_SESSION_CONFIRM_REMOVE) {
        trainlog_surface_printf(app->content, row++, 2,
            "Retirer cette occurrence de la séance ?");
        trainlog_surface_printf(app->content, row, 4,
            "1 confirmer · 0/Échap annuler — le catalogue reste intact");
    } else if (session->phase == TRAINLOG_SESSION_CONFIRM_ABANDON) {
        trainlog_surface_printf(app->content, row++, 2,
            "Abandonner la séance en cours ?");
        trainlog_surface_printf(app->content, row, 4,
            "1 confirmer · 0/Échap annuler");
    } else if (session->phase == TRAINLOG_SESSION_CONFIRM_LEAVE) {
        trainlog_surface_printf(app->content, row++, 2,
            "Proposition non acceptée conservée en mémoire.");
        trainlog_surface_printf(app->content, row, 4,
            "Entrée conserver et revenir · d Abandonner la proposition");
    } else if (session->phase == TRAINLOG_SESSION_MESSAGE) {
        trainlog_surface_printf(app->content, row++, 2, "%s", session->message);
        trainlog_surface_printf(app->content, row, 2, "Entrée pour continuer");
    } else {
        trainlog_surface_printf(app->content, row++, 2, "Type : %s",
            session_type_label(session->session_type));
        if (session->draft_count == 0U)
            trainlog_surface_printf(app->content, row, 2,
                "Aucune occurrence. a pour choisir un exercice.");
        for (index = 0U; index < session->draft_count &&
             row < app->layout.content.height; ++index) {
            char summary[128];
            draft_set_summary(&session->drafts[index], summary, sizeof(summary));
            trainlog_surface_printf(app->content, row++, 2, "%c %zu. %-28.28s  %.38s",
                index == session->selected ? '>' : ' ', index + 1U,
                session->drafts[index].name, summary);
        }
        if (session->message[0] != '\0')
            trainlog_surface_printf(app->content, app->layout.content.height - 2,
                2, "%s", session->message);
    }
}

static void app_shell_render_equipment_controller(TrainlogAppContext *app)
{
    TrainlogEquipmentController *controller = &app->equipment_controller;
    const char *label;
    int row = 4;
    trainlog_surface_printf(app->content, row++, 2,
        "Créer un équipement personnel");
    if (controller->phase == TRAINLOG_EQUIPMENT_CONFIRM_DISCARD) {
        trainlog_surface_printf(app->content, row++, 2,
            "Abandonner cette définition non enregistrée ?");
        trainlog_surface_printf(app->content, row, 4,
            "1 abandonner · 0/Échap reprendre");
        return;
    }
    if (controller->phase == TRAINLOG_EQUIPMENT_MESSAGE) {
        trainlog_surface_printf(app->content, row++, 2, "%s", controller->message);
        trainlog_surface_printf(app->content, row, 2, "Entrée pour revenir au catalogue");
        return;
    }
    trainlog_surface_printf(app->content, row++, 2, "Nom : %s",
        controller->pending.display_name[0] != '\0'
            ? controller->pending.display_name : "—");
    trainlog_surface_printf(app->content, row++, 2, "Étiquette : %s",
        controller->pending.label_name[0] != '\0'
            ? controller->pending.label_name : "—");
    trainlog_surface_printf(app->content, row++, 2, "Type : %s",
        controller->pending.equipment_type[0] != '\0'
            ? controller->pending.equipment_type : "—");
    trainlog_surface_printf(app->content, row++, 2,
        "Charge : %s", controller->load_choice == 1 ? "aucune" :
        controller->load_choice == 2 ? "externe" : "assistance");
    row += 1;
    if (controller->phase == TRAINLOG_EQUIPMENT_LOAD) {
        trainlog_surface_printf(app->content, row++, 2,
            "↑↓ ou 1–3 choisir · Entrée enregistrer");
    } else {
        label = controller->phase == TRAINLOG_EQUIPMENT_NAME ? "Nom convivial" :
            controller->phase == TRAINLOG_EQUIPMENT_LABEL
                ? "Nom d’étiquette (optionnel)" : "Type";
        trainlog_surface_printf(app->content, row++, 2, "%s", label);
        trainlog_surface_printf(app->content, row++, 4, "> %s%s",
            controller->form.text,
            controller->form.capacity_error ? "  [200 octets maximum]" : "");
    }
    if (controller->message[0] != '\0')
        trainlog_surface_printf(app->content, row + 1, 2, "%s",
            controller->message);
}

static void app_shell_render_exercise_controller(TrainlogAppContext *app)
{
    TrainlogExerciseController *controller = &app->exercise_controller;
    int row = 4;
    trainlog_surface_printf(app->content, row++, 2, "%s un exercice",
        controller->editing ? "Modifier" : "Créer");
    if (controller->phase == TRAINLOG_EXERCISE_CONFIRM_DISCARD) {
        trainlog_surface_printf(app->content, row++, 2,
            "Abandonner les modifications non enregistrées ?");
        trainlog_surface_printf(app->content, row, 4,
            "1 abandonner · 0/Échap reprendre");
        return;
    }
    if (controller->phase == TRAINLOG_EXERCISE_MESSAGE) {
        trainlog_surface_printf(app->content, row++, 2, "%s", controller->message);
        trainlog_surface_printf(app->content, row, 2, "Entrée pour revenir");
        return;
    }
    trainlog_surface_printf(app->content, row++, 2, "Nom : %s",
        controller->pending.name[0] != '\0' ? controller->pending.name : "—");
    trainlog_surface_printf(app->content, row++, 2, "Suivi : %s",
        controller->pending.tracking_mode == TRAINLOG_TRACKING_REPS
            ? "répétitions" : "durée");
    trainlog_surface_printf(app->content, row++, 2, "Organisation : %s",
        controller->pending.recording_mode == TRAINLOG_RECORDING_CONTINUOUS
            ? "continue" : "séries");
    if (controller->editing)
        trainlog_surface_printf(app->content, row++, 2,
            "Identifiant stable conservé : %s",
            controller->pending.exercise_id);
    row += 1;
    if (controller->phase == TRAINLOG_EXERCISE_NAME) {
        trainlog_surface_printf(app->content, row++, 2, "Nom d’affichage");
        trainlog_surface_printf(app->content, row++, 4, "> %s%s",
            controller->form.text,
            controller->form.capacity_error ? "  [200 octets maximum]" : "");
    } else if (controller->phase == TRAINLOG_EXERCISE_TRACKING)
        trainlog_surface_printf(app->content, row++, 2,
            "Suivi : 1 répétitions · 2 durée · ↑↓ choisir · Entrée continuer");
    else if (controller->phase == TRAINLOG_EXERCISE_RECORDING)
        trainlog_surface_printf(app->content, row++, 2,
            "Organisation : 1 séries · 2 continu · ↑↓ choisir · Entrée continuer");
    else if (controller->phase == TRAINLOG_EXERCISE_SPEED)
        trainlog_surface_printf(app->content, row++, 2,
            "Mesurer la vitesse : %s · 0 non · 1 oui · ↑↓ choisir",
            (controller->pending.data_fields & TRAINLOG_EXERCISE_DATA_SPEED_KMH)
                != 0U ? "oui" : "non");
    else if (controller->phase == TRAINLOG_EXERCISE_DISTANCE)
        trainlog_surface_printf(app->content, row++, 2,
            "Mesurer la distance : %s · 0 non · 1 oui · ↑↓ choisir",
            (controller->pending.data_fields & TRAINLOG_EXERCISE_DATA_DISTANCE_KM)
                != 0U ? "oui" : "non");
    else if (controller->phase == TRAINLOG_EXERCISE_ZONES) {
        size_t index;
        const TrainlogBodyZone *primary = trainlog_body_zone_catalog_lookup(
            controller->primary_zone);
        trainlog_surface_printf(app->content, row++, 2,
            "Zone principale : %s · secondaires : %zu",
            primary != NULL ? primary->display_name : "Non renseignée",
            controller->secondary_count);
        for (index = controller->zone_selected;
             index < trainlog_body_zone_catalog_count() &&
             row < app->layout.content.height - 2; ++index) {
            const TrainlogBodyZone *zone = trainlog_body_zone_catalog_at(index);
            trainlog_surface_printf(app->content, row++, 4, "%c %s%s",
                index == controller->zone_selected ? '>' : ' ',
                zone != NULL ? zone->display_name : "?",
                zone != NULL && zone->is_group ? " [groupe]" : "");
        }
        trainlog_surface_printf(app->content,
            app->layout.content.height - 2, 2,
            "p principale · Espace secondaire · n effacer · Entrée enregistrer");
    }
    if (controller->message[0] != '\0')
        trainlog_surface_printf(app->content,
            app->layout.content.height - 3, 2, "%s", controller->message);
}

static int app_shell_graph_height(const TrainlogAppContext *app, int top)
{
    int available = app->layout.content.height - top - 2;
    if (available > 7) return 7;
    return available;
}

static void app_shell_draw_performance_graph(
    TrainlogAppContext *app,
    TrainlogLoadMode mode,
    TrainlogTrackingMode tracking_mode,
    int top,
    int height,
    bool measured_max)
{
    size_t indices[EXERCISE_GRAPH_POINTS];
    size_t selected_count = 0U;
    size_t index;
    double minimum;
    double maximum;
    const int left = 11;
    int width = app->layout.content.width - left - 2;

    /* CONTRACT: graph coordinates are relative to the bounded content plane.
     * Header/footer/sidebar ownership is therefore preserved at every valid
     * resize, including the minimum 72x20 viewport. */
    if (height < 3 || width < 12 || top < 1 ||
        top + height >= app->layout.content.height) return;
    for (index = 0U; index < app->performance_count &&
         selected_count < EXERCISE_GRAPH_POINTS; ++index) {
        const TrainlogExercisePerformancePoint *point =
            &app->performance_points[index];
        if (point->has_performance != 0 && point->load_mode == mode &&
            (!measured_max || point->session_type == TRAINLOG_SESSION_MAX_TEST))
            indices[selected_count++] = index;
    }
    if (selected_count == 0U) {
        trainlog_surface_printf(app->content, top, 2,
            "Aucune performance réussie pour ce mode.");
        return;
    }
    minimum = exercise_graph_value(&app->performance_points[indices[0]]);
    maximum = minimum;
    for (index = 1U; index < selected_count; ++index) {
        double value = exercise_graph_value(
            &app->performance_points[indices[index]]);
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
    }
    if (maximum == minimum) { minimum -= 1.0; maximum += 1.0; }
    trainlog_surface_set_role(app->content,
        mode == TRAINLOG_LOAD_ASSISTANCE ? TRAINLOG_COLOR_WARNING
                                         : TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    trainlog_surface_printf(app->content, top - 1, left, "%s",
        mode == TRAINLOG_LOAD_ASSISTANCE
            ? (measured_max ? "Assistance mesurée (kg) — moins = mieux"
                            : "Assistance (kg) — moins = mieux")
            : mode == TRAINLOG_LOAD_EXTERNAL
            ? (measured_max ? "MAX mesuré (kg)" : "Charge du meilleur set (kg)")
            : tracking_mode == TRAINLOG_TRACKING_DURATION
            ? (measured_max ? "Durée MAX mesurée" : "Meilleure durée")
            : (measured_max ? "Répétitions MAX mesurées" : "Meilleures répétitions"));
    trainlog_surface_printf(app->content, top, 2, "%.1f", maximum);
    trainlog_surface_printf(app->content, top + height - 1, 2, "%.1f", minimum);
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_GRAPH,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    {
        int previous_x = -1;
        int previous_y = -1;
        size_t order;
        for (order = selected_count; order > 0U; --order) {
            size_t chronological = selected_count - order;
            const TrainlogExercisePerformancePoint *point =
                &app->performance_points[indices[order - 1U]];
            double ratio = (exercise_graph_value(point) - minimum) /
                (maximum - minimum);
            int x = selected_count == 1U ? left + width / 2 : left +
                (int)((chronological * (size_t)(width - 1)) /
                      (selected_count - 1U));
            int y = top + height - 1 -
                (int)(ratio * (double)(height - 1));
            if (previous_x >= 0 && x > previous_x) {
                int line_x;
                for (line_x = previous_x + 1; line_x < x; ++line_x) {
                    int line_y = previous_y + ((y - previous_y) *
                        (line_x - previous_x)) / (x - previous_x);
                    trainlog_surface_draw(app->content, line_y, line_x, '.');
                }
            }
            trainlog_surface_draw(app->content, y, x,
                chronological + 1U == selected_count ? 'O' : '*');
            previous_x = x;
            previous_y = y;
        }
    }
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_MUTED,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    {
        char oldest[11];
        char newest[11];
        exercise_short_date(app->performance_points[
            indices[selected_count - 1U]].started_at, oldest);
        exercise_short_date(app->performance_points[indices[0]].started_at, newest);
        trainlog_surface_printf(app->content, top + height, left, "%s", oldest);
        trainlog_surface_printf(app->content, top + height,
            left + width - 10, "%s", newest);
    }
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
}

static void app_shell_draw_body_points(TrainlogAppContext *app,
    const TrainlogBodyMetricPoint *points, size_t count, int top, int height,
    const char *unit)
{
    size_t start;
    size_t index;
    double minimum;
    double maximum;
    int left = 11;
    int width = app->layout.content.width - left - 2;
    if (count == 0U) {
        trainlog_surface_printf(app->content, top, 2,
            "Aucune donnée pour cette mesure.");
        return;
    }
    if (height < 3 || width < 10 || top + height >= app->layout.content.height)
        return;
    start = count > (size_t)width ? count - (size_t)width : 0U;
    minimum = points[start].value;
    maximum = minimum;
    for (index = start + 1U; index < count; ++index) {
        if (points[index].value < minimum) minimum = points[index].value;
        if (points[index].value > maximum) maximum = points[index].value;
    }
    if (maximum == minimum) { minimum -= 1.0; maximum += 1.0; }
    trainlog_surface_printf(app->content, top, 2, "%.1f", maximum);
    trainlog_surface_printf(app->content, top + height - 1, 2, "%.1f", minimum);
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_GRAPH,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    for (index = start; index < count; ++index) {
        double ratio = (points[index].value - minimum) / (maximum - minimum);
        int x = left + (int)(index - start);
        int y = top + height - 1 - (int)(ratio * (double)(height - 1));
        trainlog_surface_draw(app->content, y, x, '*');
    }
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_MUTED,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    trainlog_surface_printf(app->content, top + height, left,
        "%zu point(s) · %s", count, unit);
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
}

static void app_shell_render_body_controller(TrainlogAppContext *app)
{
    TrainlogBodyController *controller = &app->body_controller;
    const char *label = controller->field < 14U
        ? BODY_METRICS[controller->field].label : "Mesure";
    trainlog_surface_printf(app->content, 4, 2, "%s — champ %zu/14",
        controller->editing ? "Modifier le relevé" : "Nouveau relevé",
        controller->field + 1U);
    if (controller->phase == TRAINLOG_BODY_CONFIRM_DISCARD) {
        trainlog_surface_printf(app->content, 7, 2,
            "Abandonner toutes les modifications ?");
        trainlog_surface_printf(app->content, 9, 2,
            "1 abandonner · 0/Échap reprendre");
    } else if (controller->phase == TRAINLOG_BODY_MESSAGE) {
        trainlog_surface_printf(app->content, 7, 2, "%s", controller->message);
        trainlog_surface_printf(app->content, 9, 2, "Entrée pour revenir");
    } else {
        trainlog_surface_printf(app->content, 7, 2, "%s (%s)", label,
            BODY_METRICS[controller->field].unit);
        trainlog_surface_printf(app->content, 9, 2, "> %s",
            controller->form.text);
        trainlog_surface_printf(app->content, 11, 2, "%s",
            controller->editing
                ? "vide conserve · - efface · valeur positive remplace"
                : "vide signifie mesure non faite · valeur positive ajoute");
        if (controller->message[0] != '\0')
            trainlog_surface_printf(app->content, 13, 2, "%s",
                controller->message);
    }
}

static void app_shell_render_profile_controller(TrainlogAppContext *app)
{
    TrainlogProfileController *controller = &app->profile_controller;
    trainlog_surface_printf(app->content, 4, 2,
        "Profil d’estimation corporelle");
    if (controller->phase == TRAINLOG_PROFILE_CONFIRM_DISCARD) {
        trainlog_surface_printf(app->content, 7, 2,
            "Abandonner les modifications du profil ?");
        trainlog_surface_printf(app->content, 9, 2,
            "1 abandonner · 0/Échap reprendre");
    } else if (controller->phase == TRAINLOG_PROFILE_MESSAGE) {
        trainlog_surface_printf(app->content, 7, 2, "%s", controller->message);
    } else {
        trainlog_surface_printf(app->content, 7, 2, "%s",
            controller->phase == TRAINLOG_PROFILE_FORMULA
                ? "Formule : 1 homme · 2 femme"
                : "Taille en cm : 100 à 250");
        trainlog_surface_printf(app->content, 9, 2, "> %s",
            controller->form.text);
        if (controller->message[0] != '\0')
            trainlog_surface_printf(app->content, 11, 2, "%s",
                controller->message);
    }
}

static void app_shell_render_body_detail(TrainlogAppContext *app)
{
    const TrainlogBodyObservationRecord *record = &app->body_detail;
    const bool present[14] = {record->has_body_weight, record->has_neck,
        record->has_shoulders, record->has_chest, record->has_waist,
        record->has_hips, record->has_left_arm, record->has_right_arm,
        record->has_left_forearm, record->has_right_forearm,
        record->has_left_thigh, record->has_right_thigh,
        record->has_left_calf, record->has_right_calf};
    const double values[14] = {record->body_weight_kg, record->neck_cm,
        record->shoulders_cm, record->chest_cm, record->waist_cm,
        record->hips_cm, record->left_arm_cm, record->right_arm_cm,
        record->left_forearm_cm, record->right_forearm_cm,
        record->left_thigh_cm, record->right_thigh_cm,
        record->left_calf_cm, record->right_calf_cm};
    size_t index;
    int row = 4;
    trainlog_surface_printf(app->content, row++, 2, "Date : %s",
        record->observed_at);
    trainlog_surface_printf(app->content, row++, 2, "Séance liée : %s",
        record->session_id[0] != '\0' ? record->session_id : "aucune");
    for (index = app->content_scroll; index < 14U &&
         row < app->layout.content.height - 1; ++index) {
        if (present[index])
            trainlog_surface_printf(app->content, row++, 2, "%-22s %.2f %s",
                BODY_METRICS[index].label, values[index], BODY_METRICS[index].unit);
        else trainlog_surface_printf(app->content, row++, 2, "%-22s —",
            BODY_METRICS[index].label);
    }
}

static void app_shell_surface_segment(TrainlogSurface *surface, int x1, int y1,
                                      int x2, int y2, uint32_t symbol)
{
    int x;
    if (surface == NULL || x2 < x1) return;
    if (x1 == x2) { trainlog_surface_draw(surface, y2, x2, symbol); return; }
    for (x = x1; x <= x2; ++x) {
        int y = y1 + ((y2 - y1) * (x - x1)) / (x2 - x1);
        trainlog_surface_draw(surface, y, x, symbol);
    }
}

static void app_shell_render_body_global(TrainlogAppContext *app)
{
    size_t metric;
    double minimum = 100.0;
    double maximum = 100.0;
    int graph_top = 4;
    int graph_height = app->layout.content.height >= 20 ? 7 : 5;
    int left = 8;
    int width = app->layout.content.width - left - 3;
    for (metric = 0U; metric < 14U; ++metric) {
        TrainlogGlobalBodySeries *series = &app->body_global_series[metric];
        size_t point;
        if (!app->body_global_enabled[metric]) continue;
        for (point = 0U; point < series->count; ++point) {
            double value;
            if (trainlog_body_index100(series->baseline,
                    series->points[point].value, &value) != TRAINLOG_STATUS_OK)
                continue;
            if (value < minimum) minimum = value;
            if (value > maximum) maximum = value;
        }
    }
    if (maximum == minimum) { minimum = 99.0; maximum = 101.0; }
    if (width >= 10 && app->body_global_date_count > 0U) {
        trainlog_surface_printf(app->content, graph_top, 2, "%.1f", maximum);
        trainlog_surface_printf(app->content, graph_top + graph_height - 1, 2,
            "%.1f", minimum);
        for (metric = 0U; metric < 14U; ++metric) {
            TrainlogGlobalBodySeries *series = &app->body_global_series[metric];
            size_t point;
            int previous_x = -1;
            int previous_y = -1;
            if (!app->body_global_enabled[metric]) continue;
            trainlog_surface_set_role(app->content, series->role,
                TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
            for (point = 0U; point < series->count; ++point) {
                double value;
                size_t date;
                int x;
                int y;
                if (trainlog_body_index100(series->baseline,
                        series->points[point].value,
                        &value) != TRAINLOG_STATUS_OK) continue;
                date = global_date_index(app->body_global_dates,
                    app->body_global_date_count,
                    series->points[point].observed_at);
                x = app->body_global_date_count == 1U ? left + width / 2
                    : left + (int)((date * (size_t)(width - 1)) /
                        (app->body_global_date_count - 1U));
                y = normalized_graph_row(value, minimum, maximum,
                    graph_top, graph_height);
                if (previous_x >= 0)
                    app_shell_surface_segment(app->content, previous_x,
                        previous_y, x, y,
                        (uint32_t)(unsigned char)series->symbol);
                else trainlog_surface_draw(app->content, y, x,
                    (uint32_t)(unsigned char)series->symbol);
                previous_x = x;
                previous_y = y;
            }
        }
    } else trainlog_surface_printf(app->content, graph_top, 2,
        "Aucune série corporelle disponible.");
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    for (metric = 0U; metric < 14U &&
         graph_top + graph_height + 2 + (int)(metric % 7U) <
             app->layout.content.height; ++metric) {
        int row = graph_top + graph_height + 2 + (int)(metric % 7U);
        int column = metric < 7U ? 2 : app->layout.content.width / 2;
        trainlog_surface_printf(app->content, row, column, "%c [%c] %-15s %+.1f%%",
            app->body_global_selected == metric ? '>' : ' ',
            app->body_global_enabled[metric] ? 'x' : ' ',
            BODY_METRICS[metric].label,
            app->body_global_series[metric].latest_percent);
    }
    if (app->body_global_partial)
        trainlog_surface_printf(app->content, app->layout.content.height - 1, 2,
            "Limite locale atteinte : certaines séries peuvent être partielles.");
}

static void app_shell_render_body_trends(TrainlogAppContext *app)
{
    TrainlogDashboardMonthValue monthly[14][DASHBOARD_MONTH_COUNT];
    TrainlogDashboardMonth months[DASHBOARD_MONTH_COUNT];
    double baseline[14] = {0.0};
    double minimum = 0.0;
    double maximum = 0.0;
    long current_key;
    long first_key;
    size_t metric;
    size_t plotted = 0U;
    int top = 5;
    int height = app_shell_graph_height(app, top);
    int left = 8;
    int width = app->layout.content.width - left - 3;
    (void)memset(monthly, 0, sizeof(monthly));
    trainlog_surface_printf(app->content, 3, 2,
        "Évolution corporelle normalisée — 12 mois");
    if (!dashboard_current_month_key(&current_key)) {
        trainlog_surface_printf(app->content, top, 2,
            "Impossible de déterminer le mois courant.");
        return;
    }
    first_key = current_key - (long)(DASHBOARD_MONTH_COUNT - 1U);
    for (metric = 0U; metric < DASHBOARD_MONTH_COUNT; ++metric)
        dashboard_month_from_key(first_key + (long)metric, &months[metric]);
    for (metric = 0U; metric < 14U; ++metric) {
        TrainlogGlobalBodySeries *series = &app->body_global_series[metric];
        size_t point;
        size_t first = 0U;
        size_t visible = 0U;
        bool have_first = false;
        for (point = 0U; point < series->count; ++point) {
            int year;
            int month;
            long offset;
            if (!dashboard_parse_year_month(series->points[point].observed_at,
                    &year, &month)) continue;
            offset = dashboard_month_key(year, month) - first_key;
            if (offset < 0L || offset >= (long)DASHBOARD_MONTH_COUNT) continue;
            monthly[metric][(size_t)offset].present = true;
            monthly[metric][(size_t)offset].value = series->points[point].value;
        }
        for (point = 0U; point < DASHBOARD_MONTH_COUNT; ++point)
            if (monthly[metric][point].present) {
                if (!have_first) { first = point; have_first = true; }
                ++visible;
            }
        if (visible < 2U) continue;
        baseline[metric] = monthly[metric][first].value;
        ++plotted;
        for (point = 0U; point < DASHBOARD_MONTH_COUNT; ++point) {
            double percent;
            if (monthly[metric][point].present &&
                trainlog_body_percent_change(baseline[metric],
                    monthly[metric][point].value,
                    &percent) == TRAINLOG_STATUS_OK) {
                if (percent < minimum) minimum = percent;
                if (percent > maximum) maximum = percent;
            }
        }
    }
    if (plotted == 0U || height < 3 || width < 12) {
        trainlog_surface_printf(app->content, top, 2,
            "Premières courbes après 2 mois relevés pour une même mesure.");
        return;
    }
    if (minimum == maximum) { minimum = -1.0; maximum = 1.0; }
    trainlog_surface_printf(app->content, top, 2, "%+.1f%%", maximum);
    trainlog_surface_printf(app->content, top + height - 1, 2, "%+.1f%%", minimum);
    for (metric = 0U; metric < 14U; ++metric) {
        size_t month;
        int previous_x = -1;
        int previous_y = -1;
        long previous_month = -2L;
        if (baseline[metric] <= 0.0) continue;
        trainlog_surface_set_role(app->content, GLOBAL_BODY_ROLES[metric],
            TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
        for (month = 0U; month < DASHBOARD_MONTH_COUNT; ++month) {
            double percent;
            int x;
            int y;
            if (!monthly[metric][month].present) {
                previous_x = -1; previous_y = -1; previous_month = -2L;
                continue;
            }
            if (trainlog_body_percent_change(baseline[metric],
                    monthly[metric][month].value,
                    &percent) != TRAINLOG_STATUS_OK) continue;
            x = left + (int)((month * (size_t)(width - 1)) /
                (DASHBOARD_MONTH_COUNT - 1U));
            y = normalized_graph_row(percent, minimum, maximum, top, height);
            if (previous_x >= 0 && previous_month + 1L == (long)month)
                app_shell_surface_segment(app->content, previous_x, previous_y,
                    x, y, (uint32_t)(unsigned char)GLOBAL_BODY_SYMBOLS[metric]);
            else trainlog_surface_draw(app->content, y, x,
                (uint32_t)(unsigned char)GLOBAL_BODY_SYMBOLS[metric]);
            previous_x = x; previous_y = y; previous_month = (long)month;
        }
    }
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_MUTED,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    if (top + height < app->layout.content.height)
        trainlog_surface_printf(app->content, top + height, left,
            "%02d/%02d … %02d/%02d", months[0].month,
            months[0].year % 100, months[11].month, months[11].year % 100);
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    if (app->body_global_partial)
        trainlog_surface_printf(app->content, app->layout.content.height - 1, 2,
            "Limite locale atteinte : vue 12 mois potentiellement partielle.");
}

static void app_shell_render_body_analytics(TrainlogAppContext *app)
{
    const TrainlogBodyObservationRecord *latest = app->loaded_count > 0U
        ? &app->body_records[0] : NULL;
    TrainlogBodyAnalyticsResult current;
    TrainlogBodyAnalyticsResult oldest_estimate;
    const TrainlogBodyObservationRecord *oldest_weight =
        body_analytics_oldest_weight(app->body_records, app->loaded_count);
    const TrainlogBodyObservationRecord *oldest_waist =
        body_analytics_oldest_waist(app->body_records, app->loaded_count);
    bool has_current = latest != NULL && trainlog_body_analytics_calculate(
        app->body_has_profile ? &app->body_profile : NULL, latest,
        &current) == TRAINLOG_STATUS_OK;
    bool has_oldest_estimate = app->body_has_profile &&
        body_analytics_oldest_estimate(&app->body_profile, app->body_records,
            app->loaded_count, &oldest_estimate);
    int row = 4;
    if (latest == NULL) {
        trainlog_surface_printf(app->content, row, 2, "Aucun relevé corporel.");
        return;
    }
    trainlog_surface_printf(app->content, row++, 2, "Page %zu/2 · relevé %s",
        app->content_selected + 1U, latest->observed_at);
    trainlog_surface_printf(app->content, row++, 2, "Profil : %s",
        app->body_has_profile
            ? (app->body_profile.formula == TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
                ? "formule femme" : "formule homme")
            : "non configuré");
    if (app->body_has_profile)
        trainlog_surface_printf(app->content, row++, 2, "Taille : %.1f cm",
            app->body_profile.height_cm);
#define BODY_ANALYTIC(label_, has_, value_, suffix_) \
    trainlog_surface_printf(app->content, row++, 2, "%-27s %s%.2f %s", \
        label_, (has_) ? "" : "— ", (has_) ? (value_) : 0.0, suffix_)
    row += 1;
    if (app->content_selected == 0U) {
        BODY_ANALYTIC("Graisse estimée", has_current && current.has_body_fat_estimate,
            current.body_fat_percent, "%");
        BODY_ANALYTIC("Masse grasse estimée", has_current && current.has_fat_mass_estimate,
            current.fat_mass_kg, "kg");
        BODY_ANALYTIC("Masse maigre estimée", has_current && current.has_lean_mass_estimate,
            current.lean_mass_kg, "kg");
        if (latest->has_body_weight)
            trainlog_surface_printf(app->content, row++, 2,
                "Poids %.1f kg · variation %+.1f kg", latest->body_weight_kg,
                oldest_weight != NULL ? latest->body_weight_kg -
                    oldest_weight->body_weight_kg : 0.0);
        if (latest->has_waist)
            trainlog_surface_printf(app->content, row++, 2,
                "Tour de taille %.1f cm · variation %+.1f cm", latest->waist_cm,
                oldest_waist != NULL ? latest->waist_cm -
                    oldest_waist->waist_cm : 0.0);
        if (has_current && current.has_body_fat_estimate && has_oldest_estimate)
            trainlog_surface_printf(app->content, row++, 2,
                "Variation graisse estimée %+.2f point(s)",
                current.body_fat_percent - oldest_estimate.body_fat_percent);
        trainlog_surface_printf(app->content, row + 1, 2,
            "Estimation anthropométrique : tendance, pas mesure directe.");
    } else {
        BODY_ANALYTIC("Taille / hanches", has_current && current.has_waist_hip_ratio,
            current.waist_hip_ratio, "");
        BODY_ANALYTIC("Épaules / taille", has_current && current.has_shoulder_waist_ratio,
            current.shoulder_waist_ratio, "");
        BODY_ANALYTIC("Poitrine / taille", has_current && current.has_chest_waist_ratio,
            current.chest_waist_ratio, "");
        BODY_ANALYTIC("Asymétrie bras", has_current && current.has_arm_asymmetry,
            current.arm_asymmetry_percent, "%");
        BODY_ANALYTIC("Asymétrie avant-bras", has_current && current.has_forearm_asymmetry,
            current.forearm_asymmetry_percent, "%");
        BODY_ANALYTIC("Asymétrie cuisses", has_current && current.has_thigh_asymmetry,
            current.thigh_asymmetry_percent, "%");
        BODY_ANALYTIC("Asymétrie mollets", has_current && current.has_calf_asymmetry,
            current.calf_asymmetry_percent, "%");
    }
#undef BODY_ANALYTIC
}

static void app_shell_render_sync(TrainlogAppContext *app)
{
    TrainlogSyncController *sync = &app->sync_controller;
    size_t index;
    int row = 4;
    if (sync->showing_detail) {
        trainlog_surface_printf(app->content, row++, 2, "Détail %s",
            sync->history[sync->selected].sync_id);
        for (index = sync->detail_scroll; index < sync->detail_line_count &&
             row < app->layout.content.height; ++index)
            trainlog_surface_printf(app->content, row++, 2, "%.*s",
                app->layout.content.width - 4, sync->detail_lines[index]);
        return;
    }
    if (!sync->probe_known)
        trainlog_surface_printf(app->content, row++, 2,
            "État appareil non actualisé · r pour rechercher.");
    else if (sync->probe_status == TRAINLOG_STATUS_OK && sync->device.connected) {
        trainlog_surface_printf(app->content, row++, 2, "Appareil : %s %s",
            sync->device.device.vendor, sync->device.device.model);
        trainlog_surface_printf(app->content, row++, 2,
            "Stockage : %.2f GiB libres / %.2f GiB",
            sync_bytes_to_gib(sync->device.storage.free_space_bytes),
            sync_bytes_to_gib(sync->device.storage.max_capacity_bytes));
    } else trainlog_surface_printf(app->content, row++, 2,
        sync->probe_status == TRAINLOG_STATUS_CONFLICT
            ? "Service de synchronisation occupé."
            : "Aucun appareil MTP Trainlog détecté.");
    if (sync->running) {
        trainlog_surface_printf(app->content, row + 1, 2,
            "Synchronisation en cours… PC↔Android");
        trainlog_surface_printf(app->content, row + 3, 2,
            "Une seule exécution peut être active.");
        return;
    }
    if (sync->action.confirming) {
        trainlog_surface_printf(app->content, row + 1, 2, "%s",
            trainlog_sync_screen_confirmation(sync->action.direction));
        trainlog_surface_printf(app->content, row + 3, 2,
            "Entrée confirme une exécution · Échap annule");
        return;
    }
    if (sync->has_report) {
        trainlog_surface_printf(app->content, row++, 2, "%s",
            sync->report.success ? sync->report.summary :
            sync->report.error[0] != '\0' ? sync->report.error
                                           : "Synchronisation échouée.");
    }
    row += 1;
    trainlog_surface_printf(app->content, row++, 2,
        "Historique des synchronisations");
    if (sync->history_count == 0U)
        trainlog_surface_printf(app->content, row, 2,
            "Aucune synchronisation enregistrée.");
    for (index = 0U; index < sync->history_count &&
         row < app->layout.content.height; ++index) {
        const TrainlogSyncHistoryEntry *entry = &sync->history[index];
        /* CONTRACT: legacy journal directions remain parseable internal
         * metadata, but the TUI exposes one PC↔Android sync operation. Old
         * directional summaries used the same forbidden mode labels. */
        const char *summary = entry->direction_known &&
            entry->direction != TRAINLOG_SYNC_BIDIRECTIONAL
                ? "entrée historique" : entry->summary;
        trainlog_surface_printf(app->content, row++, 2, "%c %-16s %c %-17.17s %.*s",
            index == sync->selected ? '>' : ' ', entry->timestamp,
            entry->success ? '+' : '!',
            "PC↔Android",
            app->layout.content.width > 45 ? app->layout.content.width - 45 : 8,
            summary);
    }
}

static void app_shell_render_content(TrainlogAppContext *app)
{
    TrainlogAppRoute route = app->navigation.current.route;
    int width = app->layout.content.width;
    /* CONTRACT: renderers consume controller-owned immutable snapshots and
     * write coordinates relative to this persistent content plane. SQL,
     * device discovery and sync execution stay in explicit controller actions. */
    trainlog_surface_erase(app->content);
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_ACCENT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_BOLD);
    trainlog_surface_printf(app->content, 1, 2, "%s", trainlog_route_title(route));
    trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    if (app->body_controller.phase != TRAINLOG_BODY_IDLE) {
        app_shell_render_body_controller(app);
    } else if (app->profile_controller.phase != TRAINLOG_PROFILE_IDLE) {
        app_shell_render_profile_controller(app);
    } else if (app->exercise_controller.phase != TRAINLOG_EXERCISE_IDLE) {
        app_shell_render_exercise_controller(app);
    } else if (app->equipment_controller.phase != TRAINLOG_EQUIPMENT_IDLE) {
        app_shell_render_equipment_controller(app);
    } else if (route == TRAINLOG_ROUTE_SESSION_MANUAL ||
        route == TRAINLOG_ROUTE_SESSION_CURRENT ||
        route == TRAINLOG_ROUTE_SESSION_GENERATOR) {
        app_shell_render_session_controller(app);
    } else if (route == TRAINLOG_ROUTE_HOME) {
        trainlog_surface_printf(app->content, 4, 2, "Votre entraînement, au même endroit.");
        trainlog_surface_printf(app->content, 7, 2, "1  Reprendre ou commencer une séance");
        trainlog_surface_printf(app->content, 9, 2, "5  Ajouter ou consulter des mensurations");
    } else if (route == TRAINLOG_ROUTE_SESSIONS) {
        static const char *const rows[] = {"Séance en cours", "Nouvelle séance manuelle",
            "Séances effectuées"};
        size_t index;
        for (index = 0U; index < 3U; ++index) {
            trainlog_surface_set_role(app->content,
                app->content_selected == index ? TRAINLOG_COLOR_ACCENT : TRAINLOG_COLOR_DEFAULT,
                TRAINLOG_RGB_BASE,
                app->content_selected == index ? TRAINLOG_TEXT_BOLD : TRAINLOG_TEXT_NORMAL);
            trainlog_surface_printf(app->content, 4 + (int)index * 2, 2,
                "%s %s", app->content_selected == index ? "▶ [SÉLECTION]" : "  ", rows[index]);
        }
        trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
            TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    } else if (route == TRAINLOG_ROUTE_STATS) {
        static const char *const rows[] = {"Par exercice", "Mensurations", "Capacités / MAX"};
        size_t index;
        for (index = 0U; index < 3U; ++index) {
            trainlog_surface_set_role(app->content,
                app->content_selected == index ? TRAINLOG_COLOR_ACCENT : TRAINLOG_COLOR_DEFAULT,
                TRAINLOG_RGB_BASE,
                app->content_selected == index ? TRAINLOG_TEXT_BOLD : TRAINLOG_TEXT_NORMAL);
            trainlog_surface_printf(app->content, 4 + (int)index * 2, 2,
                "%s %s", app->content_selected == index ? "▶ [SÉLECTION]" : "  ", rows[index]);
        }
        trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
            TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
    } else if (route == TRAINLOG_ROUTE_SESSION_DETAIL) {
        int row = 3;
        if (app->session_detail_error) trainlog_surface_printf(app->content, row, 2,
            "Impossible de charger la séance.");
        else {
            TrainlogPersistedExerciseDetail *entry = app->session_entry_count > 0U
                ? &app->session_entries[app->session_entry_selected] : NULL;
            trainlog_surface_printf(app->content, row++, 2, "Début : %s",
                app->session_detail.started_at);
            trainlog_surface_printf(app->content, row++, 2, "Fin : %s",
                app->session_detail.ended_at[0] != '\0'
                    ? app->session_detail.ended_at : "séance ouverte");
            trainlog_surface_printf(app->content, row++, 2, "Type : %s",
                session_type_label(app->session_detail.session_type));
            row += 1;
            if (entry == NULL) trainlog_surface_printf(app->content, row, 2,
                "Aucun exercice dans cette séance.");
            else {
                char duration[64] = "";
                trainlog_surface_printf(app->content, row++, 2,
                    "Exercice %zu/%zu — %s", app->session_entry_selected + 1U,
                    app->session_entry_count, entry->name);
                trainlog_surface_printf(app->content, row++, 2,
                    "Occurrence : %s", entry->entry_id);
                if (entry->equipment_id[0] != '\0')
                    trainlog_surface_printf(app->content, row++, 2,
                        "Équipement : %s  (i pour la fiche)", entry->equipment_id);
                if (entry->has_max_weight != 0)
                    trainlog_surface_printf(app->content, row++, 2,
                        "MAX réellement mesuré : %.2f kg", entry->max_weight_kg);
                else if (entry->recording_mode == TRAINLOG_RECORDING_CONTINUOUS) {
                    (void)trainlog_duration_format(entry->continuous_duration_seconds,
                        duration, sizeof(duration));
                    trainlog_surface_printf(app->content, row++, 2,
                        "Réalisé : activité continue · %s", duration);
                    if (entry->has_continuous_speed != 0)
                        trainlog_surface_printf(app->content, row++, 2,
                            "Vitesse : %.1f km/h", entry->continuous_speed_kmh);
                    if (entry->has_continuous_distance != 0)
                        trainlog_surface_printf(app->content, row++, 2,
                            "Distance : %.2f km", entry->continuous_distance_km);
                } else {
                    size_t index;
                    trainlog_surface_printf(app->content, row++, 2,
                        "Plan : %d série(s) · repos %d s%s", entry->target_sets,
                        entry->rest_seconds, entry->has_target_weight != 0
                            ? " · charge/assistance cible définie" : "");
                    trainlog_surface_printf(app->content, row++, 2,
                        "Réalisé : %zu série(s)", entry->actual_set_count);
                    for (index = app->session_set_scroll;
                         index < entry->actual_set_count && row < app->layout.content.height;
                         ++index) {
                        char metric[64]; char weight[48];
                        if (entry->tracking_mode == TRAINLOG_TRACKING_DURATION)
                            (void)trainlog_duration_format(entry->actual_sets[index].duration_seconds,
                                metric, sizeof(metric));
                        else (void)snprintf(metric, sizeof(metric), "%d reps",
                            entry->actual_sets[index].reps);
                        if (entry->actual_sets[index].has_weight)
                            (void)snprintf(weight, sizeof(weight), "%.2f kg",
                                entry->actual_sets[index].weight_kg);
                        else (void)snprintf(weight, sizeof(weight), "—");
                        trainlog_surface_printf(app->content, row++, 4,
                            "%zu. %-18s  %s", index + 1U, metric, weight);
                    }
                }
            }
        }
    } else if (route == TRAINLOG_ROUTE_EQUIPMENT_DETAIL) {
        const TrainlogResolvedEquipment *item = &app->equipment_detail;
        trainlog_surface_printf(app->content, 4, 2, "%s", item->display_name);
        trainlog_surface_printf(app->content, 6, 2, "Identifiant : %s", item->equipment_id);
        trainlog_surface_printf(app->content, 7, 2, "Étiquette : %s",
            item->label_name[0] != '\0' ? item->label_name : "—");
        trainlog_surface_printf(app->content, 8, 2, "Type : %s",
            item->equipment_type[0] != '\0' ? item->equipment_type : "—");
        trainlog_surface_printf(app->content, 9, 2, "Charge : %s",
            item->load_semantics[0] != '\0' ? item->load_semantics : "—");
        trainlog_surface_printf(app->content, 10, 2, "Origine : %s",
            equipment_origin_label(item->origin));
    } else if (route == TRAINLOG_ROUTE_EXERCISE_DETAIL) {
        const TrainlogExercise *item = &app->exercise_detail;
        const TrainlogBodyZone *primary = NULL;
        const TrainlogBodyZone *group = NULL;
        char secondary[256] = "";
        size_t index;
        int row = 4;
        for (index = 0U; index < app->exercise_zone_count; ++index) {
            const TrainlogBodyZone *zone = trainlog_body_zone_catalog_lookup(
                app->exercise_zones[index].zone_id);
            if (app->exercise_zones[index].role == TRAINLOG_BODY_ZONE_PRIMARY)
                primary = zone;
            else if (zone != NULL) {
                size_t used = strlen(secondary);
                if (used < sizeof(secondary) - 1U)
                    (void)snprintf(secondary + used, sizeof(secondary) - used,
                        "%s%s", used > 0U ? ", " : "", zone->display_name);
            }
        }
        if (primary != NULL && primary->parent_zone_id != NULL)
            group = trainlog_body_zone_catalog_lookup(primary->parent_zone_id);
        trainlog_surface_printf(app->content, row++, 2, "%s", item->name);
        trainlog_surface_printf(app->content, row++, 2, "Identifiant : %s", item->exercise_id);
        trainlog_surface_printf(app->content, row++, 2, "Suivi : %s · organisation : %s",
            item->tracking_mode == TRAINLOG_TRACKING_REPS ? "répétitions" : "durée",
            item->recording_mode == TRAINLOG_RECORDING_CONTINUOUS ? "continue" : "séries");
        if (app->exercise_detail_metadata_error)
            trainlog_surface_printf(app->content, row++, 2,
                "Métadonnées de zones/usage partiellement indisponibles.");
        else {
            trainlog_surface_printf(app->content, row++, 2,
                "Zone principale : %s", primary != NULL
                    ? primary->display_name : "Non renseignée");
            trainlog_surface_printf(app->content, row++, 2,
                "Secondaires : %s", secondary[0] != '\0' ? secondary : "Aucune");
            trainlog_surface_printf(app->content, row++, 2,
                "Groupe : %s", group != NULL ? group->display_name : "Aucun");
        }
        trainlog_surface_printf(app->content, row++, 2,
            "Compatibles (manifeste) : %zu · utilisés historiquement : %zu",
            app->exercise_explicit_equipment_count,
            app->exercise_historic_equipment_count);
        for (index = 0U; index < app->exercise_explicit_equipment_count &&
             row < app->layout.content.height - 2; ++index)
            trainlog_surface_printf(app->content, row++, 4, "%c %.48s [compatible]",
                app->exercise_equipment_selected == index ? '>' : ' ',
                app->exercise_explicit_equipment[index].display_name);
        for (index = 0U; index < app->exercise_historic_equipment_count &&
             row < app->layout.content.height - 2; ++index) {
            size_t absolute = app->exercise_explicit_equipment_count + index;
            trainlog_surface_printf(app->content, row++, 4, "%c %.48s [usage]",
                app->exercise_equipment_selected == absolute ? '>' : ' ',
                app->exercise_historic_equipment[index].display_name);
        }
        trainlog_surface_printf(app->content, app->layout.content.height - 2, 2,
            "Entrée fiche équipement · p Performances · m MAX · k Connaissances · e Modifier");
    } else if (route == TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE) {
        const TrainlogExerciseKnowledge *record = trainlog_exercise_knowledge_lookup(
            app->exercise_detail.exercise_id);
        const TrainlogKnowledgeInterpretation *value = record == NULL ? NULL
            : record->interpretation != NULL ? record->interpretation
            : record->conditional_interpretation;
        char lines[KNOWLEDGE_LINES_MAX][KNOWLEDGE_LINE_MAX];
        size_t count = 0U;
        size_t index;
        int wrap = app->layout.content.width - 6;
        knowledge_add_wrapped(lines, &count, app->exercise_detail.name, wrap);
        if (record == NULL || value == NULL)
            knowledge_add_wrapped(lines, &count, record == NULL
                ? "Aucune fiche scientifique pour cet identifiant."
                : "Interprétation scientifique non résolue.", wrap);
        else {
            knowledge_add_wrapped(lines, &count,
                record->interpretation != NULL ? "Connaissances validées"
                : "Interprétation conditionnelle — à confirmer", wrap);
            knowledge_add_ids(lines, &count, "Mouvement :", value->pattern_ids, false, wrap);
            knowledge_add_wrapped(lines, &count, "Confiance :", wrap);
            knowledge_add_wrapped(lines, &count, value->confidence, wrap);
            knowledge_add_ids(lines, &count, "Muscles principaux :",
                value->primary_muscle_ids, true, wrap);
            knowledge_add_ids(lines, &count, "Secondaires :",
                value->secondary_muscle_ids, true, wrap);
            knowledge_add_ids(lines, &count, "Stabilisateurs :",
                value->stabilizer_muscle_ids, true, wrap);
            knowledge_add_zones(lines, &count, value, wrap);
            knowledge_add_plain_ids(lines, &count, "Sources :", value->source_refs, wrap);
        }
        if (app->content_scroll >= count)
            app->content_scroll = count > 0U ? count - 1U : 0U;
        for (index = app->content_scroll; index < count &&
             3 + (int)(index - app->content_scroll) < app->layout.content.height; ++index)
            trainlog_surface_printf(app->content,
                3 + (int)(index - app->content_scroll), 2, "%s", lines[index]);
    } else if (route == TRAINLOG_ROUTE_EXERCISE_PERFORMANCE) {
        const TrainlogExercisePerformancePoint *latest = NULL;
        const TrainlogExercisePerformancePoint *best = NULL;
        TrainlogLoadMode mode = TRAINLOG_LOAD_NONE;
        size_t index;
        int row = 4;
        char latest_text[128];
        char best_text[128];
        if (app->performance_error) {
            trainlog_surface_printf(app->content, row, 2,
                "Impossible de lire l’historique.");
        } else {
            for (index = 0U; index < app->performance_count; ++index)
                if (app->performance_points[index].has_performance != 0) {
                    latest = &app->performance_points[index]; mode = latest->load_mode; break;
                }
            for (index = 0U; latest != NULL && index < app->performance_count; ++index)
                if (app->performance_points[index].has_performance != 0 &&
                    app->performance_points[index].load_mode == mode &&
                    exercise_point_better(&app->performance_points[index], best))
                    best = &app->performance_points[index];
            exercise_format_performance(latest, latest_text, sizeof(latest_text));
            exercise_format_performance(best, best_text, sizeof(best_text));
            trainlog_surface_printf(app->content, row++, 2, "%s",
                app->exercise_detail.name);
            trainlog_surface_printf(app->content, row++, 2, "Séances enregistrées : %zu",
                app->performance_count);
            trainlog_surface_printf(app->content, row++, 2, "Dernier meilleur set : %s",
                latest != NULL ? latest_text : "aucun");
            trainlog_surface_printf(app->content, row++, 2, "Meilleur set enregistré : %s",
                best != NULL ? best_text : "aucun");
            if (latest != NULL) {
                int graph_top = row + 1;
                int graph_height = app_shell_graph_height(app, graph_top);
                app_shell_draw_performance_graph(app, mode,
                    app->exercise_detail.tracking_mode, graph_top,
                    graph_height, false);
                if (graph_height >= 3) row = graph_top + graph_height + 2;
                else row += 2;
            } else row += 2;
            for (index = app->content_scroll; index < app->performance_count &&
                 row < app->layout.content.height; ++index) {
                char date[11]; char summary[128];
                exercise_short_date(app->performance_points[index].started_at, date);
                exercise_format_performance(&app->performance_points[index], summary,
                    sizeof(summary));
                trainlog_surface_printf(app->content, row++, 2, "%s  %-13s  %s", date,
                    exercise_load_mode_label(app->performance_points[index].load_mode), summary);
            }
        }
    } else if (route == TRAINLOG_ROUTE_EXERCISE_MAX) {
        static const double increments[] = {0.5, 1.0, 2.5, 5.0};
        const TrainlogMeasuredMaxSummary *summary = &app->max_summary;
        int row = 4;
        trainlog_surface_printf(app->content, row++, 2, "%s", app->exercise_detail.name);
        if (app->performance_error) trainlog_surface_printf(app->content, row, 2,
            "Impossible de lire les tests de MAX.");
        else if (!summary->found) {
            trainlog_surface_printf(app->content, row++, 2,
                "Aucun MAX mesuré réussi.");
            trainlog_surface_printf(app->content, row, 2,
                "Seules les séances explicitement « Test de max » comptent.");
        } else {
            char current[128]; char record[128];
            exercise_format_performance(&summary->current, current, sizeof(current));
            exercise_format_performance(&summary->record, record, sizeof(record));
            trainlog_surface_printf(app->content, row++, 2, "Tests MAX : %zu · réussis : %zu",
                summary->test_count, summary->successful_test_count);
            trainlog_surface_printf(app->content, row++, 2, "Actuel : %s", current);
            trainlog_surface_printf(app->content, row++, 2, "Record même mode : %s", record);
            if (summary->current.load_mode == TRAINLOG_LOAD_EXTERNAL) {
                static const double percentages[] = {60.0, 70.0, 80.0, 90.0};
                double working[4]; size_t index; bool valid = true;
                for (index = 0U; index < 4U; ++index)
                    if (trainlog_measured_max_working_load(&summary->current,
                        percentages[index], increments[app->max_rounding_index],
                        &working[index]) != TRAINLOG_STATUS_OK) valid = false;
                if (valid) trainlog_surface_printf(app->content, row++, 2,
                    "Travail : 60%% %.1f · 70%% %.1f · 80%% %.1f · 90%% %.1f kg",
                    working[0], working[1], working[2], working[3]);
                trainlog_surface_printf(app->content, row++, 2,
                    "Arrondi %.1f kg · aucun 1RM estimé",
                    increments[app->max_rounding_index]);
            } else trainlog_surface_printf(app->content, row++, 2,
                summary->current.load_mode == TRAINLOG_LOAD_ASSISTANCE
                    ? "Assistance : moins de kg = mieux · pourcentages non applicables"
                    : "Sans charge externe · pourcentages non applicables");
            {
                int graph_top = row + 1;
                int graph_height = app_shell_graph_height(app, graph_top);
                app_shell_draw_performance_graph(app, summary->current.load_mode,
                    app->exercise_detail.tracking_mode, graph_top,
                    graph_height, true);
            }
        }
    } else if (route == TRAINLOG_ROUTE_BODY_DETAIL) {
        app_shell_render_body_detail(app);
    } else if (route == TRAINLOG_ROUTE_BODY_METRIC) {
        int graph_height = app_shell_graph_height(app, 6);
        trainlog_surface_printf(app->content, 4, 2, "%s — ←/→ change la mesure",
            BODY_METRICS[app->body_metric_selected].label);
        if (app->body_snapshot_error)
            trainlog_surface_printf(app->content, 6, 2,
                "Impossible de lire cet historique.");
        else app_shell_draw_body_points(app, app->body_metric_points,
            app->body_metric_count, 6, graph_height,
            BODY_METRICS[app->body_metric_selected].unit);
        if (app->body_metric_partial)
            trainlog_surface_printf(app->content,
                app->layout.content.height - 1, 2,
                "Limite locale atteinte : historique partiel.");
    } else if (route == TRAINLOG_ROUTE_BODY_TRENDS) {
        app_shell_render_body_trends(app);
    } else if (route == TRAINLOG_ROUTE_BODY_GLOBAL) {
        app_shell_render_body_global(app);
    } else if (route == TRAINLOG_ROUTE_BODY_ANALYTICS) {
        app_shell_render_body_analytics(app);
    } else if (route == TRAINLOG_ROUTE_SYNC) {
        app_shell_render_sync(app);
    } else if (app_shell_is_list_route(route)) {
        size_t index;
        int row = 5;
        trainlog_surface_printf(app->content, 3, 2, "Recherche : %s%s",
            app->search.bytes > 0U ? app->search.text : "—",
            app->search.focused ? "  [saisie]" : "");
        if (app->list_error) trainlog_surface_printf(app->content, row++, 2,
            "Impossible de charger cette liste.");
        else if (app->loaded_count == 0U) trainlog_surface_printf(app->content, row++, 2,
            "Aucun résultat.");
        for (index = app->list.viewport_start; index < app->loaded_count &&
             index < app->list.viewport_start + app->list.visible_rows; ++index) {
            char secondary[48] = "";
            char session_line[80] = "";
            char body_line[160] = "";
            char clipped[TRAINLOG_NAME_MAX + 8U];
            const char *name;
            int label_cells = app->layout.content.width - 28;
            if (route == TRAINLOG_ROUTE_EQUIPMENT) {
                name = app->equipment[index].display_name;
                (void)snprintf(secondary, sizeof(secondary), "%s",
                    equipment_origin_label(app->equipment[index].origin));
            } else if (route == TRAINLOG_ROUTE_SESSIONS_COMPLETED) {
                char date[17];
                session_history_datetime(app->sessions[index].started_at, date);
                (void)snprintf(session_line, sizeof(session_line),
                    "%s  %s", date,
                    session_type_history_label(app->sessions[index].session_type));
                name = session_line;
                (void)snprintf(secondary, sizeof(secondary), "%zu exercice(s)",
                    app->sessions[index].exercise_count);
            } else if (route == TRAINLOG_ROUTE_BODY) {
                char date[9];
                body_short_date(app->body_records[index].observed_at, date);
                body_summary_text(&app->body_records[index], body_line,
                    sizeof(body_line));
                name = body_line;
                (void)snprintf(secondary, sizeof(secondary), "%s", date);
            } else {
                name = app->exercises[index].name;
                (void)snprintf(secondary, sizeof(secondary), "%s",
                    app->exercises[index].tracking_mode == TRAINLOG_TRACKING_REPS
                        ? "reps" : "durée");
            }
            if (label_cells < 12) label_cells = 12;
            if (label_cells > 48) label_cells = 48;
            trainlog_shell_format_list_label(name, label_cells, clipped,
                sizeof(clipped));
            trainlog_surface_set_role(app->content,
                index == app->list.selected_index ? TRAINLOG_COLOR_ACCENT : TRAINLOG_COLOR_DEFAULT,
                TRAINLOG_RGB_BASE,
                index == app->list.selected_index ? TRAINLOG_TEXT_BOLD : TRAINLOG_TEXT_NORMAL);
            trainlog_surface_printf(app->content, row++, 2, "%s %-*s  %s",
                index == app->list.selected_index ? "> [SÉLECTION]" : " ",
                label_cells, clipped, secondary);
        }
        trainlog_surface_set_role(app->content, TRAINLOG_COLOR_DEFAULT,
            TRAINLOG_RGB_BASE, TRAINLOG_TEXT_NORMAL);
        if (app->loaded_count > 0U) {
            if (app->list.total_known)
                trainlog_surface_printf(app->content,
                    app->layout.content.height - 2, 2, "Position %zu/%zu",
                    app->list.selected_index + 1U, app->loaded_count);
            else trainlog_surface_printf(app->content,
                app->layout.content.height - 2, 2,
                "Position %zu/%zu+ · limite locale atteinte",
                app->list.selected_index + 1U, app->loaded_count);
        }
    } else if (route == TRAINLOG_ROUTE_SETTINGS) {
        trainlog_surface_printf(app->content, 4, 2, "Profil d’estimation corporelle");
        if (app->body_has_profile)
            trainlog_surface_printf(app->content, 6, 2, "%s · %.1f cm",
                app->body_profile.formula == TRAINLOG_BODY_ANALYTICS_FORMULA_FEMALE
                    ? "Formule femme" : "Formule homme",
                app->body_profile.height_cm);
        else trainlog_surface_printf(app->content, 6, 2,
            "Profil non configuré.");
        trainlog_surface_printf(app->content, 8, 2,
            "Entrée pour consulter ou modifier le profil.");
    } else {
        trainlog_surface_printf(app->content, 4, 2,
            "Entrée ouvre la vue complète existante dans ce contexte.");
        if (width < 80) trainlog_surface_printf(app->content, 6, 2,
            "Mode compact · F6 ouvre Navigation");
    }
}

static void app_shell_render_overlay(TrainlogAppContext *app)
{
    const TrainlogOverlay *overlay = trainlog_overlays_top(&app->overlays);
    TrainlogRect rect;
    TrainlogSurface *surface;
    size_t index;
    if (overlay == NULL) return;
    rect = trainlog_shell_overlay_rect(&app->layout,
        overlay->type == TRAINLOG_OVERLAY_NAVIGATION ? 36 : 72,
        overlay->type == TRAINLOG_OVERLAY_HELP ? 14 :
        overlay->type == TRAINLOG_OVERLAY_EXERCISE_MERGE ? 16 : 12);
    surface = trainlog_surface_create(app->terminal, "trainlog.overlay",
        rect.y, rect.x, rect.height, rect.width);
    if (surface == NULL) return;
    trainlog_surface_set_role(surface, TRAINLOG_COLOR_DEFAULT,
        TRAINLOG_RGB_SURFACE0, TRAINLOG_TEXT_NORMAL);
    if (overlay->type == TRAINLOG_OVERLAY_NAVIGATION) {
        trainlog_surface_printf(surface, 1, 2, "Navigation");
        for (index = 0U; index < sizeof(shell_sections) / sizeof(shell_sections[0]) &&
             3 + (int)index < rect.height; ++index)
            trainlog_surface_printf(surface, 3 + (int)index, 2, "%c %s",
                overlay->selected == index ? '>' : ' ',
                shell_section_labels[index]);
    } else if (overlay->type == TRAINLOG_OVERLAY_ACTIONS) {
        size_t visible = rect.height > 4 ? (size_t)(rect.height - 4) : 1U;
        size_t start = overlay->selected >= visible
            ? overlay->selected - visible + 1U : 0U;
        trainlog_surface_printf(surface, 1, 2, "Actions disponibles");
        for (index = start; index < app->actions.count &&
             index < start + visible; ++index)
            trainlog_surface_printf(surface, 3 + (int)(index - start), 2, "%c %s",
                overlay->selected == index ? '>' : ' ', app->actions.items[index].label);
        if (app->actions.count > visible)
            trainlog_surface_printf(surface, rect.height - 1, 2,
                "Action %zu/%zu%s%s", overlay->selected + 1U,
                app->actions.count, start > 0U ? " · ↑" : "",
                start + visible < app->actions.count ? " · ↓" : "");
    } else if (overlay->type == TRAINLOG_OVERLAY_EXERCISE_MERGE) {
        TrainlogExerciseController *controller = &app->exercise_controller;
        if (controller->phase == TRAINLOG_EXERCISE_MERGE_PICKER) {
            size_t matches = exercise_merge_match_count(controller);
            size_t visible = rect.height > 7 ? (size_t)(rect.height - 7) : 1U;
            size_t start = controller->merge_selected >= visible
                ? controller->merge_selected - visible + 1U : 0U;
            trainlog_surface_printf(surface, 1, 2, "Fusionner la source : %.48s",
                controller->merge_source.name);
            trainlog_surface_printf(surface, 3, 2, "Recherche cible : %s_",
                controller->form.text);
            for (index = start; index < matches && index < start + visible; ++index) {
                TrainlogExercise *candidate = exercise_merge_match_at(controller, index);
                if (candidate != NULL)
                    trainlog_surface_printf(surface, 5 + (int)(index - start), 2,
                        "%c %.58s", controller->merge_selected == index ? '>' : ' ',
                        candidate->name);
            }
            if (matches == 0U) trainlog_surface_printf(surface, 5, 2,
                "Aucune cible correspondante.");
            trainlog_surface_printf(surface, rect.height - 1, 2,
                "Entrée prévisualiser · Échap annuler");
        } else if (controller->phase == TRAINLOG_EXERCISE_MERGE_CONFIRM) {
            trainlog_surface_printf(surface, 1, 2, "Confirmer la fusion");
            trainlog_surface_printf(surface, 3, 2, "SOURCE : %.54s",
                controller->merge_source.name);
            trainlog_surface_printf(surface, 4, 2, "   vers CANONIQUE : %.45s",
                controller->merge_target.name);
            trainlog_surface_printf(surface, 6, 2,
                "Impact source : %zu occurrence(s), %zu série(s), %zu continu, %zu MAX.",
                controller->merge_preview.occurrences,
                controller->merge_preview.performed_sets,
                controller->merge_preview.continuous_activities,
                controller->merge_preview.max_results);
            trainlog_surface_printf(surface, 7, 2,
                "Associations : %zu équipement(s), %zu zone(s).",
                controller->merge_preview.associated_equipment,
                controller->merge_preview.body_zones);
            trainlog_surface_printf(surface, 9, 2, "%c Annuler",
                overlay->selected == 0U ? '>' : ' ');
            trainlog_surface_printf(surface, 10, 2, "%c Fusionner source vers canonique",
                overlay->selected == 1U ? '>' : ' ');
        } else {
            trainlog_surface_printf(surface, 1, 2, "Résultat de la fusion");
            trainlog_surface_printf(surface, 3, 2, "%.64s", controller->message);
            trainlog_surface_printf(surface, 6, 2, "Entrée fermer");
        }
    } else if (overlay->type == TRAINLOG_OVERLAY_CONFIRMATION &&
               app->quit_confirmation) {
        trainlog_surface_printf(surface, 1, 2, "Quitter TRAINLOG ?");
        trainlog_surface_printf(surface, 3, 2,
            "Le brouillon et les propositions gardés seulement pour ce run seront perdus.");
        trainlog_surface_printf(surface, 5, 2, "%c Annuler",
            overlay->selected == 0U ? '>' : ' ');
        trainlog_surface_printf(surface, 6, 2, "%c Quitter et perdre cet état",
            overlay->selected == 1U ? '>' : ' ');
    } else {
        trainlog_surface_printf(surface, 1, 2, "Aide contextuelle");
        trainlog_surface_printf(surface, 3, 2, "0/Home Accueil · 1/F1 Séance · 2/F2 Effectuées");
        trainlog_surface_printf(surface, 5, 2, "3/F3 Exercices · 4/F4 Équipements · 5/F5 Mensurations");
        trainlog_surface_printf(surface, 7, 2, "6 Synchronisation · F6 Navigation · F7 Actions");
        trainlog_surface_printf(surface, 9, 2, "Tab change le focus · Échap revient");
    }
    trainlog_surface_move_top(surface);
    app_shell_render_footer(app);
    trainlog_terminal_render(app->terminal);
    trainlog_surface_destroy(surface);
}

static void app_shell_render(TrainlogAppContext *app)
{
    if (!app_shell_layout(app)) { app->running = false; return; }
    if (!app->layout.usable) {
        trainlog_terminal_erase(app->terminal);
        trainlog_terminal_printf(app->terminal, 1, 2,
            "Terminal trop petit — %dx%d, minimum 72x20.",
            app->layout.columns, app->layout.rows);
        trainlog_terminal_printf(app->terminal, 3, 2, "q pour quitter");
        trainlog_terminal_render(app->terminal); return;
    }
    trainlog_surface_erase(app->header);
    trainlog_surface_set_role(app->header, TRAINLOG_COLOR_ACCENT,
        TRAINLOG_RGB_MANTLE, TRAINLOG_TEXT_BOLD);
    trainlog_surface_printf(app->header, 0, 2, "TRAINLOG");
    trainlog_surface_set_role(app->header, TRAINLOG_COLOR_MUTED,
        TRAINLOG_RGB_MANTLE, TRAINLOG_TEXT_NORMAL);
    trainlog_surface_printf(app->header, 1, 2, "%s", trainlog_route_title(app->navigation.current.route));
    if (!app->layout.sidebar_visible) {
        int controls_x = app->layout.columns > 35 ? app->layout.columns - 34 : 2;
        trainlog_surface_printf(app->header, 0, controls_x, "%c F6 Navigation",
            app->focus == TRAINLOG_FOCUS_NAVIGATION ? '>' : ' ');
        trainlog_surface_printf(app->header, 1, controls_x, "%c F7 Actions",
            app->focus == TRAINLOG_FOCUS_ACTIONS ? '>' : ' ');
    }
    app_shell_render_sidebar(app); app_shell_render_content(app);
    app_shell_render_footer(app);
    trainlog_surface_move_top(app->footer);
    trainlog_terminal_render(app->terminal);
    if (trainlog_overlays_top(&app->overlays) != NULL) app_shell_render_overlay(app);
}

static void app_shell_open_route(TrainlogAppContext *app, TrainlogAppRoute route)
{
    TrainlogAppRoute previous = app->navigation.current.route;
    TrainlogSessionController *session = &app->session;
    if (route != previous &&
        app->exercise_controller.phase != TRAINLOG_EXERCISE_IDLE &&
        app->exercise_controller.phase != TRAINLOG_EXERCISE_MESSAGE &&
        app->exercise_controller.phase != TRAINLOG_EXERCISE_CONFIRM_DISCARD) {
        /* CONTRACT: selecting a destination never silently replaces the one
         * transient exercise editor. Raw fields remain owned until discard. */
        app->exercise_controller.return_phase = app->exercise_controller.phase;
        app->exercise_controller.pending_route = route;
        app->exercise_controller.leaving = true;
        app->exercise_controller.phase = TRAINLOG_EXERCISE_CONFIRM_DISCARD;
        return;
    }
    if (route != previous &&
        app->equipment_controller.phase != TRAINLOG_EQUIPMENT_IDLE &&
        app->equipment_controller.phase != TRAINLOG_EQUIPMENT_MESSAGE &&
        app->equipment_controller.phase != TRAINLOG_EQUIPMENT_CONFIRM_DISCARD) {
        app->equipment_controller.return_phase = app->equipment_controller.phase;
        app->equipment_controller.pending_route = route;
        app->equipment_controller.leaving = true;
        app->equipment_controller.phase = TRAINLOG_EQUIPMENT_CONFIRM_DISCARD;
        return;
    }
    if (route != previous && app->body_controller.phase == TRAINLOG_BODY_FORM) {
        app->body_controller.pending_route = route;
        app->body_controller.leaving = true;
        app->body_controller.phase = TRAINLOG_BODY_CONFIRM_DISCARD;
        return;
    }
    if (route != previous &&
        (app->profile_controller.phase == TRAINLOG_PROFILE_FORMULA ||
         app->profile_controller.phase == TRAINLOG_PROFILE_HEIGHT)) {
        app->profile_controller.pending_route = route;
        app->profile_controller.leaving = true;
        app->profile_controller.return_phase = app->profile_controller.phase;
        app->profile_controller.phase = TRAINLOG_PROFILE_CONFIRM_DISCARD;
        return;
    }
    if (session->form.active && route != previous) {
        (void)snprintf(session->message, sizeof(session->message),
            "Terminez ou annulez le champ actif avant de changer de rubrique.");
        return;
    }
    if (previous == TRAINLOG_ROUTE_SESSION_GENERATOR &&
        route != TRAINLOG_ROUTE_SESSION_GENERATOR &&
        (session->generated_preview || session->generation_zone != NULL) &&
        session->phase != TRAINLOG_SESSION_CONFIRM_LEAVE) {
        session->return_phase = TRAINLOG_SESSION_GENERATOR_PREVIEW;
        session->pending_route = route;
        session->phase = TRAINLOG_SESSION_CONFIRM_LEAVE;
        return;
    }
    if (app_shell_is_list_route(previous)) {
        app->saved_lists[previous] = app->list;
        app->saved_searches[previous] = app->search;
        app->saved_searches[previous].focused = false;
    }
    (void)trainlog_navigation_open(&app->navigation, route, NULL);
    app->focus = TRAINLOG_FOCUS_CONTENT;
    app->content_selected = 0U;
    app->content_scroll = 0U;
    if (route == TRAINLOG_ROUTE_SESSION_MANUAL ||
        route == TRAINLOG_ROUTE_SESSION_CURRENT) {
        session->message[0] = '\0';
        session->phase = session->has_draft ? TRAINLOG_SESSION_DRAFT
            : TRAINLOG_SESSION_CHOOSE_TYPE;
        session->session_type = session->has_draft ? session->session_type
            : TRAINLOG_SESSION_TRAINING;
    } else if (route == TRAINLOG_ROUTE_SESSION_GENERATOR) {
        if (session->generated_preview)
            session->phase = TRAINLOG_SESSION_GENERATOR_PREVIEW;
        else {
            size_t zone_count = trainlog_body_zone_catalog_count();
            session->phase = TRAINLOG_SESSION_GENERATOR_CONFIG;
            if (session->generation_zone == NULL && zone_count > 0U)
                session->generation_zone = trainlog_body_zone_catalog_at(0U);
            if (session->generation_goal == NULL &&
                trainlog_session_generation_policy_v1.goal_count > 0U)
                session->generation_goal = &trainlog_session_generation_policy_v1.goals[0];
            if (session->generation_duration_minutes <= 0 &&
                trainlog_session_generation_policy_v1.duration_preset_count > 0U)
                session->generation_duration_minutes =
                    trainlog_session_generation_policy_v1.duration_presets_minutes[0];
        }
        session_controller_sync_durability(session);
    }
    if (route == TRAINLOG_ROUTE_SESSION_GENERATOR &&
        !session->generated_preview && session->generation_zone == NULL) {
        session->generation_zone_index = 0U;
        session->generation_goal_index = 0U;
        session->generation_duration_index = 0U;
        session->generation_zone = trainlog_body_zone_catalog_at(0U);
        if (trainlog_session_generation_policy_v1.goal_count > 0U)
            session->generation_goal = &trainlog_session_generation_policy_v1.goals[0];
        if (trainlog_session_generation_policy_v1.duration_preset_count > 0U)
            session->generation_duration_minutes =
                trainlog_session_generation_policy_v1.duration_presets_minutes[0];
        session->phase = TRAINLOG_SESSION_GENERATOR_CONFIG;
    } else if (route == TRAINLOG_ROUTE_SESSION_GENERATOR && session->generated_preview &&
               session->phase != TRAINLOG_SESSION_GENERATOR_WARNING)
        session->phase = TRAINLOG_SESSION_GENERATOR_PREVIEW;
    if (route == TRAINLOG_ROUTE_BODY_METRIC)
        app_shell_load_body_metric(app);
    else if (route == TRAINLOG_ROUTE_BODY_TRENDS ||
             route == TRAINLOG_ROUTE_BODY_GLOBAL)
        app_shell_load_body_global(app);
    else if (route == TRAINLOG_ROUTE_BODY_ANALYTICS)
        app_shell_load_body_analytics(app);
    else if (route == TRAINLOG_ROUTE_SYNC) {
        sync_controller_load_history(&app->sync_controller);
        app->sync_controller.showing_detail = false;
    } else if (route == TRAINLOG_ROUTE_SETTINGS)
        app->body_has_profile = body_analytics_profile_load(&app->body_profile);
    if (app_shell_is_list_route(route)) {
        app->list = app->saved_lists[route];
        app->search = app->saved_searches[route];
        app_shell_refresh_list(app);
    }
}

static bool app_shell_back(TrainlogAppContext *app)
{
    TrainlogAppRoute previous = app->navigation.current.route;
    if (app_shell_is_list_route(previous)) {
        app->saved_lists[previous] = app->list;
        app->saved_searches[previous] = app->search;
        app->saved_searches[previous].focused = false;
    }
    if (!trainlog_navigation_back(&app->navigation)) return false;
    if (app_shell_is_list_route(app->navigation.current.route)) {
        app->list = app->saved_lists[app->navigation.current.route];
        app->search = app->saved_searches[app->navigation.current.route];
        app_shell_refresh_list(app);
    }
    return true;
}

static bool app_shell_dispatch_session(TrainlogAppContext *app, int key);
static bool app_shell_dispatch_sync(TrainlogAppContext *app, int key);

static void app_shell_load_exercise_detail_metadata(TrainlogAppContext *app)
{
    size_t index;
    app->exercise_zone_count = 0U;
    app->exercise_explicit_equipment_count = 0U;
    app->exercise_historic_equipment_count = 0U;
    app->exercise_equipment_selected = 0U;
    app->exercise_detail_metadata_error =
        trainlog_database_list_exercise_body_zones(app->database,
            app->exercise_detail.exercise_id, app->exercise_zones,
            MAX_BODY_ZONES, &app->exercise_zone_count) != TRAINLOG_STATUS_OK;
    for (index = 0U; index < trainlog_equipment_catalog_relation_count() &&
         app->exercise_explicit_equipment_count < 64U; ++index) {
        const TrainlogExerciseEquipmentRelation *relation =
            trainlog_equipment_catalog_relation_at(index);
        if (relation != NULL && strcmp(relation->exercise_id,
                app->exercise_detail.exercise_id) == 0 &&
            trainlog_database_resolve_equipment(app->database,
                relation->equipment_id,
                &app->exercise_explicit_equipment[
                    app->exercise_explicit_equipment_count]) == TRAINLOG_STATUS_OK)
            ++app->exercise_explicit_equipment_count;
    }
    if (trainlog_database_list_exercise_equipment(app->database,
            app->exercise_detail.exercise_id,
            app->exercise_historic_equipment, 128U,
            &app->exercise_historic_equipment_count) != TRAINLOG_STATUS_OK) {
        app->exercise_historic_equipment_count = 0U;
        app->exercise_detail_metadata_error = true;
    }
}

static void app_shell_primary(TrainlogAppContext *app)
{
    TrainlogAppRoute route = app->navigation.current.route;
    if (route == TRAINLOG_ROUTE_SESSIONS) {
        static const TrainlogAppRoute targets[] = {TRAINLOG_ROUTE_SESSION_CURRENT,
            TRAINLOG_ROUTE_SESSION_MANUAL, TRAINLOG_ROUTE_SESSIONS_COMPLETED};
        app_shell_open_route(app, targets[app->content_selected % 3U]);
    } else if (route == TRAINLOG_ROUTE_STATS) {
        static const TrainlogAppRoute targets[] = {TRAINLOG_ROUTE_STATS_EXERCISE,
            TRAINLOG_ROUTE_BODY, TRAINLOG_ROUTE_MAX};
        app_shell_open_route(app, targets[app->content_selected % 3U]);
    } else if (route == TRAINLOG_ROUTE_SESSION_MANUAL ||
               route == TRAINLOG_ROUTE_SESSION_CURRENT ||
               route == TRAINLOG_ROUTE_SESSION_GENERATOR) {
        (void)app_shell_dispatch_session(app, TRAINLOG_KEY_ENTER);
    } else if (app_shell_is_list_route(route)) {
        if (app->loaded_count == 0U) return;
        /* INVARIANT: detail routes borrow list selection; they do not replace
         * its stable-ID/query/viewport state. Back restores this exact snapshot
         * after any intervening resize or catalogue refresh. */
        app->saved_lists[route] = app->list;
        app->saved_searches[route] = app->search;
        app->saved_searches[route].focused = false;
        if (route == TRAINLOG_ROUTE_EQUIPMENT) {
            app->equipment_detail = app->equipment[app->list.selected_index];
            (void)trainlog_navigation_open(&app->navigation,
                TRAINLOG_ROUTE_EQUIPMENT_DETAIL,
                app->equipment_detail.equipment_id);
        }
        else if (route == TRAINLOG_ROUTE_SESSIONS_COMPLETED)
            app_shell_open_session_detail(app,
                app->sessions[app->list.selected_index].session_id);
        else if (route == TRAINLOG_ROUTE_BODY) {
            app->body_detail = app->body_records[app->list.selected_index];
            (void)trainlog_navigation_open(&app->navigation,
                TRAINLOG_ROUTE_BODY_DETAIL,
                app->body_detail.observation_id);
        }
        else if (route == TRAINLOG_ROUTE_STATS_EXERCISE) {
            app->exercise_detail = app->exercises[app->list.selected_index];
            app_shell_open_exercise_analytics(app,
                TRAINLOG_ROUTE_EXERCISE_PERFORMANCE);
        } else if (route == TRAINLOG_ROUTE_MAX) {
            app->exercise_detail = app->exercises[app->list.selected_index];
            app_shell_open_exercise_analytics(app, TRAINLOG_ROUTE_EXERCISE_MAX);
        }
        else {
            app->exercise_detail = app->exercises[app->list.selected_index];
            app_shell_load_exercise_detail_metadata(app);
            (void)trainlog_navigation_open(&app->navigation,
                TRAINLOG_ROUTE_EXERCISE_DETAIL,
                app->exercise_detail.exercise_id);
        }
        if (route != TRAINLOG_ROUTE_EQUIPMENT && route != TRAINLOG_ROUTE_EXERCISES &&
            route != TRAINLOG_ROUTE_STATS_EXERCISE && route != TRAINLOG_ROUTE_MAX &&
            route != TRAINLOG_ROUTE_SESSIONS_COMPLETED && route != TRAINLOG_ROUTE_BODY) {
            trainlog_terminal_erase(app->terminal);
            app_shell_refresh_list(app);
        }
    } else if (route == TRAINLOG_ROUTE_SYNC) {
        sync_controller_open_detail(&app->sync_controller);
    } else if (route == TRAINLOG_ROUTE_SETTINGS) {
        profile_controller_start(app, TRAINLOG_ROUTE_SETTINGS);
    }
}

static void app_shell_dispatch_overlay(TrainlogAppContext *app, int key)
{
    TrainlogOverlay *overlay = &app->overlays.items[app->overlays.count - 1U];
    if (overlay->type == TRAINLOG_OVERLAY_EXERCISE_MERGE) {
        TrainlogExerciseController *controller = &app->exercise_controller;
        if (controller->phase == TRAINLOG_EXERCISE_MERGE_PICKER) {
            size_t matches = exercise_merge_match_count(controller);
            if (key == TRAINLOG_KEY_ESCAPE) {
                if (controller->form.text[0] != '\0') {
                    trainlog_form_init(&controller->form, "");
                    controller->merge_selected = 0U;
                } else {
                    char id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
                    (void)trainlog_overlays_pop(&app->overlays, &app->focus, id);
                    (void)memset(controller, 0, sizeof(*controller));
                }
            } else if (key == TRAINLOG_KEY_UP && controller->merge_selected > 0U)
                --controller->merge_selected;
            else if (key == TRAINLOG_KEY_DOWN && controller->merge_selected + 1U < matches)
                ++controller->merge_selected;
            else if ((key == TRAINLOG_KEY_ENTER || key == '\n') && matches > 0U) {
                TrainlogExercise *target = exercise_merge_match_at(controller,
                    controller->merge_selected);
                if (target != NULL) {
                    controller->merge_target = *target;
                    if (trainlog_database_preview_exercise_merge(app->database,
                            controller->merge_source.exercise_id,
                            &controller->merge_preview) == TRAINLOG_STATUS_OK)
                        controller->phase = TRAINLOG_EXERCISE_MERGE_CONFIRM;
                    else {
                        controller->phase = TRAINLOG_EXERCISE_MERGE_RESULT;
                        (void)snprintf(controller->message,
                            sizeof(controller->message),
                            "Prévisualisation impossible; aucune fusion effectuée.");
                    }
                    overlay->selected = 0U;
                }
            } else if (!(key == '/' && controller->form.text[0] == '\0')) {
                /* '/' activates the standard search affordance; this overlay
                 * already owns the shared UTF-8 form editor. */
                TrainlogFormResult result = trainlog_form_handle(&controller->form, key);
                if (result == TRAINLOG_FORM_EDITED) controller->merge_selected = 0U;
            }
        } else if (controller->phase == TRAINLOG_EXERCISE_MERGE_CONFIRM) {
            if (key == TRAINLOG_KEY_ESCAPE) {
                controller->phase = TRAINLOG_EXERCISE_MERGE_PICKER;
                overlay->selected = controller->merge_selected;
            } else if (key == TRAINLOG_KEY_UP || key == TRAINLOG_KEY_DOWN)
                overlay->selected = overlay->selected == 0U ? 1U : 0U;
            else if (key == TRAINLOG_KEY_ENTER || key == '\n') {
                if (overlay->selected == 0U) {
                    controller->phase = TRAINLOG_EXERCISE_MERGE_PICKER;
                    overlay->selected = controller->merge_selected;
                } else {
                    TrainlogStatus status = trainlog_database_merge_exercises(app->database,
                        controller->merge_source.exercise_id,
                        controller->merge_target.exercise_id);
                    if (status == TRAINLOG_STATUS_OK) {
                        app->exercise_detail = controller->merge_target;
                        (void)snprintf(app->navigation.current.stable_id,
                            sizeof(app->navigation.current.stable_id), "%s",
                            controller->merge_target.exercise_id);
                        app_shell_load_exercise_detail_metadata(app);
                        (void)snprintf(controller->message, sizeof(controller->message),
                            "Fusion terminée. Cible sélectionnée : %.112s",
                            controller->merge_target.name);
                    } else (void)snprintf(controller->message,
                        sizeof(controller->message), "%s",
                        status == TRAINLOG_STATUS_CONFLICT
                            ? "Fusion refusée : profil ou zone principale incompatible. Aucun changement."
                            : "Fusion impossible. Aucun changement confirmé.");
                    controller->phase = TRAINLOG_EXERCISE_MERGE_RESULT;
                }
            }
        } else if (key == TRAINLOG_KEY_ENTER || key == '\n' ||
                   key == TRAINLOG_KEY_ESCAPE) {
            char id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
            (void)trainlog_overlays_pop(&app->overlays, &app->focus, id);
            (void)memset(controller, 0, sizeof(*controller));
        }
        return;
    }
    size_t count = overlay->type == TRAINLOG_OVERLAY_NAVIGATION
        ? sizeof(shell_sections) / sizeof(shell_sections[0])
        : overlay->type == TRAINLOG_OVERLAY_CONFIRMATION ? 2U : app->actions.count;
    if (key == TRAINLOG_KEY_ESCAPE ||
        (key == TRAINLOG_KEY_F6 &&
         overlay->type == TRAINLOG_OVERLAY_NAVIGATION)) {
        char id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
        (void)trainlog_overlays_pop(&app->overlays, &app->focus, id);
        app->search.focused = app->focus == TRAINLOG_FOCUS_SEARCH;
    } else if (key == TRAINLOG_KEY_UP && overlay->selected > 0U) --overlay->selected;
    else if (key == TRAINLOG_KEY_DOWN && overlay->selected + 1U < count) ++overlay->selected;
    else if ((key == TRAINLOG_KEY_ENTER || key == '\n') && count > 0U) {
        if (overlay->type == TRAINLOG_OVERLAY_CONFIRMATION && app->quit_confirmation) {
            bool quit = overlay->selected == 1U;
            char id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
            (void)trainlog_overlays_pop(&app->overlays, &app->focus, id);
            app->search.focused = app->focus == TRAINLOG_FOCUS_SEARCH;
            app->quit_confirmation = false;
            if (quit) app->running = false;
        } else if (overlay->type == TRAINLOG_OVERLAY_NAVIGATION) {
            TrainlogAppRoute target = shell_sections[overlay->selected];
            char id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
            (void)trainlog_overlays_pop(&app->overlays, &app->focus, id);
            app->search.focused = app->focus == TRAINLOG_FOCUS_SEARCH;
            app_shell_open_route(app, target);
        } else if (overlay->type == TRAINLOG_OVERLAY_ACTIONS) {
            TrainlogAction action = app->actions.items[overlay->selected];
            char id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
            (void)trainlog_overlays_pop(&app->overlays, &app->focus, id);
            if (app->exercise_controller.phase != TRAINLOG_EXERCISE_IDLE) {
                (void)app_shell_dispatch_exercise_controller(app, action.key);
            } else if (app->equipment_controller.phase != TRAINLOG_EQUIPMENT_IDLE) {
                (void)app_shell_dispatch_equipment_controller(app, action.key);
            } else if (app->session.form.active) {
                (void)snprintf(app->session.message, sizeof(app->session.message),
                    "Terminez ou annulez le champ actif avant une autre action.");
            } else if (strcmp(action.identifier, "generator.discard") == 0) {
                session_controller_discard_generator(&app->session);
                app_shell_open_route(app, TRAINLOG_ROUTE_SESSIONS);
            } else if (strcmp(action.identifier, "exercise.merge") == 0) {
                exercise_controller_start_merge(app);
            } else if (app->navigation.current.route == TRAINLOG_ROUTE_SYNC) {
                /* CONTRACT: F7 executes the exact registered key through the
                 * production sync controller; it is not a palette-only stub. */
                (void)app_shell_dispatch_sync(app, action.key);
            } else if (app->navigation.current.route == TRAINLOG_ROUTE_SESSION_MANUAL ||
                app->navigation.current.route == TRAINLOG_ROUTE_SESSION_CURRENT ||
                app->navigation.current.route == TRAINLOG_ROUTE_SESSION_GENERATOR)
                (void)app_shell_dispatch_session(app, action.key);
            else if (action.intent == TRAINLOG_INTENT_PRIMARY) app_shell_primary(app);
            else if (action.intent == TRAINLOG_INTENT_OPEN_SEARCH &&
                app_shell_is_list_route(app->navigation.current.route)) {
                /* Keep F7 action execution aligned with the registered '/'
                 * action: it opens the filter and transfers editor focus. */
                app->search.open = true;
                app->search.focused = true;
                app->focus = TRAINLOG_FOCUS_SEARCH;
            } else if (action.intent == TRAINLOG_INTENT_OPEN_ROUTE)
                app_shell_open_route(app, action.route);
            else if (action.intent == TRAINLOG_INTENT_BACK)
                (void)app_shell_back(app);
        }
    }
}

static bool session_form_int(const TrainlogFormField *form, int *output)
{
    char *end = NULL;
    long value;
    if (form == NULL || output == NULL || form->bytes == 0U) return false;
    value = strtol(form->text, &end, 10);
    if (end == form->text || *end != '\0' || value <= 0L || value > INT_MAX)
        return false;
    *output = (int)value;
    return true;
}

static bool session_form_nonnegative_int(const TrainlogFormField *form, int *output)
{
    char *end = NULL;
    long value;
    if (form == NULL || output == NULL || form->bytes == 0U) return false;
    value = strtol(form->text, &end, 10);
    if (end == form->text || *end != '\0' || value < 0L || value > INT_MAX)
        return false;
    *output = (int)value;
    return true;
}

static bool session_form_double(const TrainlogFormField *form, double *output)
{
    char *end = NULL;
    double value;
    if (form == NULL || output == NULL || form->bytes == 0U) return false;
    value = strtod(form->text, &end);
    if (end == form->text || *end != '\0' || !isfinite(value) || value < 0.0)
        return false;
    *output = value;
    return true;
}

static void session_controller_start_form(TrainlogSessionController *session,
                                          TrainlogSessionFormPurpose purpose,
                                          const char *initial)
{
    session->form_purpose = purpose;
    trainlog_form_init(&session->form, initial);
}

static bool app_shell_dispatch_session_form(TrainlogAppContext *app, int key)
{
    TrainlogSessionController *session = &app->session;
    TrainlogSessionDraftExercise *draft = &session->drafts[session->selected];
    TrainlogFormResult result = trainlog_form_handle(&session->form, key);
    int integer_value;
    double double_value;
    if (result == TRAINLOG_FORM_OPEN_NAVIGATION) {
        (void)trainlog_overlays_push(&app->overlays, TRAINLOG_OVERLAY_NAVIGATION,
            TRAINLOG_FOCUS_EDITOR, app->navigation.current.stable_id); return true;
    }
    if (result == TRAINLOG_FORM_OPEN_ACTIONS) {
        (void)trainlog_overlays_push(&app->overlays, TRAINLOG_OVERLAY_ACTIONS,
            TRAINLOG_FOCUS_EDITOR, app->navigation.current.stable_id); return true;
    }
    if (result == TRAINLOG_FORM_CANCEL) {
        session->form.active = false;
        session->form_purpose = TRAINLOG_SESSION_FORM_NONE;
        if (session->phase == TRAINLOG_SESSION_EQUIPMENT_CREATE)
            session->phase = TRAINLOG_SESSION_EQUIPMENT_PICKER;
        return true;
    }
    if (result != TRAINLOG_FORM_SUBMIT && result != TRAINLOG_FORM_NEXT)
        return true;
    if (session->form_purpose == TRAINLOG_SESSION_FORM_EQUIPMENT_NAME) {
        if (session->form.bytes == 0U || session->form.bytes >=
            sizeof(session->pending_equipment.display_name)) {
            (void)snprintf(session->message, sizeof(session->message),
                "Le nom convivial est requis et doit tenir dans le champ.");
            return true;
        }
        (void)snprintf(session->pending_equipment.display_name,
            sizeof(session->pending_equipment.display_name), "%s", session->form.text);
        session_controller_start_form(session,
            TRAINLOG_SESSION_FORM_EQUIPMENT_LABEL, "");
        return true;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_EQUIPMENT_LABEL) {
        if (session->form.bytes >= sizeof(session->pending_equipment.label_name)) {
            (void)snprintf(session->message, sizeof(session->message),
                "Le nom d’étiquette est trop long.");
            return true;
        }
        (void)snprintf(session->pending_equipment.label_name,
            sizeof(session->pending_equipment.label_name), "%s", session->form.text);
        session_controller_start_form(session,
            TRAINLOG_SESSION_FORM_EQUIPMENT_TYPE, "");
        return true;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_EQUIPMENT_TYPE) {
        if (session->form.bytes == 0U || session->form.bytes >=
            sizeof(session->pending_equipment.equipment_type)) {
            (void)snprintf(session->message, sizeof(session->message),
                "Le type d’équipement est requis.");
            return true;
        }
        (void)snprintf(session->pending_equipment.equipment_type,
            sizeof(session->pending_equipment.equipment_type), "%s", session->form.text);
        session_controller_start_form(session,
            TRAINLOG_SESSION_FORM_EQUIPMENT_LOAD, "2");
        return true;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_EQUIPMENT_LOAD &&
               session_form_int(&session->form, &integer_value) &&
               integer_value <= 3) {
        char generated[TRAINLOG_UUID_TEXT_LENGTH + 1U];
        size_t index;
        (void)snprintf(session->pending_equipment.load_semantics,
            sizeof(session->pending_equipment.load_semantics), "%s",
            integer_value == 1 ? "none" : integer_value == 2 ? "external" : "assistance");
        generate_custom_equipment_uuid(generated);
        (void)snprintf(session->pending_equipment.equipment_id,
            sizeof(session->pending_equipment.equipment_id), "%s", generated);
        if (trainlog_database_create_custom_equipment(app->database,
            &session->pending_equipment) != TRAINLOG_STATUS_OK) {
            (void)snprintf(session->message, sizeof(session->message),
                "Équipement non créé : nom ou identifiant invalide/conflit.");
            return true;
        }
        session->form.active = false;
        session->form_purpose = TRAINLOG_SESSION_FORM_NONE;
        session_controller_load_equipment(app);
        for (index = 0U; index < session->equipment_count; ++index)
            if (strcmp(app->equipment[index].equipment_id, generated) == 0) {
                session->equipment_selected = index + 1U;
                break;
            }
        session->message[0] = '\0';
        return true;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_GENERATED_SETS &&
        session_form_int(&session->form, &integer_value) && integer_value <= 64 &&
        session->selected < session->generated.exercise_count) {
        char value[48];
        session->pending_generated_sets = integer_value;
        (void)snprintf(value, sizeof(value), "%d",
            session->generated.exercises[session->selected].target_repetitions);
        session_controller_start_form(session,
            TRAINLOG_SESSION_FORM_GENERATED_REPS, value);
        return true;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_GENERATED_REPS &&
        session_form_int(&session->form, &integer_value) && integer_value <= 10000 &&
        session->selected < session->generated.exercise_count) {
        if (!session_controller_requalify_generated(app, session->selected,
            session->pending_generated_sets, integer_value)) {
            (void)snprintf(session->message, sizeof(session->message),
                "Dose refusée; la proposition précédente est conservée.");
            return true;
        }
        session->form.active = false;
        session->form_purpose = TRAINLOG_SESSION_FORM_NONE;
        session_controller_sync_durability(session);
        return true;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_GENERATED_PERCENT_MAX &&
        session_form_int(&session->form, &integer_value) && integer_value <= 100 &&
        session->selected < session->generated.exercise_count) {
        TrainlogGeneratedExercise *generated =
            &session->generated.exercises[session->selected];
        TrainlogLatestExplicitMax latest = {0};
        double target = 0.0;
        TrainlogStatus status = trainlog_database_latest_explicit_max_equipment_context(
            app->database, generated->exercise_id, generated->equipment_id, &latest);
        generated->has_target_weight = status == TRAINLOG_STATUS_OK &&
            trainlog_measured_max_target_load(&latest, generated->equipment_id,
                strcmp(generated->equipment_load_semantics, "external") == 0
                    ? TRAINLOG_LOAD_EXTERNAL : TRAINLOG_LOAD_NONE,
                integer_value, &target) == TRAINLOG_STATUS_OK;
        generated->target_weight_kg = generated->has_target_weight ? target : 0.0;
        generated->planned_load_mode = generated->has_target_weight
            ? TRAINLOG_LOAD_EXTERNAL : TRAINLOG_LOAD_NONE;
        generated->rationale_count = 1U;
        (void)snprintf(generated->rationale_codes[0],
            sizeof(generated->rationale_codes[0]), "%s",
            generated->has_target_weight ? "user_selected_max_percentage"
                                         : "compatible_max_unavailable");
        generated->load_source_session_id[0] = '\0';
        generated->load_source_occurrence_id[0] = '\0';
        generated->load_source_started_at[0] = '\0';
        session->form.active = false;
        session->form_purpose = TRAINLOG_SESSION_FORM_NONE;
        session->dirty = true;
        session_controller_sync_durability(session);
        return true;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_SET_METRIC &&
        session_form_int(&session->form, &integer_value)) {
        if (draft->tracking_mode == TRAINLOG_TRACKING_DURATION)
            session->pending_set.duration_seconds = integer_value;
        else session->pending_set.reps = integer_value;
        if (draft->input.load_mode != TRAINLOG_LOAD_NONE) {
            session_controller_start_form(session,
                TRAINLOG_SESSION_FORM_SET_WEIGHT, "");
            return true;
        }
        if (session->set_selected < draft->input.set_count)
            draft->sets[session->set_selected] = session->pending_set;
        else if (draft->input.set_count < MAX_SETS_PER_EXERCISE)
            draft->sets[draft->input.set_count++] = session->pending_set;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_SET_WEIGHT &&
               (session->form.bytes == 0U ||
                session_form_double(&session->form, &double_value))) {
        session->pending_set.has_weight = session->form.bytes > 0U;
        session->pending_set.weight_kg = session->form.bytes > 0U ? double_value : 0.0;
        if (session->set_selected < draft->input.set_count)
            draft->sets[session->set_selected] = session->pending_set;
        else if (draft->input.set_count < MAX_SETS_PER_EXERCISE)
            draft->sets[draft->input.set_count++] = session->pending_set;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_CONTINUOUS_DURATION &&
               session_form_int(&session->form, &integer_value)) {
        draft->input.continuous_duration_seconds = integer_value;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_CONTINUOUS_SPEED &&
               (session->form.bytes == 0U || session_form_double(&session->form,
                   &double_value))) {
        draft->input.continuous_has_speed = session->form.bytes > 0U;
        draft->input.continuous_speed_kmh = session->form.bytes > 0U ? double_value : 0.0;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_CONTINUOUS_DISTANCE &&
               (session->form.bytes == 0U || session_form_double(&session->form,
                   &double_value))) {
        draft->input.continuous_has_distance = session->form.bytes > 0U;
        draft->input.continuous_distance_km = session->form.bytes > 0U ? double_value : 0.0;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_TARGET_SETS &&
               session_form_nonnegative_int(&session->form, &integer_value)) {
        draft->input.target_sets = integer_value;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_TARGET_REPS &&
               session_form_nonnegative_int(&session->form, &integer_value)) {
        draft->input.target_reps = integer_value;
        if (integer_value > 0) draft->input.target_duration_seconds = 0;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_TARGET_DURATION &&
               session_form_nonnegative_int(&session->form, &integer_value)) {
        draft->input.target_duration_seconds = integer_value;
        if (integer_value > 0) draft->input.target_reps = 0;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_REST &&
               session_form_nonnegative_int(&session->form, &integer_value)) {
        draft->input.rest_seconds = integer_value;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_TARGET_WEIGHT &&
               (session->form.bytes == 0U || session_form_double(&session->form,
                   &double_value))) {
        draft->input.target_has_weight = session->form.bytes > 0U;
        draft->input.target_weight_kg = session->form.bytes > 0U ? double_value : 0.0;
        draft->target_from_percent_max = false;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_TARGET_PERCENT_MAX &&
               session_form_int(&session->form, &integer_value) &&
               integer_value <= 100) {
        TrainlogLatestExplicitMax latest = {0};
        double target = 0.0;
        TrainlogStatus max_status = trainlog_database_latest_explicit_max_equipment_context(
            app->database, draft->input.exercise_id, draft->input.equipment_id, &latest);
        if (max_status != TRAINLOG_STATUS_OK ||
            trainlog_measured_max_target_load(&latest,
                draft->input.equipment_id, draft->input.load_mode,
                integer_value, &target) !=
                    TRAINLOG_STATUS_OK) {
            draft->input.target_has_weight = false;
            draft->input.target_weight_kg = 0.0;
            draft->target_from_percent_max = false;
            (void)snprintf(session->message, sizeof(session->message),
                "MAX compatible indisponible : exercice et équipement externe exacts requis.");
            session->form.active = false;
            session->form_purpose = TRAINLOG_SESSION_FORM_NONE;
            draft_bind_input(draft);
            session_controller_sync_durability(session);
            return true;
        }
        draft->input.target_has_weight = true;
        draft->input.target_weight_kg = target;
        draft->target_from_percent_max = true;
    } else if (session->form_purpose == TRAINLOG_SESSION_FORM_MAX_WEIGHT &&
               session_form_double(&session->form, &double_value)) {
        draft->input.has_max_weight = true;
        draft->input.max_weight_kg = double_value;
        draft->input.set_count = 0U;
        draft->input.continuous_duration_seconds = 0;
    } else {
        (void)snprintf(session->message, sizeof(session->message),
            "Valeur invalide; saisissez un nombre positif.");
        return true;
    }
    draft_bind_input(draft);
    session->dirty = true;
    session->message[0] = '\0';
    session->form.active = false;
    session->form_purpose = TRAINLOG_SESSION_FORM_NONE;
    session_controller_sync_durability(session);
    return true;
}

static bool app_shell_dispatch_session(TrainlogAppContext *app, int key)
{
    TrainlogSessionController *session = &app->session;
    TrainlogAppRoute route = app->navigation.current.route;
    if (route != TRAINLOG_ROUTE_SESSION_MANUAL &&
        route != TRAINLOG_ROUTE_SESSION_CURRENT &&
        route != TRAINLOG_ROUTE_SESSION_GENERATOR) return false;
    if (session->form.active) return app_shell_dispatch_session_form(app, key);
    if (key == TRAINLOG_KEY_F6 || key == TRAINLOG_KEY_F7 || key == '?') return false;
    if (session->phase == TRAINLOG_SESSION_CHOOSE_TYPE) {
        if (key == TRAINLOG_KEY_UP || key == TRAINLOG_KEY_DOWN ||
            key == TRAINLOG_KEY_LEFT || key == TRAINLOG_KEY_RIGHT)
            session->session_type = session->session_type == TRAINLOG_SESSION_TRAINING
                ? TRAINLOG_SESSION_MAX_TEST : TRAINLOG_SESSION_TRAINING;
        else if (key == TRAINLOG_KEY_ENTER || key == '\n') {
            if (trainlog_time_now_rfc3339(session->started_at,
                sizeof(session->started_at)) == TRAINLOG_STATUS_OK) {
                session->has_draft = true;
                session_controller_load_picker(app);
            }
        } else if (key == TRAINLOG_KEY_ESCAPE || key == 27)
            app_shell_open_route(app, TRAINLOG_ROUTE_SESSIONS);
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_EXERCISE_PICKER) {
        if (key == TRAINLOG_KEY_UP && session->picker_selected > 0U)
            --session->picker_selected;
        else if (key == TRAINLOG_KEY_DOWN &&
                 session->picker_selected + 1U < session->picker_count)
            ++session->picker_selected;
        else if (key == 'n' || key == 'N')
            exercise_controller_start_create(app, true);
        else if (key == TRAINLOG_KEY_ENTER || key == '\n')
            (void)session_controller_choose_exercise(session);
        else if (key == TRAINLOG_KEY_ESCAPE || key == 27) {
            session->replacing_occurrence = false;
            session->phase = TRAINLOG_SESSION_DRAFT;
        }
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_EQUIPMENT_PICKER) {
        if (key == TRAINLOG_KEY_UP && session->equipment_selected > 0U)
            --session->equipment_selected;
        else if (key == TRAINLOG_KEY_DOWN &&
                 session->equipment_selected < session->equipment_count)
            ++session->equipment_selected;
        else if ((key == 'x' || key == 'X') && session->selected < session->draft_count) {
            TrainlogSessionDraftExercise *draft = &session->drafts[session->selected];
            draft->input.equipment_id[0] = '\0';
            draft->input.load_mode = TRAINLOG_LOAD_NONE;
            draft->input.target_has_weight = false;
            draft->input.target_weight_kg = 0.0;
            draft->target_from_percent_max = false;
            for (size_t index = 0U; index < draft->input.set_count; ++index) {
                draft->sets[index].has_weight = false;
                draft->sets[index].weight_kg = 0.0;
            }
            draft_bind_input(draft);
            session->dirty = true;
            session->phase = TRAINLOG_SESSION_DRAFT;
        } else if ((key == TRAINLOG_KEY_ENTER || key == '\n') &&
                   session->selected < session->draft_count) {
            TrainlogSessionDraftExercise *draft = &session->drafts[session->selected];
            const char *id = session->equipment_selected == 0U ? ""
                : app->equipment[session->equipment_selected - 1U].equipment_id;
            bool context_changed = strcmp(draft->input.equipment_id, id) != 0;
            (void)snprintf(session->drafts[session->selected].input.equipment_id,
                sizeof(session->drafts[session->selected].input.equipment_id), "%s", id);
            if (session->equipment_selected == 0U)
                session->drafts[session->selected].input.load_mode = TRAINLOG_LOAD_NONE;
            else if (strcmp(app->equipment[session->equipment_selected - 1U].load_semantics,
                "assistance") == 0)
                session->drafts[session->selected].input.load_mode = TRAINLOG_LOAD_ASSISTANCE;
            else if (strcmp(app->equipment[session->equipment_selected - 1U].load_semantics,
                "external") == 0)
                session->drafts[session->selected].input.load_mode = TRAINLOG_LOAD_EXTERNAL;
            else session->drafts[session->selected].input.load_mode = TRAINLOG_LOAD_NONE;
            /* INVARIANT: a calculated target cannot survive an equipment
             * context change. Direct manual kg remains user-owned. */
            if (context_changed && draft->target_from_percent_max) {
                draft->input.target_has_weight = false;
                draft->input.target_weight_kg = 0.0;
                draft->target_from_percent_max = false;
            }
            if (session->drafts[session->selected].input.load_mode == TRAINLOG_LOAD_NONE) {
                session->drafts[session->selected].input.target_has_weight = false;
                session->drafts[session->selected].input.target_weight_kg = 0.0;
                for (size_t index = 0U; index < draft->input.set_count; ++index) {
                    draft->sets[index].has_weight = false;
                    draft->sets[index].weight_kg = 0.0;
                }
                draft_bind_input(draft);
            }
            session->dirty = true;
            session->phase = TRAINLOG_SESSION_DRAFT;
        } else if (key == 'n' || key == 'N') {
            (void)memset(&session->pending_equipment, 0,
                sizeof(session->pending_equipment));
            session->message[0] = '\0';
            session->phase = TRAINLOG_SESSION_EQUIPMENT_CREATE;
            session_controller_start_form(session,
                TRAINLOG_SESSION_FORM_EQUIPMENT_NAME, "");
        } else if (key == TRAINLOG_KEY_ESCAPE || key == 27)
            session->phase = TRAINLOG_SESSION_DRAFT;
        session_controller_sync_durability(session);
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_PLANNING &&
        session->selected < session->draft_count) {
        TrainlogSessionDraftExercise *draft = &session->drafts[session->selected];
        if (key == TRAINLOG_KEY_UP && session->planning_field > 0U)
            --session->planning_field;
        else if (key == TRAINLOG_KEY_DOWN && session->planning_field < 4U)
            ++session->planning_field;
        else if (key == TRAINLOG_KEY_TAB)
            session->planning_field = (session->planning_field + 1U) % 5U;
        else if (key == TRAINLOG_KEY_SHIFT_TAB)
            session->planning_field = session->planning_field > 0U
                ? session->planning_field - 1U : 4U;
        else if (key == '%' && session->planning_field == 4U) {
            if (draft->input.load_mode != TRAINLOG_LOAD_EXTERNAL ||
                draft->input.equipment_id[0] == '\0') {
                draft->input.target_has_weight = false;
                draft->input.target_weight_kg = 0.0;
                draft->target_from_percent_max = false;
                (void)snprintf(session->message, sizeof(session->message),
                    "%%MAX indisponible : choisissez une résistance externe compatible.");
            } else session_controller_start_form(session,
                TRAINLOG_SESSION_FORM_TARGET_PERCENT_MAX, "");
        } else if (key == TRAINLOG_KEY_ENTER || key == '\n' || key == 'e' || key == 'E') {
            char value[48] = "";
            TrainlogSessionFormPurpose purposes[] = {
                TRAINLOG_SESSION_FORM_TARGET_SETS,
                TRAINLOG_SESSION_FORM_TARGET_REPS,
                TRAINLOG_SESSION_FORM_TARGET_DURATION,
                TRAINLOG_SESSION_FORM_REST,
                TRAINLOG_SESSION_FORM_TARGET_WEIGHT
            };
            bool applicable = session->planning_field == 0U
                ? draft->input.recording_mode == TRAINLOG_RECORDING_SETS
                : session->planning_field == 1U
                ? draft->input.recording_mode == TRAINLOG_RECORDING_SETS &&
                    draft->tracking_mode == TRAINLOG_TRACKING_REPS
                : session->planning_field == 2U
                ? draft->tracking_mode == TRAINLOG_TRACKING_DURATION
                : session->planning_field == 3U
                ? draft->input.recording_mode == TRAINLOG_RECORDING_SETS
                : draft->input.load_mode != TRAINLOG_LOAD_NONE;
            if (!applicable) {
                (void)snprintf(session->message, sizeof(session->message),
                    "Ce champ ne s’applique pas au profil de cet exercice.");
                return true;
            }
            if (session->planning_field == 0U)
                (void)snprintf(value, sizeof(value), "%d", draft->input.target_sets);
            else if (session->planning_field == 1U)
                (void)snprintf(value, sizeof(value), "%d", draft->input.target_reps);
            else if (session->planning_field == 2U)
                (void)snprintf(value, sizeof(value), "%d", draft->input.target_duration_seconds);
            else if (session->planning_field == 3U)
                (void)snprintf(value, sizeof(value), "%d", draft->input.rest_seconds);
            else if (draft->input.target_has_weight)
                (void)snprintf(value, sizeof(value), "%.2f", draft->input.target_weight_kg);
            session_controller_start_form(session, purposes[session->planning_field], value);
        } else if (key == TRAINLOG_KEY_ESCAPE || key == 27 || key == 'b' || key == 'B')
            session->phase = TRAINLOG_SESSION_DRAFT;
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_CONFIRM_REMOVE) {
        if (key == '1' && session->selected < session->draft_count) {
            draft_delete_exercise(session->drafts, &session->draft_count,
                session->selected);
            if (session->selected >= session->draft_count && session->selected > 0U)
                --session->selected;
            session->dirty = true;
            session->phase = TRAINLOG_SESSION_DRAFT;
        } else if (key == '0' || key == TRAINLOG_KEY_ESCAPE || key == 27)
            session->phase = TRAINLOG_SESSION_DRAFT;
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_CONFIRM_ABANDON) {
        if (key == '1') {
            session_controller_clear_draft(session);
            app_shell_open_route(app, TRAINLOG_ROUTE_SESSIONS);
        } else if (key == '0' || key == TRAINLOG_KEY_ESCAPE || key == 27)
            session->phase = TRAINLOG_SESSION_DRAFT;
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_CONFIRM_LEAVE) {
        if (key == TRAINLOG_KEY_ENTER || key == '\n') {
            TrainlogAppRoute pending = session->pending_route;
            app_shell_open_route(app, pending);
        } else if (key == 'd' || key == 'D') {
            TrainlogAppRoute pending = session->pending_route;
            session_controller_discard_generator(session);
            app_shell_open_route(app, pending);
        } else if (key == TRAINLOG_KEY_ESCAPE || key == 27)
            session->phase = session->return_phase;
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_MESSAGE) {
        if (key == TRAINLOG_KEY_ENTER || key == '\n' ||
            key == TRAINLOG_KEY_ESCAPE || key == 27) {
            session->message[0] = '\0';
            session->phase = session->return_phase != TRAINLOG_SESSION_IDLE
                ? session->return_phase : TRAINLOG_SESSION_DRAFT;
        }
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_GENERATOR_CONFIG) {
        size_t zone_count = trainlog_body_zone_catalog_count();
        if ((key == 'z' || key == 'Z') && zone_count > 0U) {
            session->generation_zone_index =
                (session->generation_zone_index + 1U) % zone_count;
            session->generation_zone = trainlog_body_zone_catalog_at(
                session->generation_zone_index);
        } else if ((key == 'o' || key == 'O') &&
                   trainlog_session_generation_policy_v1.goal_count > 0U) {
            session->generation_goal_index = (session->generation_goal_index + 1U) %
                trainlog_session_generation_policy_v1.goal_count;
            session->generation_goal = &trainlog_session_generation_policy_v1.goals[
                session->generation_goal_index];
        } else if ((key == 't' || key == 'T') &&
                   trainlog_session_generation_policy_v1.duration_preset_count > 0U) {
            session->generation_duration_index =
                (session->generation_duration_index + 1U) %
                trainlog_session_generation_policy_v1.duration_preset_count;
            session->generation_duration_minutes =
                trainlog_session_generation_policy_v1.duration_presets_minutes[
                    session->generation_duration_index];
        } else if (key == 'g' || key == 'G' || key == TRAINLOG_KEY_ENTER || key == '\n')
            (void)session_controller_generate(app);
        else if (key == TRAINLOG_KEY_ESCAPE || key == 27) {
            session->pending_route = TRAINLOG_ROUTE_SESSIONS;
            session->return_phase = TRAINLOG_SESSION_GENERATOR_CONFIG;
            session->phase = TRAINLOG_SESSION_CONFIRM_LEAVE;
        }
        session_controller_sync_durability(session);
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_GENERATOR_WARNING) {
        if (key == 'c' || key == 'C' || key == TRAINLOG_KEY_ENTER || key == '\n') {
            session->generation_warning_acknowledged = true;
            session->phase = TRAINLOG_SESSION_GENERATOR_PREVIEW;
        } else if (key == 'z' || key == 'Z') {
            session->generated_preview = false;
            session->phase = TRAINLOG_SESSION_GENERATOR_CONFIG;
        } else if (key == TRAINLOG_KEY_ESCAPE || key == 27) {
            session->pending_route = TRAINLOG_ROUTE_SESSIONS;
            session->return_phase = TRAINLOG_SESSION_GENERATOR_WARNING;
            session->phase = TRAINLOG_SESSION_CONFIRM_LEAVE;
        }
        session_controller_sync_durability(session);
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_GENERATOR_PREVIEW) {
        size_t count = session->generated.exercise_count;
        if (key == TRAINLOG_KEY_UP && session->selected > 0U) --session->selected;
        else if (key == TRAINLOG_KEY_DOWN && session->selected + 1U < count)
            ++session->selected;
        else if ((key == '<' || key == ',') && session->selected > 0U) {
            TrainlogGeneratedExercise item = session->generated.exercises[session->selected];
            TrainlogGeneratorPreviewItem label = session->generated_items[session->selected];
            session->generated.exercises[session->selected] =
                session->generated.exercises[session->selected - 1U];
            session->generated_items[session->selected] =
                session->generated_items[session->selected - 1U];
            session->generated.exercises[--session->selected] = item;
            session->generated_items[session->selected] = label;
        } else if ((key == '>' || key == '.') && session->selected + 1U < count) {
            TrainlogGeneratedExercise item = session->generated.exercises[session->selected];
            TrainlogGeneratorPreviewItem label = session->generated_items[session->selected];
            session->generated.exercises[session->selected] =
                session->generated.exercises[session->selected + 1U];
            session->generated_items[session->selected] =
                session->generated_items[session->selected + 1U];
            session->generated.exercises[++session->selected] = item;
            session->generated_items[session->selected] = label;
        } else if ((key == 'd' || key == TRAINLOG_KEY_DELETE) && count > 0U) {
            for (size_t index = session->selected; index + 1U < count; ++index) {
                session->generated.exercises[index] = session->generated.exercises[index + 1U];
                session->generated_items[index] = session->generated_items[index + 1U];
            }
            --session->generated.exercise_count;
            if (session->selected >= session->generated.exercise_count && session->selected > 0U)
                --session->selected;
        } else if ((key == 'e' || key == 'E') && count > 0U) {
            char value[48];
            (void)snprintf(value, sizeof(value), "%d",
                session->generated.exercises[session->selected].target_sets);
            session_controller_start_form(session,
                TRAINLOG_SESSION_FORM_GENERATED_SETS, value);
        } else if (key == '%' && count > 0U) {
            session_controller_start_form(session,
                TRAINLOG_SESSION_FORM_GENERATED_PERCENT_MAX, "");
        } else if ((key == 'x' || key == 'X') && count > 0U) {
            TrainlogGeneratedExercise *generated =
                &session->generated.exercises[session->selected];
            generated->has_target_weight = false;
            generated->target_weight_kg = 0.0;
            generated->planned_load_mode = TRAINLOG_LOAD_NONE;
            generated->rationale_count = 1U;
            (void)snprintf(generated->rationale_codes[0],
                sizeof(generated->rationale_codes[0]), "%s", "numeric_load_absent");
            generated->load_source_session_id[0] = '\0';
            generated->load_source_occurrence_id[0] = '\0';
            generated->load_source_started_at[0] = '\0';
        } else if ((key == 'u' || key == 'U') && count > 0U) {
            TrainlogGeneratedExercise *generated =
                &session->generated.exercises[session->selected];
            (void)session_controller_requalify_generated(app, session->selected,
                generated->target_sets, generated->target_repetitions);
        } else if (key == 'g' || key == 'G') {
            (void)session_controller_generate(app);
        } else if (key == 'a' || key == 'A' || key == TRAINLOG_KEY_ENTER || key == '\n') {
            if (session_controller_accept_generator(app))
                app_shell_open_route(app, TRAINLOG_ROUTE_SESSION_CURRENT);
        } else if (key == 'q' || key == 'Q' || key == TRAINLOG_KEY_ESCAPE || key == 27) {
            session->pending_route = TRAINLOG_ROUTE_SESSIONS;
            session->return_phase = TRAINLOG_SESSION_GENERATOR_PREVIEW;
            session->phase = TRAINLOG_SESSION_CONFIRM_LEAVE;
        }
        session_controller_sync_durability(session);
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_ACTUALS && session->draft_count > 0U) {
        TrainlogSessionDraftExercise *draft = &session->drafts[session->selected];
        if (key == 'f' || key == 'F' || key == 'b' || key == 'B' ||
            key == TRAINLOG_KEY_ESCAPE || key == 27) session->phase = TRAINLOG_SESSION_DRAFT;
        else if (session->session_type == TRAINLOG_SESSION_MAX_TEST ||
                 draft->input.has_max_weight) {
            if (key == TRAINLOG_KEY_ENTER || key == '\n' || key == 'e' || key == 'E') {
                char value[48] = "";
                if (draft->input.has_max_weight)
                    (void)snprintf(value, sizeof(value), "%.2f", draft->input.max_weight_kg);
                session_controller_start_form(session, TRAINLOG_SESSION_FORM_MAX_WEIGHT, value);
            }
        } else if (draft->input.recording_mode == TRAINLOG_RECORDING_CONTINUOUS) {
            if (key == TRAINLOG_KEY_ENTER || key == '\n' || key == 'e' || key == 'E') {
                char value[48] = "";
                if (draft->input.continuous_duration_seconds > 0)
                    (void)snprintf(value, sizeof(value), "%d",
                        draft->input.continuous_duration_seconds);
                session_controller_start_form(session,
                    TRAINLOG_SESSION_FORM_CONTINUOUS_DURATION, value);
            } else if ((key == 's' || key == 'S') &&
                (draft->input.data_fields & TRAINLOG_EXERCISE_DATA_SPEED_KMH) != 0U) {
                char value[48] = "";
                if (draft->input.continuous_has_speed)
                    (void)snprintf(value, sizeof(value), "%.2f",
                        draft->input.continuous_speed_kmh);
                session_controller_start_form(session,
                    TRAINLOG_SESSION_FORM_CONTINUOUS_SPEED, value);
            } else if ((key == 'k' || key == 'K') &&
                (draft->input.data_fields & TRAINLOG_EXERCISE_DATA_DISTANCE_KM) != 0U) {
                char value[48] = "";
                if (draft->input.continuous_has_distance)
                    (void)snprintf(value, sizeof(value), "%.2f",
                        draft->input.continuous_distance_km);
                session_controller_start_form(session,
                    TRAINLOG_SESSION_FORM_CONTINUOUS_DISTANCE, value);
            }
        } else if (key == TRAINLOG_KEY_UP && session->set_selected > 0U)
            --session->set_selected;
        else if (key == TRAINLOG_KEY_DOWN &&
                 session->set_selected + 1U < draft->input.set_count)
            ++session->set_selected;
        else if (key == TRAINLOG_KEY_TAB || key == TRAINLOG_KEY_SHIFT_TAB ||
                 key == TRAINLOG_KEY_LEFT || key == TRAINLOG_KEY_RIGHT)
            session->set_field = session->set_field == 0U ? 1U : 0U;
        else if ((key == 'd' || key == 'D' || key == TRAINLOG_KEY_DELETE) &&
                 draft->input.set_count > 0U) {
            for (size_t index = session->set_selected;
                 index + 1U < draft->input.set_count; ++index)
                draft->sets[index] = draft->sets[index + 1U];
            --draft->input.set_count;
            if (session->set_selected >= draft->input.set_count &&
                session->set_selected > 0U) --session->set_selected;
            draft_bind_input(draft); session->dirty = true;
        } else if ((key == 'a' || key == 'A') &&
                   draft->input.set_count < MAX_SETS_PER_EXERCISE) {
            (void)memset(&session->pending_set, 0, sizeof(session->pending_set));
            session->set_selected = draft->input.set_count;
            session_controller_start_form(session, TRAINLOG_SESSION_FORM_SET_METRIC, "");
        } else if ((key == TRAINLOG_KEY_ENTER || key == '\n' || key == 'e' || key == 'E') &&
                   session->set_selected < draft->input.set_count) {
            char value[48];
            session->pending_set = draft->sets[session->set_selected];
            if (session->set_field == 1U && draft->input.load_mode != TRAINLOG_LOAD_NONE) {
                (void)snprintf(value, sizeof(value), "%.2f", session->pending_set.weight_kg);
                session_controller_start_form(session, TRAINLOG_SESSION_FORM_SET_WEIGHT, value);
            } else {
                (void)snprintf(value, sizeof(value), "%d",
                    draft->tracking_mode == TRAINLOG_TRACKING_DURATION
                        ? session->pending_set.duration_seconds : session->pending_set.reps);
                session_controller_start_form(session, TRAINLOG_SESSION_FORM_SET_METRIC, value);
            }
        }
        session_controller_sync_durability(session);
        return true;
    }
    if (session->phase == TRAINLOG_SESSION_DRAFT) {
        session->message[0] = '\0';
        if (key == TRAINLOG_KEY_UP && session->selected > 0U) --session->selected;
        else if (key == TRAINLOG_KEY_DOWN && session->selected + 1U < session->draft_count)
            ++session->selected;
        else if (key == 'a' || key == 'A') {
            session->replacing_occurrence = false;
            (void)session_controller_load_picker(app);
        } else if ((key == 'r' || key == 'R') && session->draft_count > 0U) {
            session->replacing_occurrence = true;
            (void)session_controller_load_picker(app);
        } else if ((key == 'i' || key == 'I') && session->draft_count > 0U)
            session_controller_load_equipment(app);
        else if ((key == 'p' || key == 'P') && session->draft_count > 0U) {
            session->planning_field = 0U;
            session->phase = TRAINLOG_SESSION_PLANNING;
        } else if ((key == 'd' || key == 'D' || key == TRAINLOG_KEY_DELETE) &&
                   session->draft_count > 0U)
            session->phase = TRAINLOG_SESSION_CONFIRM_REMOVE;
        else if ((key == TRAINLOG_KEY_ENTER || key == '\n' || key == 'e' || key == 'E') &&
                 session->draft_count > 0U) {
            session->set_selected = 0U;
            session->set_field = 0U;
            session->phase = TRAINLOG_SESSION_ACTUALS;
        } else if (key == 'f' || key == 'F') {
            TrainlogStatus status = session_controller_save(app);
            if (status == TRAINLOG_STATUS_OK) {
                app_shell_open_route(app, TRAINLOG_ROUTE_SESSIONS_COMPLETED);
                return true;
            }
            if (status != TRAINLOG_STATUS_INVALID_ARGUMENT)
                (void)snprintf(session->message, sizeof(session->message),
                    "Échec de l’enregistrement; le brouillon est conservé.");
        } else if (key == 'q' || key == 'Q')
            session->phase = TRAINLOG_SESSION_CONFIRM_ABANDON;
        else if (key == TRAINLOG_KEY_ESCAPE || key == 27)
            app_shell_open_route(app, TRAINLOG_ROUTE_SESSIONS);
        session_controller_sync_durability(session);
        return true;
    }
    return true;
}

static bool app_shell_dispatch_sync(TrainlogAppContext *app, int key)
{
    TrainlogSyncController *sync = &app->sync_controller;
    TrainlogSyncScreenAction action;
    bool was_confirming;
    if (app->navigation.current.route != TRAINLOG_ROUTE_SYNC) return false;
    /* INVARIANT: synchronous execution may be re-entered by terminal or test
     * callbacks, but no input can launch/probe/open history during that run. */
    if (sync->running) return true;
    if (sync->showing_detail) {
        size_t visible = app->layout.content.height > 5
            ? (size_t)(app->layout.content.height - 5) : 1U;
        if (key == TRAINLOG_KEY_ESCAPE || key == 'b' || key == 'B' ||
            key == TRAINLOG_KEY_ENTER || key == '\n') {
            sync->showing_detail = false;
            return true;
        }
        if (key == TRAINLOG_KEY_UP && sync->detail_scroll > 0U)
            --sync->detail_scroll;
        else if (key == TRAINLOG_KEY_DOWN &&
                 sync->detail_scroll + visible < sync->detail_line_count)
            ++sync->detail_scroll;
        else if (key == TRAINLOG_KEY_PAGE_UP)
            sync->detail_scroll = sync->detail_scroll > visible
                ? sync->detail_scroll - visible : 0U;
        else if (key == TRAINLOG_KEY_PAGE_DOWN &&
                 sync->detail_line_count > visible) {
            size_t maximum = sync->detail_line_count - visible;
            sync->detail_scroll = sync->detail_scroll + visible < maximum
                ? sync->detail_scroll + visible : maximum;
        }
        return true;
    }
    was_confirming = sync->action.confirming;
    action = trainlog_sync_screen_dispatch(&sync->action,
        key == TRAINLOG_KEY_ESCAPE ? 27 : key);
    if (action.effect == TRAINLOG_SYNC_SCREEN_REFRESH) {
        (void)memset(&sync->device, 0, sizeof(sync->device));
        sync->probe_status = (sync->probe != NULL ? sync->probe
                                                  : trainlog_sync_probe)(
            &sync->device);
        sync->probe_known = true;
        return true;
    }
    if (action.effect == TRAINLOG_SYNC_SCREEN_CONFIRM ||
        action.effect == TRAINLOG_SYNC_SCREEN_CANCEL) return true;
    if (action.effect == TRAINLOG_SYNC_SCREEN_RUN) {
        TrainlogAppSyncRun run = sync->run != NULL ? sync->run : trainlog_sync_run;
        TrainlogStatus run_status;
        (void)memset(&sync->report, 0, sizeof(sync->report));
        sync->has_report = false;
        sync->running = true;
        /* Render before entering the synchronous shared engine. This provides
         * immediate progress without a second scheduler or history system. */
        if (app->terminal != NULL) app_shell_render(app);
        /* INVARIANT: this is the sole engine call site in the controller and
         * every TUI launch has the complete bidirectional intent. */
        run_status = run(TRAINLOG_SYNC_TRIGGER_TUI, false,
            TRAINLOG_SYNC_BIDIRECTIONAL, &sync->report);
        sync->running = false;
        /* WHY: retain the engine's precise diagnostic, but never leave a
         * completed failed action at the unhelpful `error=unknown` boundary. */
        if ((!sync->report.success || run_status != TRAINLOG_STATUS_OK) &&
            sync->report.error[0] == '\0')
            (void)snprintf(sync->report.error, sizeof(sync->report.error), "%s",
                sync_controller_status_error(run_status));
        sync->has_report = true;
        sync_controller_load_history(sync);
        (void)memset(&sync->device, 0, sizeof(sync->device));
        sync->probe_status = (sync->probe != NULL ? sync->probe
                                                  : trainlog_sync_probe)(
            &sync->device);
        sync->probe_known = true;
        return true;
    }
    if (was_confirming) return true;
    if (key == TRAINLOG_KEY_UP && sync->history_count > 0U)
        sync->selected = sync->selected > 0U ? sync->selected - 1U
                                             : sync->history_count - 1U;
    else if (key == TRAINLOG_KEY_DOWN && sync->history_count > 0U)
        sync->selected = sync->selected + 1U < sync->history_count
            ? sync->selected + 1U : 0U;
    else if ((key == TRAINLOG_KEY_ENTER || key == '\n') &&
             sync->history_count > 0U)
        sync_controller_open_detail(sync);
    else return false;
    return true;
}

static void app_shell_dispatch(TrainlogAppContext *app, int key)
{
    const TrainlogAction *action;
    if (!app->layout.usable) { if (key == 'q' || key == 'Q') app->running = false; return; }
    if (trainlog_overlays_top(&app->overlays) != NULL) {
        app_shell_dispatch_overlay(app, key); return;
    }
    /* Local forms own their editor keys before shell aliases or focus controls. */
    if (app->focus == TRAINLOG_FOCUS_EDITOR) {
        if (app_shell_dispatch_exercise_controller(app, key)) return;
        if (app_shell_dispatch_equipment_controller(app, key)) return;
        if (app_shell_dispatch_body_controller(app, key)) return;
        if (app_shell_dispatch_profile_controller(app, key)) return;
        if (app_shell_dispatch_session(app, key)) return;
    }
    /* Local editor input precedes every route or shell alias. Home/End and
     * printable digits therefore remain text editing while search is focused. */
    if (app->search.focused && app_shell_is_list_route(app->navigation.current.route)) {
        bool changed = false;
        if (key == TRAINLOG_KEY_TAB || key == TRAINLOG_KEY_SHIFT_TAB) {
            app->search.focused = false;
            app->focus = key == TRAINLOG_KEY_SHIFT_TAB
                ? TRAINLOG_FOCUS_NAVIGATION : TRAINLOG_FOCUS_CONTENT;
            return;
        }
        if (key == TRAINLOG_KEY_F6 || key == TRAINLOG_KEY_F7) {
            /* Let a focused search invoke shell controls; the overlay restores
             * TRAINLOG_FOCUS_SEARCH and re-enables this editor on close. */
            app->search.focused = false;
        } else {
        if (key == TRAINLOG_KEY_ENTER || key == '\n') {
            app->search.focused = false; app->focus = TRAINLOG_FOCUS_CONTENT; return;
        }
        if (key == TRAINLOG_KEY_ESCAPE || key == 27) {
            bool closed = trainlog_search_escape(&app->search);
            if (closed) app->focus = TRAINLOG_FOCUS_CONTENT;
            app_shell_refresh_list(app); return;
        }
        if (key == TRAINLOG_KEY_HOME) trainlog_search_home(&app->search);
        else if (key == TRAINLOG_KEY_END) trainlog_search_end(&app->search);
        else if (key == TRAINLOG_KEY_LEFT) trainlog_search_left(&app->search);
        else if (key == TRAINLOG_KEY_RIGHT) trainlog_search_right(&app->search);
        else if (key == TRAINLOG_KEY_BACKSPACE || key == TRAINLOG_KEY_DELETE)
            changed = trainlog_search_backspace(&app->search);
        else if (key >= 0x20 && key <= 0x10ffff) {
            char encoded[5] = "";
            utf8proc_ssize_t bytes = utf8proc_encode_char((utf8proc_int32_t)key,
                (utf8proc_uint8_t *)encoded);
            if (bytes > 0) changed = trainlog_search_insert(&app->search,
                encoded, (size_t)bytes);
        }
        if (changed) app_shell_refresh_list(app);
        return;
        }
    }
    if (key == TRAINLOG_KEY_F6) {
        if (app->layout.sidebar_visible) {
            if (app->focus == TRAINLOG_FOCUS_NAVIGATION)
                app->focus = app->navigation_restore_focus == TRAINLOG_FOCUS_NAVIGATION
                    ? TRAINLOG_FOCUS_CONTENT : app->navigation_restore_focus;
            else {
                app->navigation_restore_focus = app->focus;
                app->focus = TRAINLOG_FOCUS_NAVIGATION;
            }
        }
        else (void)trainlog_overlays_push(&app->overlays, TRAINLOG_OVERLAY_NAVIGATION,
            app->focus, app->navigation.current.stable_id);
        return;
    }
    if (key == TRAINLOG_KEY_F7) {
        (void)trainlog_overlays_push(&app->overlays, TRAINLOG_OVERLAY_ACTIONS,
            app->focus, app->navigation.current.stable_id); return;
    }
    if (key == '?') {
        (void)trainlog_overlays_push(&app->overlays, TRAINLOG_OVERLAY_HELP,
            app->focus, app->navigation.current.stable_id); return;
    }
    if (key == TRAINLOG_KEY_TAB || key == TRAINLOG_KEY_SHIFT_TAB) {
        if (app->layout.sidebar_visible) {
            if (key == TRAINLOG_KEY_SHIFT_TAB)
                app->focus = app->focus == TRAINLOG_FOCUS_NAVIGATION
                    ? TRAINLOG_FOCUS_ACTIONS : app->focus == TRAINLOG_FOCUS_ACTIONS
                        ? TRAINLOG_FOCUS_CONTENT : app->focus == TRAINLOG_FOCUS_CONTENT
                            ? TRAINLOG_FOCUS_SEARCH : TRAINLOG_FOCUS_NAVIGATION;
            else app->focus = app->focus == TRAINLOG_FOCUS_NAVIGATION
                ? TRAINLOG_FOCUS_SEARCH : app->focus == TRAINLOG_FOCUS_SEARCH
                    ? TRAINLOG_FOCUS_CONTENT : app->focus == TRAINLOG_FOCUS_CONTENT
                        ? TRAINLOG_FOCUS_ACTIONS : TRAINLOG_FOCUS_NAVIGATION;
        } else if (key == TRAINLOG_KEY_SHIFT_TAB)
            app->focus = app->focus == TRAINLOG_FOCUS_NAVIGATION
                ? TRAINLOG_FOCUS_ACTIONS : app->focus == TRAINLOG_FOCUS_ACTIONS
                    ? TRAINLOG_FOCUS_CONTENT : app->focus == TRAINLOG_FOCUS_CONTENT
                        ? TRAINLOG_FOCUS_SEARCH : TRAINLOG_FOCUS_NAVIGATION;
        else app->focus = app->focus == TRAINLOG_FOCUS_NAVIGATION
            ? TRAINLOG_FOCUS_SEARCH : app->focus == TRAINLOG_FOCUS_SEARCH
                ? TRAINLOG_FOCUS_CONTENT : app->focus == TRAINLOG_FOCUS_CONTENT
                    ? TRAINLOG_FOCUS_ACTIONS : TRAINLOG_FOCUS_NAVIGATION;
        app->search.focused = app->focus == TRAINLOG_FOCUS_SEARCH;
        if (app->search.focused) app->search.open = true;
        return;
    }
    if (app->focus == TRAINLOG_FOCUS_NAVIGATION) {
        if (!app->layout.sidebar_visible) {
            if (key == TRAINLOG_KEY_ENTER || key == '\n')
                (void)trainlog_overlays_push(&app->overlays,
                    TRAINLOG_OVERLAY_NAVIGATION, TRAINLOG_FOCUS_NAVIGATION,
                    app->navigation.current.stable_id);
            else if (key == TRAINLOG_KEY_ESCAPE)
                app->focus = app->navigation_restore_focus == TRAINLOG_FOCUS_NAVIGATION
                    ? TRAINLOG_FOCUS_CONTENT : app->navigation_restore_focus;
            /* INVARIANT: compact Navigation owns its input and cannot fall
             * through to the content route controller. */
            return;
        }
        size_t count = sizeof(shell_sections) / sizeof(shell_sections[0]);
        if (key == TRAINLOG_KEY_UP && app->navigation_selected > 0U) --app->navigation_selected;
        else if (key == TRAINLOG_KEY_DOWN && app->navigation_selected + 1U < count)
            ++app->navigation_selected;
        else if (key == TRAINLOG_KEY_ENTER || key == '\n') {
            app_shell_open_route(app, shell_sections[app->navigation_selected]);
            app->focus = TRAINLOG_FOCUS_CONTENT;
        } else if (key == TRAINLOG_KEY_ESCAPE)
            app->focus = app->navigation_restore_focus == TRAINLOG_FOCUS_NAVIGATION
                ? TRAINLOG_FOCUS_CONTENT : app->navigation_restore_focus;
        app->search.focused = app->focus == TRAINLOG_FOCUS_SEARCH;
        /* CONTRACT: a key delivered to persistent navigation is consumed
         * exactly once. Escape closes focus without also navigating Back. */
        return;
    }
    if (app->focus == TRAINLOG_FOCUS_ACTIONS) {
        if (key == TRAINLOG_KEY_ENTER || key == '\n')
            (void)trainlog_overlays_push(&app->overlays, TRAINLOG_OVERLAY_ACTIONS,
                TRAINLOG_FOCUS_ACTIONS, app->navigation.current.stable_id);
        else if (key == TRAINLOG_KEY_ESCAPE)
            app->focus = TRAINLOG_FOCUS_CONTENT;
        /* INVARIANT: the rendered Actions control owns its input. */
        return;
    }
    /* WHY: route controllers may legitimately own printable keys. Search is a
     * shell action, so resolve its registered '/' before those controllers.
     * CONTRACT: physical '/' and F7 select the same list.search action. */
    action = trainlog_actions_find_key(&app->actions, key);
    if (action != NULL && action->intent == TRAINLOG_INTENT_OPEN_SEARCH &&
        app_shell_is_list_route(app->navigation.current.route)) {
        app->search.open = true;
        app->search.focused = true;
        app->focus = TRAINLOG_FOCUS_SEARCH;
        return;
    }
    if (app_shell_dispatch_exercise_controller(app, key)) return;
    if (app_shell_dispatch_equipment_controller(app, key)) return;
    if (app_shell_dispatch_body_controller(app, key)) return;
    if (app_shell_dispatch_profile_controller(app, key)) return;
    if (app_shell_dispatch_session(app, key)) return;
    if (app_shell_dispatch_sync(app, key)) return;
    if ((app->navigation.current.route == TRAINLOG_ROUTE_SESSIONS ||
         app->navigation.current.route == TRAINLOG_ROUTE_STATS) &&
        (key == TRAINLOG_KEY_UP || key == TRAINLOG_KEY_DOWN)) {
        size_t count = 3U;
        if (key == TRAINLOG_KEY_UP && app->content_selected > 0U) --app->content_selected;
        if (key == TRAINLOG_KEY_DOWN && app->content_selected + 1U < count) ++app->content_selected;
        return;
    }
    if (app_shell_is_list_route(app->navigation.current.route)) {
        if ((key == 'a' || key == 'A') &&
            app->navigation.current.route == TRAINLOG_ROUTE_EXERCISES) {
            exercise_controller_start_create(app, false);
            return;
        }
        if ((key == 'n' || key == 'N') &&
            app->navigation.current.route == TRAINLOG_ROUTE_EQUIPMENT) {
            equipment_controller_start(app);
            return;
        }
        if (key == TRAINLOG_KEY_UP) {
            trainlog_list_move(&app->list, app->stable_ids, -1); return;
        }
        if (key == TRAINLOG_KEY_DOWN) {
            trainlog_list_move(&app->list, app->stable_ids, 1); return;
        }
        if (key == TRAINLOG_KEY_PAGE_UP) {
            trainlog_list_move(&app->list, app->stable_ids,
                -(int)app->list.visible_rows); return;
        }
        if (key == TRAINLOG_KEY_PAGE_DOWN) {
            trainlog_list_move(&app->list, app->stable_ids,
                (int)app->list.visible_rows); return;
        }
        if ((key == 'z' || key == 'Z') &&
            app->navigation.current.route == TRAINLOG_ROUTE_EXERCISES) {
            size_t zone_count = trainlog_body_zone_catalog_count();
            ++app->exercise_zone_filter;
            if (app->exercise_zone_filter > (int)zone_count)
                app->exercise_zone_filter = -1;
            app_shell_refresh_list(app); return;
        }
        if ((key == 'x' || key == 'X') &&
            app->navigation.current.route == TRAINLOG_ROUTE_EXERCISES) {
            trainlog_search_init(&app->search); app->exercise_zone_filter = -1;
            app_shell_refresh_list(app); return;
        }
    }
    if (app->navigation.current.route == TRAINLOG_ROUTE_BODY) {
        if (key == 'a' || key == 'A') {
            body_controller_start_add(app); return;
        }
        if ((key == 'e' || key == 'E') && app->loaded_count > 0U) {
            (void)body_controller_start_edit(app,
                app->body_records[app->list.selected_index].observation_id);
            return;
        }
        if (key == 'v' || key == 'V') {
            app_shell_open_route(app, TRAINLOG_ROUTE_BODY_ANALYTICS); return;
        }
        if (key == 'm' || key == 'M') {
            app_shell_open_route(app, TRAINLOG_ROUTE_BODY_METRIC); return;
        }
        if (key == 'g' || key == 'G') {
            app->body_metric_selected = 0U;
            app_shell_open_route(app, TRAINLOG_ROUTE_BODY_TRENDS); return;
        }
        if (key == 'o' || key == 'O') {
            app_shell_open_route(app, TRAINLOG_ROUTE_BODY_GLOBAL); return;
        }
    }
    if (app->navigation.current.route == TRAINLOG_ROUTE_BODY_DETAIL) {
        if (key == 'e' || key == 'E') {
            (void)body_controller_start_edit(app,
                app->body_detail.observation_id); return;
        }
        if (key == TRAINLOG_KEY_UP && app->content_scroll > 0U) {
            --app->content_scroll; return;
        }
        if (key == TRAINLOG_KEY_DOWN && app->content_scroll + 1U < 14U) {
            ++app->content_scroll; return;
        }
    }
    if (app->navigation.current.route == TRAINLOG_ROUTE_BODY_METRIC) {
        if (key == TRAINLOG_KEY_LEFT) {
            app->body_metric_selected = app->body_metric_selected > 0U
                ? app->body_metric_selected - 1U : 13U;
            app_shell_load_body_metric(app); return;
        }
        if (key == TRAINLOG_KEY_RIGHT) {
            app->body_metric_selected = (app->body_metric_selected + 1U) % 14U;
            app_shell_load_body_metric(app); return;
        }
    }
    if (app->navigation.current.route == TRAINLOG_ROUTE_BODY_GLOBAL) {
        if (key == TRAINLOG_KEY_UP)
            app->body_global_selected = app->body_global_selected > 0U
                ? app->body_global_selected - 1U : 13U;
        else if (key == TRAINLOG_KEY_DOWN)
            app->body_global_selected = (app->body_global_selected + 1U) % 14U;
        else if (key == ' ')
            app->body_global_enabled[app->body_global_selected] =
                !app->body_global_enabled[app->body_global_selected];
        else goto body_global_unhandled;
        return;
body_global_unhandled:;
    }
    if (app->navigation.current.route == TRAINLOG_ROUTE_BODY_ANALYTICS) {
        if (key == TRAINLOG_KEY_LEFT || key == TRAINLOG_KEY_RIGHT) {
            app->content_selected = app->content_selected == 0U ? 1U : 0U;
            return;
        }
        if (key == 'p' || key == 'P') {
            profile_controller_start(app, TRAINLOG_ROUTE_BODY_ANALYTICS); return;
        }
    }
    if (app->navigation.current.route == TRAINLOG_ROUTE_EXERCISE_DETAIL) {
        size_t equipment_count = app->exercise_explicit_equipment_count +
            app->exercise_historic_equipment_count;
        if (key == TRAINLOG_KEY_UP && app->exercise_equipment_selected > 0U) {
            --app->exercise_equipment_selected;
            return;
        }
        if (key == TRAINLOG_KEY_DOWN &&
            app->exercise_equipment_selected + 1U < equipment_count) {
            ++app->exercise_equipment_selected;
            return;
        }
        if ((key == TRAINLOG_KEY_ENTER || key == '\n') && equipment_count > 0U) {
            size_t selected = app->exercise_equipment_selected;
            app->equipment_detail = selected < app->exercise_explicit_equipment_count
                ? app->exercise_explicit_equipment[selected]
                : app->exercise_historic_equipment[
                    selected - app->exercise_explicit_equipment_count];
            (void)trainlog_navigation_open(&app->navigation,
                TRAINLOG_ROUTE_EQUIPMENT_DETAIL,
                app->equipment_detail.equipment_id);
            return;
        }
        if (key == 'k' || key == 'K') {
            (void)trainlog_navigation_open(&app->navigation,
                TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE,
                app->exercise_detail.exercise_id); return;
        }
        if (key == 'p' || key == 'P') {
            app_shell_open_exercise_analytics(app,
                TRAINLOG_ROUTE_EXERCISE_PERFORMANCE); return;
        }
        if (key == 'm' || key == 'M') {
            app_shell_open_exercise_analytics(app, TRAINLOG_ROUTE_EXERCISE_MAX); return;
        }
        if (key == 'e' || key == 'E') {
            exercise_controller_start_edit(app);
            return;
        }
        if (key == 'u' || key == 'U') {
            exercise_controller_start_merge(app);
            return;
        }
    }
    if (app->navigation.current.route == TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE) {
        size_t page = app->layout.content.height > 4
            ? (size_t)(app->layout.content.height - 4) : 1U;
        if (key == TRAINLOG_KEY_UP && app->content_scroll > 0U) --app->content_scroll;
        else if (key == TRAINLOG_KEY_DOWN) ++app->content_scroll;
        else if (key == TRAINLOG_KEY_PAGE_UP)
            app->content_scroll = app->content_scroll > page ? app->content_scroll - page : 0U;
        else if (key == TRAINLOG_KEY_PAGE_DOWN) app->content_scroll += page;
        else if (key == 'k' || key == 'K') (void)app_shell_back(app);
        else goto shell_nonknowledge_input;
        return;
    }
shell_nonknowledge_input:
    if (app->navigation.current.route == TRAINLOG_ROUTE_EXERCISE_PERFORMANCE ||
        app->navigation.current.route == TRAINLOG_ROUTE_EXERCISE_MAX) {
        size_t page = app->layout.content.height > 10
            ? (size_t)(app->layout.content.height - 10) : 1U;
        if (key == TRAINLOG_KEY_UP && app->content_scroll > 0U) --app->content_scroll;
        else if (key == TRAINLOG_KEY_DOWN &&
            app->content_scroll + 1U < app->performance_count) ++app->content_scroll;
        else if (key == TRAINLOG_KEY_PAGE_UP)
            app->content_scroll = app->content_scroll > page ? app->content_scroll - page : 0U;
        else if (key == TRAINLOG_KEY_PAGE_DOWN && app->performance_count > 0U)
            app->content_scroll = app->content_scroll + page < app->performance_count
                ? app->content_scroll + page : app->performance_count - 1U;
        else if ((key == 'r' || key == 'R') &&
            app->navigation.current.route == TRAINLOG_ROUTE_EXERCISE_MAX)
            app->max_rounding_index = (app->max_rounding_index + 1U) % 4U;
        else goto shell_nonanalytics_input;
        return;
    }
shell_nonanalytics_input:
    if (app->navigation.current.route == TRAINLOG_ROUTE_SESSION_DETAIL &&
        !app->session_detail_error) {
        TrainlogPersistedExerciseDetail *entry = app->session_entry_count > 0U
            ? &app->session_entries[app->session_entry_selected] : NULL;
        if ((key == TRAINLOG_KEY_LEFT || key == TRAINLOG_KEY_UP) &&
            app->session_entry_count > 0U) {
            app->session_entry_selected = app->session_entry_selected > 0U
                ? app->session_entry_selected - 1U : app->session_entry_count - 1U;
            app->session_set_scroll = 0U; return;
        }
        if ((key == TRAINLOG_KEY_RIGHT || key == TRAINLOG_KEY_DOWN) &&
            app->session_entry_count > 0U) {
            app->session_entry_selected = (app->session_entry_selected + 1U) %
                app->session_entry_count;
            app->session_set_scroll = 0U; return;
        }
        if (key == TRAINLOG_KEY_PAGE_UP && app->session_set_scroll > 0U) {
            --app->session_set_scroll; return;
        }
        if (key == TRAINLOG_KEY_PAGE_DOWN && entry != NULL &&
            app->session_set_scroll + 1U < entry->actual_set_count) {
            ++app->session_set_scroll; return;
        }
        if ((key == 'i' || key == 'I') && entry != NULL &&
            entry->equipment_id[0] != '\0') {
            if (trainlog_database_resolve_equipment(app->database,
                entry->equipment_id, &app->equipment_detail) == TRAINLOG_STATUS_OK)
                (void)trainlog_navigation_open(&app->navigation,
                    TRAINLOG_ROUTE_EQUIPMENT_DETAIL,
                    app->equipment_detail.equipment_id);
            return;
        }
        if (key == 'e' || key == 'E') {
            char session_id[TRAINLOG_ID_MAX + 1U];
            (void)snprintf(session_id, sizeof(session_id), "%s",
                app->session_detail.session_id);
            if (app->session.has_draft) {
                app_shell_open_route(app, TRAINLOG_ROUTE_SESSION_CURRENT);
                (void)snprintf(app->session.message, sizeof(app->session.message),
                    "Une séance est déjà en cours; elle n’a pas été remplacée.");
            } else {
                TrainlogSessionSummary persisted;
                if (load_persisted_draft(app->database, session_id, &persisted,
                    app->session.drafts, &app->session.draft_count)) {
                    app->session.has_draft = true;
                    app->session.dirty = false;
                    app->session.correcting = true;
                    app->session.session_type = persisted.session_type;
                    (void)snprintf(app->session.persisted_session_id,
                        sizeof(app->session.persisted_session_id), "%s", session_id);
                    app->session.phase = TRAINLOG_SESSION_DRAFT;
                    session_controller_sync_durability(&app->session);
                    app_shell_open_route(app, TRAINLOG_ROUTE_SESSION_CURRENT);
                }
            }
            return;
        }
    }
    if (key == TRAINLOG_KEY_ENTER || key == '\n') { app_shell_primary(app); return; }
    if (key == TRAINLOG_KEY_HOME) key = '0';
    else if (key == TRAINLOG_KEY_F1) key = '1';
    else if (key == TRAINLOG_KEY_F2) key = '2';
    else if (key == TRAINLOG_KEY_F3) key = '3';
    else if (key == TRAINLOG_KEY_F4) key = '4';
    else if (key == TRAINLOG_KEY_F5) key = '5';
    action = trainlog_actions_find_key(&app->actions, key);
    if (action != NULL && action->intent == TRAINLOG_INTENT_OPEN_ROUTE) {
        app_shell_open_route(app, action->route); return;
    }
    else if (key == TRAINLOG_KEY_ESCAPE || key == 'b' || key == 'B') {
        if (!app_shell_back(app)) app_shell_open_route(app, TRAINLOG_ROUTE_HOME);
    } else if ((key == 'q' || key == 'Q') && app->navigation.current.route == TRAINLOG_ROUTE_HOME) {
        session_controller_sync_durability(&app->session);
        if (app->session.has_draft || app->session.generated_preview ||
            app->session.generation_zone != NULL ||
            app->session.durability.transient_form_dirty) {
            app->quit_confirmation = true;
            (void)trainlog_overlays_push(&app->overlays,
                TRAINLOG_OVERLAY_CONFIRMATION, app->focus,
                app->navigation.current.stable_id);
        } else app->running = false;
    }
}

int trainlog_tui_run(TrainlogDatabase *database)
{
    TrainlogAppContext app;
    if (database == NULL) {
        return 1;
    }

    (void)setlocale(LC_ALL, "");

    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.terminal = trainlog_terminal_create();
    if (app.terminal == NULL) {
        return 1;
    }

    trainlog_terminal_cursor_visible(app.terminal, false);
    trainlog_navigation_init(&app.navigation);
    trainlog_overlays_init(&app.overlays);
    trainlog_list_init(&app.list);
    trainlog_search_init(&app.search);
    trainlog_sync_screen_state_init(&app.sync_controller.action);
    app.sync_controller.probe_status = TRAINLOG_STATUS_NOT_FOUND;
    app.sync_controller.probe = trainlog_sync_probe;
    app.sync_controller.run = trainlog_sync_run;
    app.exercise_zone_filter = -1;
    app.focus = TRAINLOG_FOCUS_CONTENT;
    app.running = true;
    /* CONTRACT: this is the single shell event pump. Controllers return to it
     * after an explicit action; navigation and resize never write domain data. */
    while (app.running) {
        int key;
        app_shell_actions(&app);
        app_shell_render(&app);
        if (!app.running) break;
        key = trainlog_terminal_get_key(app.terminal);
        if (key == TRAINLOG_KEY_NONE) continue;
        if (key == TRAINLOG_KEY_RESIZE) {
            if (!trainlog_terminal_refresh_geometry(app.terminal)) {
                app.running = false;
                break;
            }
            continue;
        }
        app_shell_dispatch(&app, key);
    }
    app_shell_destroy_surfaces(&app);
    app_shell_release_session_detail(&app);
    trainlog_terminal_destroy(app.terminal);
    return 0;
}
