/**
 * @file tui.c
 * @brief First usable Trainlog ncurses interface.
 */

#include "trainlog/tui.h"

#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curses.h>

#include "trainlog/bodyviz.h"
#include "trainlog/catalog.h"
#include "trainlog/duration.h"
#include "trainlog/id.h"
#include "trainlog/theme.h"
#include "trainlog/timeutil.h"

#define MAX_EXERCISES 128U
#define MAX_SESSION_EXERCISES 32U
#define MAX_SETS_PER_EXERCISE 64U
#define MAX_SESSIONS 128U
#define MAX_WEIGHT_POINTS 256U

#define MAX_BODY_METRIC_POINTS 256U


/* TRAINLOG_TUI_V02_POLISH */

typedef enum DashboardAction {
    DASHBOARD_NEW_SESSION = 0,
    DASHBOARD_HISTORY,
    DASHBOARD_EXERCISES,
    DASHBOARD_BODY,
    DASHBOARD_QUIT
} DashboardAction;

/* TRAINLOG_DASHBOARD_FORWARD_DECLARATIONS */
static DashboardAction screen_dashboard(
    TrainlogDatabase *database
);

static void screen_exercises(
    TrainlogDatabase *database
);

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


static void draw_shell(const char *heading, const char *footer)
{
    erase();
    box(stdscr, 0, 0);

    attron(A_BOLD | trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT));
    mvprintw(1, 2, " %s ", heading);
    attroff(A_BOLD | trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT));

    attron(trainlog_theme_attribute(TRAINLOG_COLOR_MUTED));
    mvprintw(LINES - 2, 2, "%-*s", COLS - 4, footer);
    attroff(trainlog_theme_attribute(TRAINLOG_COLOR_MUTED));
}

static void wait_key(void)
{
    attron(trainlog_theme_attribute(TRAINLOG_COLOR_MUTED));
    mvprintw(LINES - 2, 2, "Appuyez sur une touche pour continuer...");
    attroff(trainlog_theme_attribute(TRAINLOG_COLOR_MUTED));
    refresh();
    (void)getch();
}

static void title(const char *text)
{
    attron(A_BOLD | trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT));
    mvprintw(1, 2, "%s", text);
    attroff(A_BOLD | trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT));
}

static void status_line(const char *text, TrainlogColorRole role)
{
    attron(trainlog_theme_attribute(role));
    mvprintw(LINES - 3, 2, "%-*s", COLS - 4, text);
    attroff(trainlog_theme_attribute(role));
}

static bool prompt_text(
    int row,
    const char *label,
    char *output,
    size_t output_size,
    bool allow_empty
)
{
    int rc;

    mvprintw(row, 2, "%s", label);
    clrtoeol();
    echo();
    curs_set(1);
    rc = getnstr(output, (int)(output_size - 1U));
    noecho();
    curs_set(0);

    if (rc == ERR) {
        return false;
    }

    if (!allow_empty && output[0] == '\0') {
        return false;
    }

    return true;
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
        refresh();
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
        refresh();
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
    int height
)
{
    size_t indices[EXERCISE_GRAPH_POINTS];
    size_t selected_count = 0U;
    size_t index;
    double minimum = 0.0;
    double maximum = 0.0;
    const int left = 11;
    int width =
        COLS - left - 4;

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
        mvprintw(
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
        attron(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_WARNING
            )
        );

        mvprintw(
            top,
            left,
            "Assistance (kg) — moins = mieux"
        );

        attroff(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_WARNING
            )
        );
    } else if (mode ==
               TRAINLOG_LOAD_EXTERNAL) {
        mvprintw(
            top,
            left,
            "Charge du meilleur set (kg)"
        );
    } else if (
        tracking_mode ==
        TRAINLOG_TRACKING_DURATION
    ) {
        mvprintw(
            top,
            left,
            "Meilleure durée"
        );
    } else {
        mvprintw(
            top,
            left,
            "Meilleures répétitions"
        );
    }

    mvprintw(
        top + 1,
        2,
        "%.1f",
        maximum
    );

    mvprintw(
        top + height - 1,
        2,
        "%.1f",
        minimum
    );

    attron(
        trainlog_theme_attribute(
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

                    mvaddch(
                        line_y,
                        line_x,
                        (chtype)'.'
                    );
                }
            }

            mvaddch(
                y,
                x,
                chronological + 1U ==
                    selected_count
                    ? (chtype)'O'
                    : (chtype)'*'
            );

            previous_x = x;
            previous_y = y;
        }
    }

    attroff(
        trainlog_theme_attribute(
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

        mvprintw(
            top + height,
            left,
            "%s",
            oldest
        );

        mvprintw(
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
    WINDOW *panel;
    int height;
    int width;

    if (top < 0 ||
        left < 0 ||
        bottom <= top ||
        right <= left ||
        bottom >= LINES ||
        right >= COLS) {
        return;
    }

    height = bottom - top + 1;
    width = right - left + 1;

    panel = derwin(
        stdscr,
        height,
        width,
        top,
        left
    );

    if (panel == NULL) {
        return;
    }

    box(panel, 0, 0);

    if (label != NULL &&
        label[0] != '\0' &&
        width > 8) {
        wattron(
            panel,
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );

        mvwprintw(
            panel,
            0,
            2,
            " %.*s ",
            width - 6,
            label
        );

        wattroff(
            panel,
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );
    }

    syncok(panel, TRUE);
    wsyncup(panel);
    delwin(panel);
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

    bool framed =
        COLS >= 100 &&
        LINES >= 30;

    int summary_top = 3;
    int summary_bottom =
        framed ? 9 : 8;

    int graph_top =
        framed ? 11 : 10;

    int graph_bottom =
        framed ? 20 : 18;

    int history_top =
        framed ? 22 : 19;

    int history_bottom =
        LINES - 4;

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

    draw_shell(
        "TRAINLOG — Performance exercice",
        "b/Échap retour"
    );

    if (framed) {
        exercise_panel(
            summary_top,
            2,
            summary_bottom,
            COLS - 3,
            "PERFORMANCE"
        );

        exercise_panel(
            graph_top,
            2,
            graph_bottom,
            COLS - 3,
            "EVOLUTION"
        );

        if (history_bottom >
            history_top + 2) {
            exercise_panel(
                history_top,
                2,
                history_bottom,
                COLS - 3,
                "HISTORIQUE"
            );
        }
    }

    attron(
        A_BOLD |
        trainlog_theme_attribute(
            TRAINLOG_COLOR_ACCENT
        )
    );

    mvprintw(
        framed ? summary_top + 1 : 3,
        framed ? 5 : 4,
        "%s",
        exercise->name
    );

    attroff(
        A_BOLD |
        trainlog_theme_attribute(
            TRAINLOG_COLOR_ACCENT
        )
    );

    mvprintw(
        framed ? summary_top + 2 : 4,
        framed ? 5 : 4,
        "Séances enregistrées : %zu",
        count
    );

    if (latest == NULL) {
        mvprintw(
            framed ? summary_top + 3 : 5,
            framed ? 5 : 4,
            "Aucune série réussie enregistrée."
        );
    } else {
        mvprintw(
            framed ? summary_top + 3 : 5,
            framed ? 5 : 4,
            "Mode suivi : %s",
            exercise_load_mode_label(
                graph_mode
            )
        );

        mvprintw(
            framed ? summary_top + 4 : 6,
            framed ? 5 : 4,
            "Dernier meilleur set : %.*s",
            COLS - 30,
            latest_text
        );

        mvprintw(
            framed ? summary_top + 5 : 7,
            framed ? 5 : 4,
            "Meilleur set enregistré : %.*s",
            COLS - 33,
            best_text
        );

        if (graph_mode !=
            TRAINLOG_LOAD_NONE) {
            attron(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );

            mvprintw(
                framed ? summary_bottom - 1 : 8,
                framed ? 5 : 4,
                "Meilleur set ≠ max mesuré."
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );
        }
    }

    if (latest != NULL) {
        int graph_content_top =
            framed
                ? graph_top + 1
                : graph_top;

        int graph_height =
            framed
                ? graph_bottom -
                    graph_top -
                    2
                : 7;

        draw_exercise_performance_graph(
            points,
            count,
            graph_mode,
            exercise->tracking_mode,
            graph_content_top,
            graph_height
        );
    }

    if (framed) {
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

            mvprintw(
                history_first_row +
                    (int)index,
                5,
                "%s  %-13s  %.*s",
                date,
                exercise_load_mode_label(
                    points[index].load_mode
                ),
                COLS - 40,
                summary
            );
        }
    } else {
        history_limit =
            LINES > 12
                ? (size_t)(LINES - 12)
                : 0U;

        mvprintw(
            19,
            4,
            "Dernières séances :"
        );

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

            mvprintw(
                20 + (int)index,
                6,
                "%s  %.*s",
                date,
                COLS - 20,
                summary
            );
        }
    }

    refresh();

    for (;;) {
        int key = getch();

        if (key == 'b' ||
            key == 27 ||
            key == '\n' ||
            key == KEY_ENTER) {
            return;
        }
    }
}

static void screen_exercises(TrainlogDatabase *database)
{
    TrainlogExercise exercises[MAX_EXERCISES];
    size_t selected = 0U;

    for (;;) {
        size_t count = 0U;
        size_t top = 0U;
        size_t index;
        bool framed =
            COLS >= 90 &&
            LINES >= 24;

        int list_top =
            framed ? 3 : 3;

        int list_bottom =
            framed
                ? LINES - 4
                : LINES - 3;

        int visible_rows =
            framed
                ? list_bottom - list_top - 1
                : LINES - 7;

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
            selected = count - 1U;
        }

        if (count > 0U &&
            selected >= (size_t)visible_rows) {
            top =
                selected -
                (size_t)visible_rows +
                1U;
        }

        draw_shell(
            "TRAINLOG — Exercices",
            "↑↓ naviguer  Entrée performance  a ajouter  b/Échap retour"
        );

        if (framed) {
            exercise_panel(
                list_top,
                2,
                list_bottom,
                COLS - 3,
                "CATALOGUE"
            );
        }

        if (count == 0U) {
            mvprintw(
                framed ? list_top + 2 : 4,
                5,
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
                (framed ? list_top + 1 : 3) +
                (int)index;

            int item_col =
                framed ? 5 : 4;

            if (absolute == selected) {
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            mvprintw(
                item_row,
                item_col,
                " %-42s [%s] ",
                exercises[absolute].name,
                exercises[absolute].tracking_mode ==
                    TRAINLOG_TRACKING_REPS
                    ? "reps"
                    : "durée"
            );

            if (absolute == selected) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        refresh();
        key = getch();

        if (key == 'b' ||
            key == 27) {
            return;
        }

        if (count > 0U &&
            key == KEY_UP) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : count - 1U;
        } else if (
            count > 0U &&
            key == KEY_DOWN
        ) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : 0U;
        } else if (
            count > 0U &&
            (key == '\n' ||
             key == KEY_ENTER)
        ) {
            screen_exercise_performance(
                database,
                &exercises[selected]
            );
        } else if (key == 'a') {
            char name[TRAINLOG_NAME_MAX + 1U];
            int mode = 1;
            TrainlogExercise created;
            TrainlogStatus status;

            draw_shell(
                "Nouvel exercice",
                "Entrée valide chaque champ"
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

            status =
                trainlog_catalog_create_exercise(
                    database,
                    name,
                    mode == 1
                        ? TRAINLOG_TRACKING_REPS
                        : TRAINLOG_TRACKING_DURATION,
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

static bool choose_exercise(
    TrainlogDatabase *database,
    TrainlogExercise *output
)
{
    TrainlogExercise exercises[MAX_EXERCISES];
    size_t count = 0U;
    size_t selected = 0U;

    if (trainlog_database_list_exercises(
            database,
            exercises,
            MAX_EXERCISES,
            &count
        ) != TRAINLOG_STATUS_OK ||
        count == 0U) {
        return false;
    }

    for (;;) {
        size_t index;
        size_t top = 0U;
        int visible_rows = LINES - 7;
        int key;

        if (visible_rows < 1) {
            return false;
        }

        if (selected >= (size_t)visible_rows) {
            top = selected - (size_t)visible_rows + 1U;
        }

        draw_shell(
            "Choisir un exercice",
            "↑↓ naviguer  Entrée choisir  Échap annuler"
        );

        for (index = 0U;
             index < (size_t)visible_rows &&
             top + index < count;
             ++index) {
            size_t absolute = top + index;
            int item_row = 3 + (int)index;

            if (absolute == selected) {
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT)
                );
            }

            mvprintw(
                item_row,
                4,
                " %-42s [%s] ",
                exercises[absolute].name,
                exercises[absolute].tracking_mode == TRAINLOG_TRACKING_REPS
                    ? "reps"
                    : "durée"
            );

            if (absolute == selected) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT)
                );
            }
        }

        refresh();
        key = getch();

        if (key == KEY_UP) {
            selected = selected > 0U ? selected - 1U : count - 1U;
        } else if (key == KEY_DOWN) {
            selected = selected + 1U < count ? selected + 1U : 0U;
        } else if (key == '\n' || key == KEY_ENTER) {
            *output = exercises[selected];
            return true;
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
        refresh();
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
        mvprintw(row, 4, "Aucune donnée pour cette mesure.");
        return;
    }

    width = COLS - 14;
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

        mvprintw(graph_row, 2, "%.1f %s", minimum, unit);

        attron(trainlog_theme_attribute(TRAINLOG_COLOR_GRAPH));

        for (index = start; index < count; ++index) {
            int x = 12 + (int)(index - start);

            if (x < COLS - 2) {
                mvaddch(graph_row, x, (chtype)'*');
            }
        }

        attroff(trainlog_theme_attribute(TRAINLOG_COLOR_GRAPH));
        return;
    }

    mvprintw(row, 2, "%.1f", maximum);
    mvprintw(row + height - 1, 2, "%.1f", minimum);

    attron(trainlog_theme_attribute(TRAINLOG_COLOR_GRAPH));

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

        if (x < COLS - 2) {
            mvaddch(y, x, (chtype)'*');
        }
    }

    attroff(trainlog_theme_attribute(TRAINLOG_COLOR_GRAPH));
}

static bool body_metric_pair(
    TrainlogBodyMetric metric,
    TrainlogBodyMetric *left,
    TrainlogBodyMetric *right,
    const char **label
)
{
    if (left == NULL || right == NULL || label == NULL) {
        return false;
    }

    switch (metric) {
    case TRAINLOG_BODY_METRIC_LEFT_ARM:
    case TRAINLOG_BODY_METRIC_RIGHT_ARM:
        *left = TRAINLOG_BODY_METRIC_LEFT_ARM;
        *right = TRAINLOG_BODY_METRIC_RIGHT_ARM;
        *label = "Bras";
        return true;

    case TRAINLOG_BODY_METRIC_LEFT_FOREARM:
    case TRAINLOG_BODY_METRIC_RIGHT_FOREARM:
        *left = TRAINLOG_BODY_METRIC_LEFT_FOREARM;
        *right = TRAINLOG_BODY_METRIC_RIGHT_FOREARM;
        *label = "Avant-bras";
        return true;

    case TRAINLOG_BODY_METRIC_LEFT_THIGH:
    case TRAINLOG_BODY_METRIC_RIGHT_THIGH:
        *left = TRAINLOG_BODY_METRIC_LEFT_THIGH;
        *right = TRAINLOG_BODY_METRIC_RIGHT_THIGH;
        *label = "Cuisses";
        return true;

    case TRAINLOG_BODY_METRIC_LEFT_CALF:
    case TRAINLOG_BODY_METRIC_RIGHT_CALF:
        *left = TRAINLOG_BODY_METRIC_LEFT_CALF;
        *right = TRAINLOG_BODY_METRIC_RIGHT_CALF;
        *label = "Mollets";
        return true;

    default:
        return false;
    }
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
        "Laissez vide les mesures non faites aujourd'hui"
    );

    row = 4;

#define BODY_PROMPT(label_, flag_, value_)                                   \
    do {                                                                     \
        (void)prompt_optional_double(                                        \
            row++,                                                           \
            (label_),                                                        \
            &(flag_),                                                        \
            &(value_)                                                        \
        );                                                                   \
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
        "Laissez vide les mesures non faites aujourd'hui"
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
    chtype current;
    chtype character;

    if (row < 0 ||
        row >= LINES ||
        column < 0 ||
        column >= COLS) {
        return;
    }

    current =
        mvinch(row, column) &
        A_CHARTEXT;

    character =
        (chtype)(unsigned char)symbol;

    if (current != (chtype)' ' &&
        current != (chtype)'.' &&
        current != character) {
        character = (chtype)'#';
    }

    attron(trainlog_theme_attribute(role));
    mvaddch(row, column, character);
    attroff(trainlog_theme_attribute(role));
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
        COLS - graph_left - 3;

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
        mvprintw(
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

    mvprintw(
        3,
        4,
        "Vue globale — première mesure de chaque série = 100"
    );

    if (minimum == maximum) {
        minimum = 99.0;
        maximum = 101.0;
    }

    mvprintw(
        graph_top,
        2,
        "%.1f",
        maximum
    );

    mvprintw(
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

        attron(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        for (column = graph_left;
             column <
                graph_left + graph_width;
             ++column) {
            mvaddch(
                baseline_row,
                column,
                (chtype)'.'
            );
        }

        attroff(
            trainlog_theme_attribute(
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
                    : (COLS / 2);

            attron(
                trainlog_theme_attribute(
                    item->role
                )
            );

            mvprintw(
                row,
                column,
                "%c %-16s %+.1f%%",
                item->symbol,
                BODY_METRICS[metric_index].label,
                item->latest_percent
            );

            attroff(
                trainlog_theme_attribute(
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

    attron(
        trainlog_theme_attribute(
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

            if (x > COLS - 7) {
                x = COLS - 7;
            }

            mvprintw(
                row,
                x,
                "%s",
                label
            );
        } else {
            if (x + 2 < COLS - 1) {
                mvprintw(
                    row,
                    x,
                    "%02d",
                    months[index].month
                );
            }
        }
    }

    attroff(
        trainlog_theme_attribute(
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
    WINDOW *panel;
    int height;
    int width;

    if (top < 0 ||
        left < 0 ||
        bottom <= top ||
        right <= left ||
        bottom >= LINES ||
        right >= COLS) {
        return;
    }

    height = bottom - top + 1;
    width = right - left + 1;

    panel = derwin(
        stdscr,
        height,
        width,
        top,
        left
    );

    if (panel == NULL) {
        return;
    }

    box(panel, 0, 0);

    if (label != NULL &&
        label[0] != '\0' &&
        width > 8) {
        wattron(
            panel,
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );

        mvwprintw(
            panel,
            0,
            2,
            " %.*s ",
            width - 6,
            label
        );

        wattroff(
            panel,
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );
    }

    /*
     * derwin() shares the parent screen storage. syncok()+wsyncup() makes
     * the panel border part of stdscr, so later dashboard content and one
     * final refresh() compose cleanly.
     */
    syncok(panel, TRUE);
    wsyncup(panel);
    delwin(panel);
}

static void dashboard_ascii_header(void)
{
    static const char *const logo[] = {
        "TTTTT RRRR   AAA  IIIII N   N L       OOO   GGG ",
        "  T   R   R A   A   I   NN  N L      O   O G    ",
        "  T   RRRR  AAAAA   I   N N N L      O   O G  GG",
        "  T   R  R  A   A   I   N  NN L      O   O G   G",
        "  T   R   R A   A IIIII N   N LLLLL   OOO   GGG "
    };

    const size_t line_count =
        sizeof(logo) /
        sizeof(logo[0]);

    size_t index;

    attron(
        A_BOLD |
        trainlog_theme_attribute(
            TRAINLOG_COLOR_ACCENT
        )
    );

    for (index = 0U;
         index < line_count;
         ++index) {
        int width =
            (int)strlen(logo[index]);

        int column =
            (COLS - width) / 2;

        if (column < 2) {
            column = 2;
        }

        mvprintw(
            1 + (int)index,
            column,
            "%.*s",
            COLS - column - 2,
            logo[index]
        );
    }

    attroff(
        A_BOLD |
        trainlog_theme_attribute(
            TRAINLOG_COLOR_ACCENT
        )
    );

    attron(
        trainlog_theme_attribute(
            TRAINLOG_COLOR_MUTED
        )
    );

    {
        const char *label =
            ":: D A S H B O A R D ::";
        int width =
            (int)strlen(label);
        int column =
            (COLS - width) / 2;

        if (column < 2) {
            column = 2;
        }

        mvprintw(
            6,
            column,
            "%s",
            label
        );
    }

    attroff(
        trainlog_theme_attribute(
            TRAINLOG_COLOR_MUTED
        )
    );
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
        COLS >= 100 &&
        LINES >= 30;

    int graph_panel_top =
        large_layout ? 8 : 2;

    int graph_panel_bottom =
        large_layout ? 17 : 9;

    int graph_top =
        graph_panel_top + 1;

    int graph_height =
        graph_panel_bottom -
        graph_panel_top -
        3;

    int graph_left =
        large_layout ? 10 : 8;

    int graph_right =
        COLS - 5;

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
            COLS - 3,
            "EVOLUTION CORPORELLE - 12 MOIS"
        );

        dashboard_panel(
            legend_panel_top,
            2,
            legend_panel_bottom,
            COLS - 3,
            "MESURES"
        );
    } else {
        attron(
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );

        mvprintw(
            1,
            3,
            "TRAINLOG :: DASHBOARD"
        );

        attroff(
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );
    }

    if (!dashboard_current_month_key(
            &current_key
        )) {
        mvprintw(
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
        attron(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        mvprintw(
            graph_top + 1,
            graph_left,
            "Premières courbes après 2 mois relevés pour une même mesure."
        );

        mvprintw(
            graph_top + 2,
            graph_left,
            "Un mois sans relevé reste vide."
        );

        attroff(
            trainlog_theme_attribute(
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

        mvprintw(
            graph_top,
            3,
            "%+.1f%%",
            maximum
        );

        mvprintw(
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

            attron(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );

            for (column = graph_left;
                 column <= graph_right;
                 ++column) {
                mvaddch(
                    zero_row,
                    column,
                    (chtype)'.'
                );
            }

            attroff(
                trainlog_theme_attribute(
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
            (COLS - 8) /
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

            attron(
                trainlog_theme_attribute(
                    item->role
                )
            );

            mvprintw(
                row,
                column,
                "%c %-*.*s %6.1f",
                item->symbol,
                label_width,
                label_width,
                label,
                latest
            );

            mvprintw(
                row,
                column + cell_width - 7,
                "%6s",
                evolution
            );

            attroff(
                trainlog_theme_attribute(
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
    static const char *const labels[] = {
        "1 Séance",
        "2 Historique",
        "3 Exercices",
        "4 Corps"
    };

    static const char *const footer =
        "←→ naviguer  Entrée ouvrir  F1-F4 accès direct  q quitter";

    int selected = 0;

    for (;;) {
        bool large_layout =
            COLS >= 100 &&
            LINES >= 30;

        int nav_top =
            large_layout ? 25 : LINES - 4;

        int nav_bottom =
            large_layout ? 27 : LINES - 3;

        int key;
        int index;
        int column;

        erase();
        box(stdscr, 0, 0);

        if (large_layout) {
            dashboard_ascii_header();

            dashboard_panel(
                nav_top,
                2,
                nav_bottom,
                COLS - 3,
                "NAVIGATION"
            );
        }

        draw_dashboard_body_graph(database);

        column =
            large_layout ? 6 : 3;

        for (index = 0;
             index < 4;
             ++index) {
            int width =
                (int)strlen(labels[index]) + 4;

            if (index == selected) {
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            mvprintw(
                large_layout
                    ? nav_top + 1
                    : LINES - 3,
                column,
                " %s ",
                labels[index]
            );

            if (index == selected) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            column += width + 3;
        }

        attron(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        mvprintw(
            LINES - 2,
            2,
            "%.*s",
            COLS - 4,
            footer
        );

        attroff(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        refresh();
        key = getch();

        switch (key) {
        case KEY_UP:
        case KEY_LEFT:
            selected =
                selected > 0
                    ? selected - 1
                    : 3;
            break;

        case KEY_DOWN:
        case KEY_RIGHT:
            selected =
                selected < 3
                    ? selected + 1
                    : 0;
            break;

        case '\n':
        case KEY_ENTER:
            return (DashboardAction)selected;

        case KEY_F(1):
        case '1':
            return DASHBOARD_NEW_SESSION;

        case KEY_F(2):
        case '2':
            return DASHBOARD_HISTORY;

        case KEY_F(3):
        case '3':
            return DASHBOARD_EXERCISES;

        case KEY_F(4):
        case '4':
            return DASHBOARD_BODY;

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
    int actual_sets;
    bool target_has_weight = false;
    double target_weight = 0.0;
    size_t set_index;

    if (!choose_exercise(database, &exercise)) {
        return false;
    }

    target_metric =
        exercise.tracking_mode == TRAINLOG_TRACKING_REPS
            ? 10
            : 45;

    draw_shell(
        exercise.name,
        "Durées : 90, 90s, 1:30, 1m30, 2m"
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
            ) || !target_has_weight) {
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

    if (exercise.tracking_mode == TRAINLOG_TRACKING_REPS) {
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

    (void)memset(output, 0, sizeof(*output));

    (void)snprintf(
        output->exercise_id,
        sizeof(output->exercise_id),
        "%s",
        exercise.exercise_id
    );

    output->load_mode =
        load_mode == 1
            ? TRAINLOG_LOAD_NONE
            : (load_mode == 2
                ? TRAINLOG_LOAD_EXTERNAL
                : TRAINLOG_LOAD_ASSISTANCE);

    output->rest_seconds = rest_seconds;
    output->target_sets = target_sets;

    output->target_reps =
        exercise.tracking_mode == TRAINLOG_TRACKING_REPS
            ? target_metric
            : 0;

    output->target_duration_seconds =
        exercise.tracking_mode == TRAINLOG_TRACKING_DURATION
            ? target_metric
            : 0;

    output->target_has_weight = target_has_weight;
    output->target_weight_kg = target_weight;
    output->sets = set_storage;
    output->set_count = (size_t)actual_sets;

    for (set_index = 0U; set_index < output->set_count; ++set_index) {
        int actual_metric = target_metric;

        (void)memset(
            &set_storage[set_index],
            0,
            sizeof(set_storage[set_index])
        );

        draw_shell(
            exercise.name,
            "Durées : 90, 90s, 1:30, 1m30, 2m"
        );

        mvprintw(
            3,
            4,
            "Série %zu / %zu",
            set_index + 1U,
            output->set_count
        );

        if (exercise.tracking_mode == TRAINLOG_TRACKING_REPS) {
            if (!prompt_int_value(
                    5,
                    "Répétitions réalisées",
                    0,
                    10000,
                    target_metric,
                    &actual_metric
                )) {
                return false;
            }

            set_storage[set_index].reps = actual_metric;
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

            set_storage[set_index].duration_seconds = actual_metric;
        }

        if (target_has_weight) {
            char buffer[64];
            char prompt[128];
            double actual_weight = target_weight;

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

            set_storage[set_index].has_weight = true;
            set_storage[set_index].weight_kg = actual_weight;
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

    if (output == NULL) {
        return false;
    }

    for (;;) {
        WINDOW *panel;
        int key;
        int panel_width =
            COLS - 8;

        if (panel_width < 40) {
            panel_width = 40;
        }

        draw_shell(
            "TRAINLOG — Nouvelle séance",
            "↑↓ choisir  Entrée valider  Échap annuler"
        );

        panel = derwin(
            stdscr,
            9,
            panel_width,
            3,
            4
        );

        if (panel == NULL) {
            return false;
        }

        box(panel, 0, 0);

        wattron(
            panel,
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );

        mvwprintw(
            panel,
            0,
            2,
            " TYPE DE SEANCE "
        );

        wattroff(
            panel,
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );

        if (selected == 0) {
            wattron(
                panel,
                A_REVERSE |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        }

        mvwprintw(
            panel,
            2,
            3,
            " Entraînement "
        );

        if (selected == 0) {
            wattroff(
                panel,
                A_REVERSE |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        }

        wattron(
            panel,
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        mvwprintw(
            panel,
            3,
            5,
            "Séance normale : progression, volume, travail courant."
        );

        wattroff(
            panel,
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        if (selected == 1) {
            wattron(
                panel,
                A_REVERSE |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        }

        mvwprintw(
            panel,
            5,
            3,
            " Test de max "
        );

        if (selected == 1) {
            wattroff(
                panel,
                A_REVERSE |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        }

        wattron(
            panel,
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        mvwprintw(
            panel,
            6,
            5,
            "Séance explicitement dédiée aux mesures de max."
        );

        wattroff(
            panel,
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        syncok(panel, TRUE);
        wsyncup(panel);
        delwin(panel);

        refresh();
        key = getch();

        switch (key) {
        case KEY_UP:
        case KEY_LEFT:
        case KEY_DOWN:
        case KEY_RIGHT:
            selected =
                selected == 0
                    ? 1
                    : 0;
            break;

        case '\n':
        case KEY_ENTER:
            *output =
                selected == 0
                    ? TRAINLOG_SESSION_TRAINING
                    : TRAINLOG_SESSION_MAX_TEST;
            return true;

        case 27:
            return false;

        default:
            break;
        }
    }
}

static void screen_new_session(TrainlogDatabase *database)
{
    TrainlogSessionExerciseInput exercise_inputs[MAX_SESSION_EXERCISES];
    TrainlogSetInput set_storage[MAX_SESSION_EXERCISES][MAX_SETS_PER_EXERCISE];
    TrainlogSessionInput session;
    char session_id[TRAINLOG_GENERATED_ID_CAPACITY];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char ended_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    TrainlogSessionType session_type =
        TRAINLOG_SESSION_TRAINING;
    size_t exercise_count = 0U;
    int another = 1;
    TrainlogStatus status;

    if (trainlog_database_exercise_count(
            database,
            &exercise_count
        ) != TRAINLOG_STATUS_OK) {
        return;
    }

    if (exercise_count == 0U) {
        erase();
        title("Nouvelle séance");
        status_line(
            "Ajoutez d'abord au moins un exercice.",
            TRAINLOG_COLOR_WARNING
        );
        wait_key();
        return;
    }

    exercise_count = 0U;

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

    while (another == 1 && exercise_count < MAX_SESSION_EXERCISES) {
        if (!build_session_exercise(
                database,
                &exercise_inputs[exercise_count],
                set_storage[exercise_count],
                MAX_SETS_PER_EXERCISE
            )) {
            break;
        }

        ++exercise_count;

        erase();
        title("Séance en cours");
        mvprintw(4, 2, "%zu exercice(s) enregistré(s).", exercise_count);

        if (!prompt_int_value(
                6,
                "Ajouter un autre exercice ? 1=oui 0=non",
                0,
                1,
                0,
                &another
            )) {
            another = 0;
        }
    }

    if (trainlog_time_now_rfc3339(
            ended_at,
            sizeof(ended_at)
        ) != TRAINLOG_STATUS_OK) {
        return;
    }

    (void)memset(&session, 0, sizeof(session));
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
    session.exercises = exercise_inputs;
    session.exercise_count = exercise_count;

    status = trainlog_database_insert_session(database, &session);

    erase();
    title("Fin de séance");
    if (status == TRAINLOG_STATUS_OK) {
        attron(trainlog_theme_attribute(TRAINLOG_COLOR_SUCCESS));
        mvprintw(4, 2, "✓ Séance enregistrée.");
        attroff(trainlog_theme_attribute(TRAINLOG_COLOR_SUCCESS));
        mvprintw(6, 2, "Début : %s", started_at);
        mvprintw(7, 2, "Fin   : %s", ended_at);
        mvprintw(
            8,
            2,
            "Type  : %s",
            session_type_label(session_type)
        );
        mvprintw(9, 2, "Exercices : %zu", exercise_count);
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

static void screen_session_detail(
    TrainlogDatabase *database,
    const char *session_id
)
{
    TrainlogSessionSummary session;
    TrainlogPersistedExerciseDetail exercises[MAX_SESSION_EXERCISES];
    size_t count = 0U;
    size_t selected = 0U;
    TrainlogStatus status;

    status = trainlog_database_get_session_details(
        database,
        session_id,
        &session,
        exercises,
        MAX_SESSION_EXERCISES,
        &count
    );

    if (status != TRAINLOG_STATUS_OK) {
        draw_shell("Détail séance", "Une touche pour revenir");
        status_line(
            "Impossible de charger la séance.",
            TRAINLOG_COLOR_ERROR
        );
        wait_key();
        return;
    }

    for (;;) {
        int key;

        draw_shell(
            "TRAINLOG — Détail séance",
            "←→ ou ↑↓ exercice précédent/suivant  b/Échap retour"
        );

        mvprintw(3, 4, "Début : %s", session.started_at);

        mvprintw(
            4,
            4,
            "Fin   : %s",
            session.ended_at[0] != '\0'
                ? session.ended_at
                : "séance ouverte"
        );

        mvprintw(
            5,
            4,
            "Type  : %s",
            session_type_label(
                session.session_type
            )
        );

        if (count == 0U) {
            mvprintw(7, 4, "Aucun exercice dans cette séance.");
        } else {
            TrainlogPersistedExerciseDetail *exercise =
                &exercises[selected];

            char rest_text[64];
            char target_duration_text[64];

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

            if (exercise->tracking_mode == TRAINLOG_TRACKING_DURATION) {
                (void)trainlog_duration_format(
                    exercise->target_duration_seconds,
                    target_duration_text,
                    sizeof(target_duration_text)
                );
            }

            attron(
                A_BOLD |
                trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT)
            );

            mvprintw(
                6,
                4,
                "Exercice %zu/%zu — %s",
                selected + 1U,
                count,
                exercise->name
            );

            attroff(
                A_BOLD |
                trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT)
            );

            mvprintw(
                8,
                4,
                "Mode : %-12s   Charge : %-10s   Repos : %s",
                exercise->tracking_mode == TRAINLOG_TRACKING_REPS
                    ? "répétitions"
                    : "durée",
                session_detail_load_label(exercise->load_mode),
                rest_text
            );

            if (exercise->tracking_mode == TRAINLOG_TRACKING_REPS) {
                mvprintw(
                    10,
                    4,
                    "Cible : %d série(s) × %d reps",
                    exercise->target_sets,
                    exercise->target_reps
                );
            } else {
                mvprintw(
                    10,
                    4,
                    "Cible : %d série(s) × %s",
                    exercise->target_sets,
                    target_duration_text
                );
            }

            if (exercise->has_target_weight != 0) {
                mvprintw(
                    11,
                    4,
                    "Charge cible : %.1f kg",
                    exercise->target_weight_kg
                );
            } else {
                mvprintw(11, 4, "Charge cible : —");
            }

            attron(
                A_BOLD |
                trainlog_theme_attribute(TRAINLOG_COLOR_SUCCESS)
            );

            mvprintw(
                13,
                4,
                "Réalisé : %zu série(s)",
                exercise->actual_set_count
            );

            attroff(
                A_BOLD |
                trainlog_theme_attribute(TRAINLOG_COLOR_SUCCESS)
            );

            mvprintw(
                15,
                4,
                "%.*s",
                COLS - 8,
                exercise->actual_summary
            );

            if (exercise->load_mode == TRAINLOG_LOAD_ASSISTANCE) {
                attron(
                    trainlog_theme_attribute(TRAINLOG_COLOR_WARNING)
                );
                mvprintw(
                    17,
                    4,
                    "Assistance : plus de kg = davantage d'aide."
                );
                attroff(
                    trainlog_theme_attribute(TRAINLOG_COLOR_WARNING)
                );
            }
        }

        refresh();
        key = getch();

        if (key == 'b' || key == 27) {
            return;
        }

        if (count > 0U &&
            (key == KEY_RIGHT || key == KEY_DOWN)) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : 0U;
        } else if (count > 0U &&
                   (key == KEY_LEFT || key == KEY_UP)) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : count - 1U;
        }
    }
}

static void screen_history(TrainlogDatabase *database)
{
    TrainlogSessionSummary sessions[MAX_SESSIONS];
    size_t selected = 0U;

    for (;;) {
        size_t count = 0U;
        size_t top = 0U;
        size_t index;
        int visible_rows = LINES - 8;
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

        draw_shell(
            "TRAINLOG — Historique",
            "↑↓ naviguer  Entrée détail  b/Échap retour"
        );

        if (count == 0U) {
            mvprintw(4, 4, "Aucune séance.");
            refresh();
            key = getch();

            if (key == 'b' || key == 27) {
                return;
            }

            continue;
        }

        if (selected >= count) {
            selected = count - 1U;
        }

        if (selected >= (size_t)visible_rows) {
            top =
                selected -
                (size_t)visible_rows +
                1U;
        }

        for (index = 0U;
             index < (size_t)visible_rows &&
             top + index < count;
             ++index) {
            size_t absolute = top + index;
            int item_row = 3 + (int)index;

            if (absolute == selected) {
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            mvprintw(
                item_row,
                4,
                " %-25s  %-14s  %2zu exercice(s) ",
                sessions[absolute].started_at,
                session_type_history_label(
                    sessions[absolute].session_type
                ),
                sessions[absolute].exercise_count
            );

            if (absolute == selected) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        refresh();
        key = getch();

        if (key == 'b' || key == 27) {
            return;
        }

        if (key == KEY_UP) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : count - 1U;
        } else if (key == KEY_DOWN) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : 0U;
        } else if (key == '\n' ||
                   key == KEY_ENTER) {
            screen_session_detail(
                database,
                sessions[selected].session_id
            );
        }
    }
}

static void screen_body(TrainlogDatabase *database)
{
    size_t selected = 0U;
    bool global_mode = false;

    const size_t metric_count =
        sizeof(BODY_METRICS) /
        sizeof(BODY_METRICS[0]);

    for (;;) {
        int key;

        if (global_mode) {
            draw_shell(
                "TRAINLOG — Corps — Vue globale",
                "g vue individuelle  b/Échap retour"
            );

            draw_global_body_overlay(database);

            refresh();
            key = getch();

            if (key == 'b' || key == 27) {
                return;
            }

            if (key == 'g' || key == 'G') {
                global_mode = false;
            }

            continue;
        }

        {
            const TrainlogBodyMetricView *view =
                &BODY_METRICS[selected];

            TrainlogBodyMetricPoint
                points[MAX_BODY_METRIC_POINTS];

            size_t count = 0U;
            size_t start;
            size_t index;

            (void)trainlog_database_list_body_metric_points(
                database,
                view->metric,
                points,
                MAX_BODY_METRIC_POINTS,
                &count
            );

            draw_shell(
                "TRAINLOG — Corps",
                "←→ métrique  g vue globale  a ajouter  b/Échap retour"
            );

            attron(
                A_BOLD |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );

            mvprintw(
                3,
                4,
                "%s  [%zu/%zu]",
                view->label,
                selected + 1U,
                metric_count
            );

            attroff(
                A_BOLD |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );

            if (count > 0U) {
                double first =
                    points[0].value;

                double latest =
                    points[count - 1U].value;

                double delta =
                    latest - first;

                mvprintw(
                    4,
                    4,
                    "Actuel : %.1f %s   Départ : %.1f %s   Évolution : %+.1f %s",
                    latest,
                    view->unit,
                    first,
                    view->unit,
                    delta,
                    view->unit
                );
            } else {
                mvprintw(
                    4,
                    4,
                    "Aucune valeur enregistrée."
                );
            }

            {
                TrainlogBodyMetric left;
                TrainlogBodyMetric right;
                const char *pair_label;

                if (body_metric_pair(
                        view->metric,
                        &left,
                        &right,
                        &pair_label
                    )) {
                    TrainlogBodyPairPoint pair;

                    if (trainlog_database_latest_body_pair(
                            database,
                            left,
                            right,
                            &pair
                        ) == TRAINLOG_STATUS_OK &&
                        pair.found) {
                        double difference =
                            pair.right_value -
                            pair.left_value;

                        mvprintw(
                            5,
                            4,
                            "%s : G %.1f cm  D %.1f cm  écart %.1f cm %s",
                            pair_label,
                            pair.left_value,
                            pair.right_value,
                            difference < 0.0
                                ? -difference
                                : difference,
                            difference > 0.0
                                ? "à droite"
                                : (
                                    difference < 0.0
                                        ? "à gauche"
                                        : "équilibré"
                                )
                        );
                    }
                }
            }

            draw_body_metric_graph(
                7,
                5,
                points,
                count,
                view->unit
            );

            mvprintw(
                13,
                4,
                "Dernières valeurs :"
            );

            start =
                count > 3U
                    ? count - 3U
                    : 0U;

            for (index = start;
                 index < count;
                 ++index) {
                mvprintw(
                    14 + (int)(index - start),
                    6,
                    "%-25s %.1f %s",
                    points[index].observed_at,
                    points[index].value,
                    view->unit
                );
            }
        }

        refresh();
        key = getch();

        if (key == 'b' || key == 27) {
            return;
        }

        if (key == KEY_RIGHT) {
            selected =
                selected + 1U < metric_count
                    ? selected + 1U
                    : 0U;
        } else if (key == KEY_LEFT) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : metric_count - 1U;
        } else if (key == 'a') {
            add_body_observation(database);
        } else if (key == 'g' || key == 'G') {
            global_mode = true;
        }
    }
}

int trainlog_tui_run(TrainlogDatabase *database)
{
    if (database == NULL) {
        return 1;
    }

    (void)setlocale(LC_ALL, "");

    if (initscr() == NULL) {
        return 1;
    }

    cbreak();
    noecho();
    keypad(stdscr, true);
    curs_set(0);
    trainlog_theme_initialize();

    for (;;) {
        DashboardAction action;

        if (LINES < 20 || COLS < 72) {
            int key;

            erase();
            mvprintw(
                1,
                2,
                "Terminal trop petit — minimum 72x20."
            );
            mvprintw(3, 2, "q pour quitter");
            refresh();

            key = getch();
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
        case DASHBOARD_BODY:
            screen_body(database);
            break;
        case DASHBOARD_QUIT:
            endwin();
            return 0;
        default:
            break;
        }
    }

    endwin();
    return 0;
}
