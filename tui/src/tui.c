/**
 * @file tui.c
 * @brief First usable Trainlog ncurses interface.
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
#include <sys/wait.h>
#include <wchar.h>
#include <unistd.h>

#include <curses.h>

#include "trainlog/bodyviz.h"
#include "trainlog/catalog.h"
#include "trainlog/duration.h"
#include "trainlog/id.h"
#include "trainlog/mtp.h"
#include "trainlog/theme.h"
#include "trainlog/timeutil.h"
#include "trainlog/usb.h"

#define MAX_EXERCISES 128U
#define MAX_SESSION_EXERCISES 32U
#define MAX_SETS_PER_EXERCISE 64U
#define MAX_SESSIONS 128U
#define MAX_WEIGHT_POINTS 256U

#define MAX_BODY_METRIC_POINTS 256U


/* TRAINLOG_TUI_V02_POLISH */
/* TRAINLOG_TUI_PROFILED_EXERCISE_CREATION */
/* TRAINLOG_SYNC_RESPONSIVE_CACHE */
/* TRAINLOG_SYNC_LARGE_LAYOUT_S_FIX */
/* TRAINLOG_SYNC_HISTORY_BIDIRECTIONAL_V1 */
/* TRAINLOG_SYNC_PC_TO_ANDROID_DIAGNOSTICS */
/* TRAINLOG_SYNC_FULL_MTP_SILENCE */

typedef enum DashboardAction {
    DASHBOARD_NEW_SESSION = 0,
    DASHBOARD_HISTORY,
    DASHBOARD_EXERCISES,
    DASHBOARD_BODY,
    DASHBOARD_SYNC,
    DASHBOARD_QUIT
} DashboardAction;

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

    mvprintw(
        row,
        2,
        "%s",
        label
    );

    getyx(
        stdscr,
        cursor_row,
        input_column
    );

    (void)cursor_row;

    noecho();
    (void)curs_set(1);

    for (;;) {
        wint_t value;
        int rc;

        move(
            row,
            input_column
        );

        clrtoeol();

        if (used > 0U) {
            addnstr(
                output,
                (int)used
            );
        }

        refresh();

        rc = get_wch(&value);

        if (rc == ERR) {
            break;
        }

        if (rc == KEY_CODE_YES) {
            int key =
                (int)value;

            if (key == KEY_ENTER) {
                if (allow_empty ||
                    used > 0U) {
                    accepted = true;
                    break;
                }

                continue;
            }

            if (key == KEY_BACKSPACE ||
                key == KEY_DC) {
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

        if (value == (wint_t)27) {
            accepted = false;
            break;
        }

        if (value == (wint_t)'\n' ||
            value == (wint_t)'\r') {
            if (allow_empty ||
                used > 0U) {
                accepted = true;
                break;
            }

            continue;
        }

        if (value == (wint_t)8 ||
            value == (wint_t)127) {
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

        if (value >= (wint_t)32) {
            char encoded[MB_LEN_MAX];
            mbstate_t state;
            size_t encoded_size;

            (void)memset(
                &state,
                0,
                sizeof(state)
            );

            encoded_size =
                wcrtomb(
                    encoded,
                    (wchar_t)value,
                    &state
                );

            if (encoded_size ==
                    (size_t)-1 ||
                encoded_size == 0U ||
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

    noecho();
    (void)curs_set(0);

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

    bool decorated =
        COLS >= 100 &&
        LINES >= 36;

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

    if (decorated) {
        erase();
        box(stdscr, 0, 0);

        section_ascii_header(
            ":: P E R F O R M A N C E   E X E R C I C E ::"
        );

        dashboard_panel(
            summary_top,
            2,
            summary_bottom,
            COLS - 3,
            "PERFORMANCE"
        );

        dashboard_panel(
            graph_top,
            2,
            graph_bottom,
            COLS - 3,
            "EVOLUTION"
        );

        if (history_bottom >
            history_top + 2) {
            focused_panel(
                history_top,
                2,
                history_bottom,
                COLS - 3,
                "HISTORIQUE",
                true
            );
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
            "b/Échap retour"
        );

        attroff(
            trainlog_theme_attribute(
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
        summary_top + 1,
        5,
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
        summary_top + 2,
        5,
        "Séances enregistrées : %zu",
        count
    );

    if (latest == NULL) {
        mvprintw(
            summary_top + 3,
            5,
            "Aucune série réussie enregistrée."
        );
    } else {
        mvprintw(
            summary_top + 3,
            5,
            "Mode suivi : %s",
            exercise_load_mode_label(
                graph_mode
            )
        );

        mvprintw(
            summary_top + 4,
            5,
            "Dernier meilleur set : %.*s",
            COLS - 30,
            latest_text
        );

        mvprintw(
            summary_top + 5,
            5,
            "Meilleur set enregistré : %.*s",
            COLS - 33,
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
            graph_height
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
    }

    refresh();

    for (;;) {
        int key = getch();

        if (key == 'b' ||
            key == 'B' ||
            key == 27 ||
            key == '\n' ||
            key == KEY_ENTER) {
            return;
        }
    }
}

/* TRAINLOG_SECTION_ASCII_HEADER */

static void section_ascii_header(
    const char *subtitle
)
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

    if (subtitle != NULL &&
        subtitle[0] != '\0') {
        int width =
            (int)strlen(subtitle);

        int column =
            (COLS - width) / 2;

        if (column < 2) {
            column = 2;
        }

        attron(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        mvprintw(
            6,
            column,
            "%s",
            subtitle
        );

        attroff(
            trainlog_theme_attribute(
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

    attron(
        trainlog_theme_attribute(
            TRAINLOG_COLOR_MUTED
        )
    );

    for (row = top;
         row <= bottom;
         ++row) {
        mvaddch(
            row,
            column,
            ACS_VLINE
        );
    }

    attroff(
        trainlog_theme_attribute(
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

    attron(
        A_BOLD |
        trainlog_theme_attribute(
            TRAINLOG_COLOR_ACCENT
        )
    );

    mvaddch(
        thumb,
        column,
        ACS_CKBOARD
    );

    attroff(
        A_BOLD |
        trainlog_theme_attribute(
            TRAINLOG_COLOR_ACCENT
        )
    );
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
            COLS >= 100 &&
            LINES >= 30;

        bool framed =
            COLS >= 90 &&
            LINES >= 24;

        int list_top =
            large_layout
                ? 11
                : 3;

        int list_bottom =
            LINES - 4;

        int first_row =
            list_top + 1;

        int visible_rows =
            framed
                ? list_bottom -
                    first_row
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
            erase();
            box(stdscr, 0, 0);

            section_ascii_header(
                ":: E X E R C I C E S ::"
            );

            primary_top_navbar(
                3,
                nav_selected,
                focus == 0
            );

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
                "Tab zone  ↑↓/PgUp/PgDn catalogue  ←→ menu  Entrée ouvrir  a ajouter  0/Home accueil  F1-F4 direct  b/Échap retour"
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Exercices",
                "↑↓ naviguer  Entrée performance  a ajouter  b/Échap retour"
            );
        }

        if (framed) {
            if (large_layout) {
                focused_panel(
                    list_top,
                    2,
                    list_bottom,
                    COLS - 3,
                    "CATALOGUE",
                    focus == 1
                );
            } else {
                exercise_panel(
                    list_top,
                    2,
                    list_bottom,
                    COLS - 3,
                    "CATALOGUE"
                );
            }
        }

        if (count == 0U) {
            mvprintw(
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

            if (focus == 1 &&
                absolute == selected) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        if (large_layout) {
            section_scrollbar(
                first_row,
                list_bottom - 1,
                COLS - 5,
                selected,
                count,
                (size_t)visible_rows
            );
        }

        refresh();
        key = getch();

        if (large_layout &&
            (key == '\t' ||
             key == KEY_BTAB)) {
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
            if (key == KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : 5;
            } else if (key == KEY_RIGHT) {
                nav_selected =
                    nav_selected < 5
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == KEY_ENTER
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
            key == KEY_UP) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : 0U;

            continue;
        }

        if (count > 0U &&
            key == KEY_DOWN) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : count - 1U;

            continue;
        }

        if (count > 0U &&
            key == KEY_PPAGE) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected > jump
                    ? selected - jump
                    : 0U;

            continue;
        }

        if (count > 0U &&
            key == KEY_NPAGE) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected + jump < count
                    ? selected + jump
                    : count - 1U;

            continue;
        }

        if (count > 0U &&
            (key == '\n' ||
             key == KEY_ENTER)) {
            screen_exercise_performance(
                database,
                &exercises[selected]
            );

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
            COLS >= 100 &&
            LINES >= 30;
        int list_top =
            large_layout ? 8 : 3;
        int list_bottom =
            LINES - 4;
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

            mvprintw(
                4,
                4,
                "Aucun exercice disponible."
            );

            refresh();
            key = getch();

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
            erase();
            box(stdscr, 0, 0);

            section_ascii_header(
                ":: C H O I S I R   E X E R C I C E ::"
            );

            focused_panel(
                list_top,
                2,
                list_bottom,
                COLS - 3,
                "CATALOGUE",
                true
            );

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
                "↑↓ choisir  Entrée sélectionner  a créer un exercice  Échap annuler"
            );

            attroff(
                trainlog_theme_attribute(
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
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            mvprintw(
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
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        if (large_layout) {
            section_scrollbar(
                first_row,
                list_bottom - 1,
                COLS - 5,
                selected,
                count,
                (size_t)visible_rows
            );
        }

        refresh();
        key = getch();

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
        } else if (
            key == '\n' ||
            key == KEY_ENTER
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

            if (x > COLS - 8) {
                x = COLS - 8;
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
    WINDOW *panel;
    int height;
    int width;
    chtype border_attribute;

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

    border_attribute =
        active
            ? A_BOLD |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_WARNING
                )
            : trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            );

    wattron(
        panel,
        border_attribute
    );

    box(panel, 0, 0);

    if (label != NULL &&
        label[0] != '\0' &&
        width > 8) {
        mvwprintw(
            panel,
            0,
            2,
            " %.*s ",
            width - 6,
            label
        );
    }

    wattroff(
        panel,
        border_attribute
    );

    syncok(panel, TRUE);
    wsyncup(panel);
    delwin(panel);
}

static void primary_top_navbar(
    int active_page,
    int selected_page,
    bool focused
)
{
    static const char *const labels[] = {
        "0 Accueil",
        "1 Séance",
        "2 Historique",
        "3 Exercices",
        "4 Corps",
        "5 Sync"
    };

    int column = 5;
    int index;

    focused_panel(
        8,
        2,
        10,
        COLS - 3,
        "NAVIGATION",
        focused
    );

    for (index = 0;
         index < 6;
         ++index) {
        int width =
            (int)strlen(labels[index]) + 4;

        if (index == selected_page) {
            attron(
                A_REVERSE |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        } else if (index == active_page) {
            attron(
                A_BOLD |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        }

        mvprintw(
            9,
            column,
            " %s ",
            labels[index]
        );

        if (index == selected_page) {
            attroff(
                A_REVERSE |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );
        } else if (index == active_page) {
            attroff(
                A_BOLD |
                trainlog_theme_attribute(
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
        selected_page > 5) {
        return false;
    }

    if (selected_page == 0) {
        return true;
    }

    return ungetch(
        '0' + selected_page
    ) != ERR;
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
    case KEY_HOME:
        return true;

    case KEY_F(1):
    case '1':
        forwarded = '1';
        break;

    case KEY_F(2):
    case '2':
        forwarded = '2';
        break;

    case KEY_F(3):
    case '3':
        forwarded = '3';
        break;

    case KEY_F(4):
    case '4':
        forwarded = '4';
        break;

    case KEY_F(5):
    case '5':
        forwarded = '5';
        break;

    default:
        return false;
    }

    return ungetch(forwarded) != ERR;
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
        "0 Accueil",
        "1 Séance",
        "2 Historique",
        "3 Exercices",
        "4 Corps",
        "5 Sync"
    };

    static const char *const footer =
        "←→ naviguer  Entrée ouvrir  0 accueil  F1-F4 accès direct  q quitter";

    int selected = 0;

    for (;;) {
        bool large_layout =
            COLS >= 100 &&
            LINES >= 30;

        int nav_top =
            large_layout ? 8 : LINES - 4;

        int nav_bottom =
            large_layout ? 10 : LINES - 3;

        int key;
        int index;
        int column;

        erase();
        box(stdscr, 0, 0);

        if (large_layout) {
            dashboard_ascii_header();

            focused_panel(
                nav_top,
                2,
                nav_bottom,
                COLS - 3,
                "NAVIGATION",
                true
            );
        }

        draw_dashboard_body_graph(database);

        column =
            large_layout ? 5 : 3;

        for (index = 0;
             index < 6;
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

            column += width + 2;
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
                    : 5;
            break;

        case KEY_DOWN:
        case KEY_RIGHT:
            selected =
                selected < 5
                    ? selected + 1
                    : 0;
            break;

        case '\n':
        case KEY_ENTER:
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
                return DASHBOARD_BODY;
            case 5:
                return DASHBOARD_SYNC;
            default:
                break;
            }
            break;

        case '0':
        case KEY_HOME:
            selected = 0;
            break;

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

        case KEY_F(5):
        case '5':
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
        (size_t)actual_sets;

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

        draw_shell(
            exercise.name,
            "Échap annuler · Durées : 90, 90s, 1:30, 1m30, 2m"
        );

        mvprintw(
            3,
            4,
            "Série %zu / %zu",
            set_index + 1U,
            output->set_count
        );

        if (exercise.tracking_mode ==
            TRAINLOG_TRACKING_REPS) {
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
            COLS >= 100 &&
            LINES >= 30;

        int key;

        if (large_layout) {
            erase();
            box(stdscr, 0, 0);

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
                COLS - 5,
                "TYPE DE SEANCE",
                focus == 1
            );

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
                "Tab zone  ←→ menu  ↑↓ type  Entrée valider  0/Home accueil  F2-F4 direct  Échap annuler"
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );

            if (focus == 1 &&
                selected == 0) {
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            mvprintw(
                13,
                7,
                " Entraînement "
            );

            if (focus == 1 &&
                selected == 0) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            attron(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );

            mvprintw(
                14,
                9,
                "Séance normale : progression, volume, travail courant."
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );

            if (focus == 1 &&
                selected == 1) {
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            mvprintw(
                17,
                7,
                " Test de max "
            );

            if (focus == 1 &&
                selected == 1) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            attron(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );

            mvprintw(
                18,
                9,
                "Séance explicitement dédiée aux mesures de max."
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            WINDOW *panel;
            int panel_width =
                COLS - 8;

            draw_shell(
                "TRAINLOG — Nouvelle séance",
                "↑↓ choisir  Entrée valider  Échap annuler"
            );

            if (panel_width < 40) {
                panel_width = 40;
            }

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

            mvwprintw(
                panel,
                3,
                5,
                "Séance normale : progression, volume, travail courant."
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

            mvwprintw(
                panel,
                6,
                5,
                "Séance explicitement dédiée aux mesures de max."
            );

            syncok(panel, TRUE);
            wsyncup(panel);
            delwin(panel);
        }

        refresh();
        key = getch();

        if (large_layout &&
            (key == '\t' ||
             key == KEY_BTAB)) {
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
             key == KEY_HOME)) {
            return false;
        }

        if (large_layout &&
            (key == '2' ||
             key == KEY_F(2) ||
             key == '3' ||
             key == KEY_F(3) ||
             key == '4' ||
             key == KEY_F(4) ||
             key == '5' ||
             key == KEY_F(5))) {
            if (primary_top_nav_forward(key)) {
                return false;
            }

            continue;
        }

        if (large_layout &&
            focus == 0) {
            if (key == KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : 5;
            } else if (key == KEY_RIGHT) {
                nav_selected =
                    nav_selected < 5
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == KEY_ENTER
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

        if (key == KEY_UP ||
            key == KEY_LEFT ||
            key == KEY_DOWN ||
            key == KEY_RIGHT) {
            selected =
                selected == 0
                    ? 1
                    : 0;
            continue;
        }

        if (key == '\n' ||
            key == KEY_ENTER) {
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

    if (draft->input.load_mode ==
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

    mvprintw(
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
            COLS >= 100 &&
            LINES >= 30;

        int frame_top =
            large_layout ? 8 : 3;

        int frame_bottom =
            LINES - 4;

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
            erase();
            box(stdscr, 0, 0);

            section_ascii_header(
                ":: S E A N C E   E N   C O U R S ::"
            );

            focused_panel(
                frame_top,
                2,
                frame_bottom,
                COLS - 3,
                "RESUME AVANT ENREGISTREMENT",
                true
            );

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
                "↑↓ choisir  e/Entrée modifier  a ajouter  d supprimer  f enregistrer  q/Échap abandonner"
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Séance en cours",
                "↑↓ choisir  e/Entrée modifier  a ajouter  d supprimer  f enregistrer  q/Échap abandonner"
            );
        }

        attron(
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );

        mvprintw(
            frame_top + 1,
            large_layout ? 5 : 4,
            "Type : %s",
            session_type_label(
                session_type
            )
        );

        attroff(
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );

        if (*count == 0U) {
            mvprintw(
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
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            mvprintw(
                row,
                large_layout ? 5 : 4,
                " %2zu  %-36.36s  %-28.28s ",
                absolute + 1U,
                drafts[absolute].name,
                summary
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

                refresh();
                (void)getch();
                continue;
            }

            return true;
        }

        if (key == KEY_UP &&
            *count > 0U) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : *count - 1U;
            continue;
        }

        if (key == KEY_DOWN &&
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

                refresh();
                (void)getch();
                continue;
            }

            if (draft_build_exercise(
                    database,
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
             key == KEY_ENTER) &&
            *count > 0U) {
            TrainlogSessionDraftExercise replacement;

            if (draft_build_exercise(
                    database,
                    &replacement,
                    drafts[selected].notes
                )) {
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
        attron(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_SUCCESS
            )
        );

        mvprintw(
            4,
            2,
            "✓ Séance enregistrée."
        );

        attroff(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_SUCCESS
            )
        );

        mvprintw(
            6,
            2,
            "Début : %s",
            started_at
        );

        mvprintw(
            7,
            2,
            "Fin   : %s",
            ended_at
        );

        mvprintw(
            8,
            2,
            "Type  : %s",
            session_type_label(
                session_type
            )
        );

        mvprintw(
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
            COLS >= 100 &&
            LINES >= 32;

        int key;

        if (decorated) {
            erase();
            box(stdscr, 0, 0);

            section_ascii_header(
                ":: D E T A I L   S E A N C E ::"
            );

            dashboard_panel(
                8,
                2,
                13,
                COLS - 3,
                "SEANCE"
            );

            focused_panel(
                14,
                2,
                LINES - 4,
                COLS - 3,
                "EXERCICE",
                true
            );

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
                "←→/↑↓ exercice précédent/suivant  e modifier la séance  b/Échap retour"
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );

            mvprintw(
                10,
                5,
                "Début : %s",
                session.started_at
            );

            mvprintw(
                11,
                5,
                "Fin   : %s",
                session.ended_at[0] != '\0'
                    ? session.ended_at
                    : "séance ouverte"
            );

            mvprintw(
                12,
                5,
                "Type  : %s",
                session_type_label(
                    session.session_type
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Détail séance",
                "←→/↑↓ naviguer  e modifier  b/Échap retour"
            );

            mvprintw(
                3,
                4,
                "Début : %s",
                session.started_at
            );

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
        }

        if (count == 0U) {
            mvprintw(
                decorated ? 17 : 7,
                decorated ? 5 : 4,
                "Aucun exercice dans cette séance."
            );
        } else {
            TrainlogPersistedExerciseDetail *exercise =
                &exercises[selected];

            if (exercise->recording_mode ==
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

                attron(
                    A_BOLD |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );

                mvprintw(
                    title_row,
                    decorated ? 5 : 4,
                    "Exercice %zu/%zu — %s",
                    selected + 1U,
                    count,
                    exercise->name
                );

                attroff(
                    A_BOLD |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );

                mvprintw(
                    mode_row,
                    decorated ? 5 : 4,
                    "Mode : continu"
                );

                mvprintw(
                    row++,
                    decorated ? 5 : 4,
                    "Durée : %s",
                    duration_text
                );

                if (exercise->has_continuous_speed != 0) {
                    mvprintw(
                        row++,
                        decorated ? 5 : 4,
                        "Vitesse : %.1f km/h",
                        exercise->continuous_speed_kmh
                    );
                }

                if (exercise->has_continuous_distance != 0) {
                    mvprintw(
                        row++,
                        decorated ? 5 : 4,
                        "Distance : %.2f km",
                        exercise->continuous_distance_km
                    );
                }

                attron(
                    A_BOLD |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_SUCCESS
                    )
                );

                mvprintw(
                    row + 1,
                    decorated ? 5 : 4,
                    "Réalisé : activité continue"
                );

                attroff(
                    A_BOLD |
                    trainlog_theme_attribute(
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

                attron(
                    A_BOLD |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );

                mvprintw(
                    title_row,
                    decorated ? 5 : 4,
                    "Exercice %zu/%zu — %s",
                    selected + 1U,
                    count,
                    exercise->name
                );

                attroff(
                    A_BOLD |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );

                mvprintw(
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

                if (exercise->tracking_mode ==
                    TRAINLOG_TRACKING_REPS) {
                    mvprintw(
                        target_row,
                        decorated ? 5 : 4,
                        "Cible : %d série(s) × %d reps",
                        exercise->target_sets,
                        exercise->target_reps
                    );
                } else {
                    mvprintw(
                        target_row,
                        decorated ? 5 : 4,
                        "Cible : %d série(s) × %s",
                        exercise->target_sets,
                        target_duration_text
                    );
                }

                if (exercise->has_target_weight != 0) {
                    mvprintw(
                        weight_row,
                        decorated ? 5 : 4,
                        "Charge cible : %.1f kg",
                        exercise->target_weight_kg
                    );
                } else {
                    mvprintw(
                        weight_row,
                        decorated ? 5 : 4,
                        "Charge cible : —"
                    );
                }

                attron(
                    A_BOLD |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_SUCCESS
                    )
                );

                mvprintw(
                    actual_row,
                    decorated ? 5 : 4,
                    "Réalisé : %zu série(s)",
                    exercise->actual_set_count
                );

                attroff(
                    A_BOLD |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_SUCCESS
                    )
                );

                mvprintw(
                    summary_row,
                    decorated ? 5 : 4,
                    "%.*s",
                    COLS - 10,
                    exercise->actual_summary
                );

                if (exercise->load_mode ==
                    TRAINLOG_LOAD_ASSISTANCE) {
                    attron(
                        trainlog_theme_attribute(
                            TRAINLOG_COLOR_WARNING
                        )
                    );

                    mvprintw(
                        warning_row,
                        decorated ? 5 : 4,
                        "Assistance : plus de kg = davantage d'aide."
                    );

                    attroff(
                        trainlog_theme_attribute(
                            TRAINLOG_COLOR_WARNING
                        )
                    );
                }
            }
        }

        refresh();
        key = getch();

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

        if (count > 0U &&
            (key == KEY_RIGHT ||
             key == KEY_DOWN)) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : 0U;
        } else if (
            count > 0U &&
            (key == KEY_LEFT ||
             key == KEY_UP)
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
            ":: H I S T O R I Q U E ::";

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

    attron(
        trainlog_theme_attribute(
            TRAINLOG_COLOR_MUTED
        )
    );

    for (row = top;
         row <= bottom;
         ++row) {
        mvaddch(
            row,
            column,
            ACS_VLINE
        );
    }

    attroff(
        trainlog_theme_attribute(
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

    attron(
        A_BOLD |
        trainlog_theme_attribute(
            TRAINLOG_COLOR_ACCENT
        )
    );

    mvaddch(
        thumb,
        column,
        ACS_CKBOARD
    );

    attroff(
        A_BOLD |
        trainlog_theme_attribute(
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
            COLS >= 100 &&
            LINES >= 30;

        int list_top =
            large_layout
                ? 11
                : 3;

        int list_bottom =
            LINES - 4;

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

        erase();
        box(stdscr, 0, 0);

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
                COLS - 3,
                "SEANCES ENREGISTREES",
                focus == 1
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
                2,
                " TRAINLOG — Historique "
            );

            attroff(
                A_BOLD |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );
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
            large_layout
                ? "Tab zone  ↑↓/PgUp/PgDn liste  ←→ menu  Entrée ouvrir  e modifier  0/Home accueil  F1-F4 direct  b/Échap retour"
                : "↑↓ naviguer  Entrée détail  e modifier  b/Échap retour"
        );

        attroff(
            trainlog_theme_attribute(
                TRAINLOG_COLOR_MUTED
            )
        );

        if (count == 0U) {
            mvprintw(
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
                    attron(
                        A_REVERSE |
                        trainlog_theme_attribute(
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

                    mvprintw(
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
                    attroff(
                        A_REVERSE |
                        trainlog_theme_attribute(
                            TRAINLOG_COLOR_ACCENT
                        )
                    );
                }
            }

            if (large_layout) {
                history_scrollbar(
                    first_row,
                    list_bottom - 1,
                    COLS - 5,
                    selected,
                    count,
                    (size_t)visible_rows
                );
            }
        }

        refresh();
        key = getch();

        if (large_layout &&
            (key == '\t' ||
             key == KEY_BTAB)) {
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
            if (key == KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : 5;
            } else if (key == KEY_RIGHT) {
                nav_selected =
                    nav_selected < 5
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == KEY_ENTER
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

        if (key == KEY_UP) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : 0U;

            continue;
        }

        if (key == KEY_DOWN) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : count - 1U;

            continue;
        }

        if (key == KEY_PPAGE) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected > jump
                    ? selected - jump
                    : 0U;

            continue;
        }

        if (key == KEY_NPAGE) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected + jump < count
                    ? selected + jump
                    : count - 1U;

            continue;
        }

        if (key == '\n' ||
            key == KEY_ENTER) {
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

    attron(
        trainlog_theme_attribute(
            TRAINLOG_COLOR_MUTED
        )
    );

    for (row = top;
         row <= bottom;
         ++row) {
        mvaddch(
            row,
            column,
            ACS_VLINE
        );
    }

    attroff(
        trainlog_theme_attribute(
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

    attron(
        A_BOLD |
        trainlog_theme_attribute(
            TRAINLOG_COLOR_ACCENT
        )
    );

    mvaddch(
        thumb,
        column,
        ACS_CKBOARD
    );

    attroff(
        A_BOLD |
        trainlog_theme_attribute(
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
        mvprintw(
            row,
            column,
            "%-24s %7.1f %s",
            label,
            value,
            unit
        );
    } else {
        mvprintw(
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
            COLS >= 100 &&
            LINES >= 34;
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
            erase();
            box(stdscr, 0, 0);

            section_ascii_header(
                ":: D E T A I L   C O R P S ::"
            );

            focused_panel(
                frame_top,
                2,
                LINES - 4,
                COLS - 3,
                page == 0
                    ? "RELEVE — GENERAL"
                    : "RELEVE — MEMBRES",
                true
            );

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
                "←→ page  e Modifier  b/Échap retour"
            );

            attroff(
                trainlog_theme_attribute(
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
                LINES - 4,
                COLS - 3,
                page == 0
                    ? "RELEVE — GENERAL"
                    : "RELEVE — MEMBRES"
            );
        }

        attron(
            A_BOLD |
            trainlog_theme_attribute(
                TRAINLOG_COLOR_ACCENT
            )
        );

        mvprintw(
            frame_top + 2,
            5,
            "%s   page %d/2",
            date,
            page + 1
        );

        attroff(
            A_BOLD |
            trainlog_theme_attribute(
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

        refresh();
        key = getch();

        if (key == 'b' ||
            key == 'B' ||
            key == 27) {
            return;
        }

        if (key == KEY_LEFT ||
            key == KEY_RIGHT) {
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

static void screen_body(
    TrainlogDatabase *database
)
{
    TrainlogBodyObservationRecord
        records[MAX_BODY_OBSERVATIONS];

    size_t selected = 0U;
    int nav_selected = 4;
    int focus = 1;

    for (;;) {
        TrainlogBodyMetricPoint
            weight_points[MAX_BODY_METRIC_POINTS];

        size_t count = 0U;
        size_t weight_count = 0U;
        size_t top = 0U;
        size_t index;

        bool large_layout =
            COLS >= 100 &&
            LINES >= 30;

        int graph_top =
            large_layout ? 11 : 3;

        int graph_bottom =
            large_layout ? 19 : 11;

        int list_top =
            graph_bottom + 1;

        int list_bottom =
            LINES - 4;

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
            erase();
            box(stdscr, 0, 0);

            section_ascii_header(
                ":: C O R P S ::"
            );

            primary_top_navbar(
                4,
                nav_selected,
                focus == 0
            );

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
                "Tab zone  ↑↓/PgUp/PgDn relevés  ←→ menu  Entrée détail  e Modifier  a ajouter  g vue globale  0/Home accueil  F1-F4 direct"
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Corps",
                "↑↓ choisir  Entrée détail  e Modifier  a ajouter  g vue globale  b/Échap retour"
            );
        }

        body_panel(
            graph_top,
            2,
            graph_bottom,
            COLS - 3,
            "EVOLUTION DU POIDS"
        );

        if (weight_count == 0U) {
            mvprintw(
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
                COLS - 3,
                "RELEVES ENREGISTRES",
                focus == 1
            );
        } else {
            body_panel(
                list_top,
                2,
                list_bottom,
                COLS - 3,
                "RELEVES ENREGISTRES"
            );
        }

        if (count == 0U) {
            mvprintw(
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
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }

            mvprintw(
                row,
                5,
                " %-8s  %.*s ",
                date,
                COLS - 20,
                summary
            );

            if (focus == 1 &&
                absolute == selected) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_ACCENT
                    )
                );
            }
        }

        body_draw_scrollbar(
            first_row,
            list_bottom - 1,
            COLS - 5,
            selected,
            count,
            (size_t)visible_rows
        );

        refresh();
        key = getch();

        if (large_layout &&
            (key == '\t' ||
             key == KEY_BTAB)) {
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
            if (key == KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : 5;
            } else if (key == KEY_RIGHT) {
                nav_selected =
                    nav_selected < 5
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == KEY_ENTER
            ) {
                if (primary_top_nav_activate(
                        nav_selected
                    )) {
                    return;
                }
            }

            continue;
        }

        if (key == KEY_UP &&
            count > 0U) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : 0U;

            continue;
        }

        if (key == KEY_DOWN &&
            count > 0U) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : count - 1U;

            continue;
        }

        if (key == KEY_PPAGE &&
            count > 0U) {
            size_t jump =
                (size_t)visible_rows;

            selected =
                selected > jump
                    ? selected - jump
                    : 0U;

            continue;
        }

        if (key == KEY_NPAGE &&
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
             key == KEY_ENTER) &&
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

        if (key == 'g' ||
            key == 'G') {
            draw_shell(
                "TRAINLOG — Corps — Vue globale",
                "Une touche pour revenir"
            );

            draw_global_body_overlay(
                database
            );

            refresh();
            (void)getch();
        }
    }
}

/* TRAINLOG_SYNC_TUI */

#define SYNC_DEVICE_CAPACITY 8U
#define SYNC_STORAGE_CAPACITY 8U
#define SYNC_ENTRY_CAPACITY 128U

typedef struct TrainlogSyncOverview {
    bool connected;
    bool storage_ready;
    bool exchange_ready;
    size_t device_count;
    size_t remote_entry_count;
    size_t remote_json_count;
    size_t local_exercise_count;
    TrainlogUsbDevice device;
    TrainlogMtpStorage storage;
    uint32_t exchange_folder_id;
} TrainlogSyncOverview;

static bool sync_name_has_json_suffix(
    const char *name
)
{
    size_t length;

    if (name == NULL) {
        return false;
    }

    length = strlen(name);

    return
        length >= 5U &&
        strcmp(
            name + length - 5U,
            ".json"
        ) == 0;
}

static double sync_bytes_to_gib(
    uint64_t bytes
)
{
    return
        (double)bytes /
        (1024.0 * 1024.0 * 1024.0);
}

typedef struct TrainlogMobileImportReport {
    size_t exercises_imported;
    size_t exercises_reconciled;
    size_t exercises_skipped;
    size_t sessions_imported;
    size_t sessions_skipped;
    size_t body_imported;
    size_t body_skipped;
    size_t catalog_published;
} TrainlogMobileImportReport;

static TrainlogStatus sync_find_mtp_child(
    const TrainlogUsbDevice *device,
    uint32_t storage_id,
    uint32_t parent_id,
    const char *name,
    bool folder,
    uint32_t *output_id,
    uint64_t *output_size
)
{
    TrainlogMtpEntry entries[SYNC_ENTRY_CAPACITY];
    size_t count = 0U;
    size_t index;
    TrainlogStatus status;

    if (device == NULL ||
        name == NULL ||
        output_id == NULL ||
        output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_id = 0U;
    *output_size = 0U;

    status =
        trainlog_mtp_list_folder(
            device->bus_number,
            device->device_number,
            storage_id,
            parent_id,
            entries,
            SYNC_ENTRY_CAPACITY,
            &count
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    for (index = 0U;
         index < count;
         ++index) {
        if (entries[index].folder == folder &&
            strcmp(
                entries[index].name,
                name
            ) == 0) {
            *output_id =
                entries[index].item_id;

            *output_size =
                entries[index].size_bytes;

            return TRAINLOG_STATUS_OK;
        }
    }

    return TRAINLOG_STATUS_NOT_FOUND;
}

static TrainlogStatus sync_find_mobile_export(
    const TrainlogSyncOverview *overview,
    uint32_t *output_item_id,
    uint64_t *output_size
)
{
    uint32_t download_id = 0U;
    uint32_t trainlog_id = 0U;
    uint64_t ignored_size = 0U;
    TrainlogStatus status;

    if (overview == NULL ||
        output_item_id == NULL ||
        output_size == NULL ||
        !overview->connected ||
        !overview->storage_ready) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status =
        sync_find_mtp_child(
            &overview->device,
            overview->storage.storage_id,
            UINT32_MAX,
            "Download",
            true,
            &download_id,
            &ignored_size
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    status =
        sync_find_mtp_child(
            &overview->device,
            overview->storage.storage_id,
            download_id,
            "Trainlog",
            true,
            &trainlog_id,
            &ignored_size
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    return
        sync_find_mtp_child(
            &overview->device,
            overview->storage.storage_id,
            trainlog_id,
            "trainlog-mobile-export-v1.json",
            false,
            output_item_id,
            output_size
        );
}

static TrainlogStatus sync_silenced_mobile_export_download(
    const TrainlogSyncOverview *overview,
    const char *local_path,
    uint64_t *output_size
)
{
    int saved_stdout = -1;
    int saved_stderr = -1;
    int null_fd = -1;
    uint32_t item_id = 0U;
    uint64_t size_bytes = 0U;
    TrainlogStatus status;

    if (overview == NULL ||
        local_path == NULL ||
        local_path[0] == '\0' ||
        output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    saved_stdout =
        dup(STDOUT_FILENO);

    saved_stderr =
        dup(STDERR_FILENO);

    null_fd =
        open(
            "/dev/null",
            O_WRONLY
        );

    if (saved_stdout < 0 ||
        saved_stderr < 0 ||
        null_fd < 0) {
        if (saved_stdout >= 0) {
            (void)close(saved_stdout);
        }

        if (saved_stderr >= 0) {
            (void)close(saved_stderr);
        }

        if (null_fd >= 0) {
            (void)close(null_fd);
        }

        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    (void)fflush(stdout);
    (void)fflush(stderr);

    if (dup2(
            null_fd,
            STDOUT_FILENO
        ) < 0 ||
        dup2(
            null_fd,
            STDERR_FILENO
        ) < 0) {
        (void)dup2(
            saved_stdout,
            STDOUT_FILENO
        );

        (void)dup2(
            saved_stderr,
            STDERR_FILENO
        );

        (void)close(saved_stdout);
        (void)close(saved_stderr);
        (void)close(null_fd);

        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    status =
        sync_find_mobile_export(
            overview,
            &item_id,
            &size_bytes
        );

    if (status == TRAINLOG_STATUS_OK) {
        status =
            trainlog_mtp_receive_file(
                overview->device.bus_number,
                overview->device.device_number,
                item_id,
                local_path
            );
    }

    (void)fflush(stdout);
    (void)fflush(stderr);

    (void)dup2(
        saved_stdout,
        STDOUT_FILENO
    );

    (void)dup2(
        saved_stderr,
        STDERR_FILENO
    );

    (void)close(saved_stdout);
    (void)close(saved_stderr);
    (void)close(null_fd);

    if (status ==
        TRAINLOG_STATUS_OK) {
        *output_size =
            size_bytes;
    }

    return status;
}

static bool sync_resolve_importer_path(
    char *output,
    size_t output_size
)
{
    char executable[PATH_MAX + 1U];
    ssize_t length;
    int level;
    char *slash;
    int written;

    if (output == NULL ||
        output_size == 0U) {
        return false;
    }

    length =
        readlink(
            "/proc/self/exe",
            executable,
            PATH_MAX
        );

    if (length <= 0 ||
        (size_t)length >=
            sizeof(executable)) {
        return false;
    }

    executable[(size_t)length] =
        '\0';

    for (level = 0;
         level < 3;
         ++level) {
        slash =
            strrchr(
                executable,
                '/'
            );

        if (slash == NULL ||
            slash == executable) {
            return false;
        }

        *slash = '\0';
    }

    written =
        snprintf(
            output,
            output_size,
            "%s/tools/import_mobile_export.py",
            executable
        );

    if (written < 0 ||
        (size_t)written >=
            output_size) {
        return false;
    }

    return
        access(
            output,
            R_OK
        ) == 0;
}

static size_t sync_report_value(
    const char *text,
    const char *name
)
{
    const char *position;
    char *end = NULL;
    unsigned long long value;

    if (text == NULL ||
        name == NULL) {
        return 0U;
    }

    position =
        strstr(
            text,
            name
        );

    if (position == NULL) {
        return 0U;
    }

    position +=
        strlen(name);

    if (*position != '=') {
        return 0U;
    }

    ++position;

    value =
        strtoull(
            position,
            &end,
            10
        );

    if (end == position ||
        value >
            (unsigned long long)
                SIZE_MAX) {
        return 0U;
    }

    return (size_t)value;
}

static bool sync_read_import_report(
    const char *path,
    TrainlogMobileImportReport *report,
    char *raw_output,
    size_t raw_output_size
)
{
    FILE *file;
    size_t used;

    if (path == NULL ||
        report == NULL ||
        raw_output == NULL ||
        raw_output_size < 2U) {
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

    used =
        fread(
            raw_output,
            1U,
            raw_output_size - 1U,
            file
        );

    if (ferror(file) != 0) {
        (void)fclose(file);
        return false;
    }

    raw_output[used] =
        '\0';

    if (fclose(file) != 0) {
        return false;
    }

    if (strstr(
            raw_output,
            "MOBILE_IMPORT=PASS"
        ) == NULL) {
        return false;
    }

    (void)memset(
        report,
        0,
        sizeof(*report)
    );

    report->exercises_imported =
        sync_report_value(
            raw_output,
            "exercises_imported"
        );

    report->exercises_reconciled =
        sync_report_value(
            raw_output,
            "exercises_reconciled"
        );

    report->exercises_skipped =
        sync_report_value(
            raw_output,
            "exercises_skipped"
        );

    report->sessions_imported =
        sync_report_value(
            raw_output,
            "sessions_imported"
        );

    report->sessions_skipped =
        sync_report_value(
            raw_output,
            "sessions_skipped"
        );

    report->body_imported =
        sync_report_value(
            raw_output,
            "body_imported"
        );

    report->body_skipped =
        sync_report_value(
            raw_output,
            "body_skipped"
        );

    return true;
}

static TrainlogStatus sync_run_mobile_importer(
    TrainlogMobileImportReport *report,
    char *raw_output,
    size_t raw_output_size
)
{
    static const char *const LOCAL_EXPORT =
        "/tmp/trainlog-mobile-export-v1.json";

    static const char *const RESULT_PATH =
        "/tmp/trainlog-mobile-import-result.txt";

    char importer[PATH_MAX + 1U];
    pid_t child;
    int child_status;
    int result_fd;

    if (report == NULL ||
        raw_output == NULL ||
        raw_output_size == 0U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    if (!sync_resolve_importer_path(
            importer,
            sizeof(importer)
        )) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    result_fd =
        open(
            RESULT_PATH,
            O_WRONLY |
                O_CREAT |
                O_TRUNC,
            0600
        );

    if (result_fd < 0) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    child =
        fork();

    if (child < (pid_t)0) {
        (void)close(result_fd);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    if (child == (pid_t)0) {
        if (dup2(
                result_fd,
                STDOUT_FILENO
            ) < 0 ||
            dup2(
                result_fd,
                STDERR_FILENO
            ) < 0) {
            _exit(126);
        }

        (void)close(result_fd);

        execlp(
            "python3",
            "python3",
            importer,
            LOCAL_EXPORT,
            (char *)NULL
        );

        _exit(127);
    }

    (void)close(result_fd);

    if (waitpid(
            child,
            &child_status,
            0
        ) < (pid_t)0) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    if (!WIFEXITED(
            child_status
        ) ||
        WEXITSTATUS(
            child_status
        ) != 0) {
        (void)sync_read_import_report(
            RESULT_PATH,
            report,
            raw_output,
            raw_output_size
        );

        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    if (!sync_read_import_report(
            RESULT_PATH,
            report,
            raw_output,
            raw_output_size
        )) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus sync_import_mobile_export(
    const TrainlogSyncOverview *overview,
    TrainlogMobileImportReport *report,
    char *raw_output,
    size_t raw_output_size,
    uint64_t *output_download_size
)
{
    static const char *const LOCAL_EXPORT =
        "/tmp/trainlog-mobile-export-v1.json";

    TrainlogStatus status;

    if (overview == NULL ||
        report == NULL ||
        raw_output == NULL ||
        output_download_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status =
        sync_silenced_mobile_export_download(
            overview,
            LOCAL_EXPORT,
            output_download_size
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    return
        sync_run_mobile_importer(
            report,
            raw_output,
            raw_output_size
        );
}

static void sync_load_overview(
    TrainlogDatabase *database,
    TrainlogSyncOverview *overview
)
{
    TrainlogUsbDevice devices[SYNC_DEVICE_CAPACITY];
    TrainlogMtpStorage storages[SYNC_STORAGE_CAPACITY];
    TrainlogMtpEntry entries[SYNC_ENTRY_CAPACITY];

    size_t device_count = 0U;
    size_t storage_count = 0U;
    size_t entry_count = 0U;
    size_t index;
    bool folder_created = false;

    if (overview == NULL) {
        return;
    }

    (void)memset(
        overview,
        0,
        sizeof(*overview)
    );

    (void)trainlog_database_exercise_count(
        database,
        &overview->local_exercise_count
    );

    if (trainlog_usb_list_mtp_devices(
            devices,
            SYNC_DEVICE_CAPACITY,
            &device_count
        ) != TRAINLOG_STATUS_OK ||
        device_count == 0U) {
        return;
    }

    overview->connected = true;
    overview->device_count = device_count;
    overview->device = devices[0];

    if (trainlog_mtp_list_storages(
            overview->device.bus_number,
            overview->device.device_number,
            storages,
            SYNC_STORAGE_CAPACITY,
            &storage_count
        ) != TRAINLOG_STATUS_OK ||
        storage_count == 0U) {
        return;
    }

    overview->storage_ready = true;
    overview->storage = storages[0];

    if (trainlog_mtp_ensure_root_folder(
            overview->device.bus_number,
            overview->device.device_number,
            overview->storage.storage_id,
            "Trainlog",
            &overview->exchange_folder_id,
            &folder_created
        ) != TRAINLOG_STATUS_OK) {
        return;
    }

    (void)folder_created;
    overview->exchange_ready = true;

    if (trainlog_mtp_list_folder(
            overview->device.bus_number,
            overview->device.device_number,
            overview->storage.storage_id,
            overview->exchange_folder_id,
            entries,
            SYNC_ENTRY_CAPACITY,
            &entry_count
        ) != TRAINLOG_STATUS_OK) {
        return;
    }

    overview->remote_entry_count =
        entry_count;

    for (index = 0U;
         index < entry_count;
         ++index) {
        if (!entries[index].folder &&
            sync_name_has_json_suffix(
                entries[index].name
            )) {
            ++overview->remote_json_count;
        }
    }

    if (overview->connected &&
        overview->storage_ready) {
        uint32_t mobile_item_id = 0U;
        uint64_t mobile_size = 0U;

        if (sync_find_mobile_export(
                overview,
                &mobile_item_id,
                &mobile_size
            ) == TRAINLOG_STATUS_OK) {
            ++overview->remote_json_count;
        }
    }
}

#define SYNC_HISTORY_CAPACITY 32U
#define SYNC_HISTORY_TEXT_MAX 191U

typedef struct TrainlogSyncHistoryEntry {
    char timestamp[17];
    bool success;
    char summary[
        SYNC_HISTORY_TEXT_MAX + 1U
    ];
} TrainlogSyncHistoryEntry;

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
        getenv("XDG_DATA_HOME");

    const char *home =
        getenv("HOME");

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

static void sync_history_append(
    bool success,
    const char *summary
)
{
    char path[
        PATH_MAX + 1U
    ];

    char timestamp[17];
    time_t now;
    struct tm local_time;
    FILE *file;

    if (
        summary == NULL ||
        !sync_history_path(
            path,
            sizeof(path)
        )
    ) {
        return;
    }

    now = time(NULL);

    if (
        now == (time_t)-1 ||
        localtime_r(
            &now,
            &local_time
        ) == NULL
    ) {
        return;
    }

    if (
        strftime(
            timestamp,
            sizeof(timestamp),
            "%d/%m/%Y %H:%M",
            &local_time
        ) == 0U
    ) {
        return;
    }

    file =
        fopen(
            path,
            "ab"
        );

    if (file == NULL) {
        return;
    }

    (void)fprintf(
        file,
        "%s\t%d\t%.*s\n",
        timestamp,
        success
            ? 1
            : 0,
        (int)SYNC_HISTORY_TEXT_MAX,
        summary
    );

    (void)fclose(file);
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
    char line[512];

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
        char *first_tab;
        char *second_tab;
        char *newline;
        TrainlogSyncHistoryEntry *entry;

        first_tab =
            strchr(
                line,
                '\t'
            );

        if (first_tab == NULL) {
            continue;
        }

        *first_tab = '\0';

        second_tab =
            strchr(
                first_tab + 1,
                '\t'
            );

        if (second_tab == NULL) {
            continue;
        }

        *second_tab = '\0';

        newline =
            strchr(
                second_tab + 1,
                '\n'
            );

        if (newline != NULL) {
            *newline = '\0';
        }

        entry =
            &ring[next];

        (void)snprintf(
            entry->timestamp,
            sizeof(entry->timestamp),
            "%s",
            line
        );

        entry->success =
            strcmp(
                first_tab + 1,
                "1"
            ) == 0;

        (void)snprintf(
            entry->summary,
            sizeof(entry->summary),
            "%s",
            second_tab + 1
        );

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

    (void)fclose(file);

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

static void sync_load_overview_silenced(
    TrainlogDatabase *database,
    TrainlogSyncOverview *overview
)
{
    int saved_stdout =
        dup(STDOUT_FILENO);

    int saved_stderr =
        dup(STDERR_FILENO);

    int null_fd =
        open(
            "/dev/null",
            O_WRONLY |
                O_CLOEXEC
        );

    (void)fflush(stdout);
    (void)fflush(stderr);

    if (
        saved_stdout >= 0 &&
        saved_stderr >= 0 &&
        null_fd >= 0
    ) {
        (void)dup2(
            null_fd,
            STDOUT_FILENO
        );

        (void)dup2(
            null_fd,
            STDERR_FILENO
        );
    }

    sync_load_overview(
        database,
        overview
    );

    (void)fflush(stdout);
    (void)fflush(stderr);

    if (saved_stdout >= 0) {
        (void)dup2(
            saved_stdout,
            STDOUT_FILENO
        );

        (void)close(
            saved_stdout
        );
    }

    if (saved_stderr >= 0) {
        (void)dup2(
            saved_stderr,
            STDERR_FILENO
        );

        (void)close(
            saved_stderr
        );
    }

    if (null_fd >= 0) {
        (void)close(
            null_fd
        );
    }
}

static bool sync_resolve_repo_tool(
    const char *tool_name,
    char *output,
    size_t output_size
)
{
    char executable[
        PATH_MAX + 1U
    ];

    ssize_t length;
    int level;
    char *slash;
    int written;

    if (
        tool_name == NULL ||
        output == NULL ||
        output_size == 0U
    ) {
        return false;
    }

    length =
        readlink(
            "/proc/self/exe",
            executable,
            PATH_MAX
        );

    if (
        length <= 0 ||
        (size_t)length >=
            sizeof(executable)
    ) {
        return false;
    }

    executable[
        (size_t)length
    ] = '\0';

    for (
        level = 0;
        level < 3;
        ++level
    ) {
        slash =
            strrchr(
                executable,
                '/'
            );

        if (
            slash == NULL ||
            slash == executable
        ) {
            return false;
        }

        *slash = '\0';
    }

    written =
        snprintf(
            output,
            output_size,
            "%s/tools/%s",
            executable,
            tool_name
        );

    return
        written >= 0 &&
        (size_t)written <
            output_size &&
        access(
            output,
            R_OK
        ) == 0;
}

static TrainlogStatus sync_run_pc_catalog_export(
    size_t *output_count
)
{
    static const char *const
        OUTPUT_PATH =
            "/tmp/trainlog-pc-catalog-v1.json";

    static const char *const
        RESULT_PATH =
            "/tmp/trainlog-pc-catalog-result.txt";

    char tool[
        PATH_MAX + 1U
    ];

    char result[512];
    int result_fd;
    pid_t child;
    int child_status;
    FILE *file;
    size_t used;

    if (output_count == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = 0U;

    if (
        !sync_resolve_repo_tool(
            "export_pc_catalog.py",
            tool,
            sizeof(tool)
        )
    ) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    result_fd =
        open(
            RESULT_PATH,
            O_WRONLY |
                O_CREAT |
                O_TRUNC,
            0600
        );

    if (result_fd < 0) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    child = fork();

    if (child < (pid_t)0) {
        (void)close(result_fd);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    if (child == (pid_t)0) {
        if (
            dup2(
                result_fd,
                STDOUT_FILENO
            ) < 0 ||
            dup2(
                result_fd,
                STDERR_FILENO
            ) < 0
        ) {
            _exit(126);
        }

        (void)close(result_fd);

        execlp(
            "python3",
            "python3",
            tool,
            OUTPUT_PATH,
            (char *)NULL
        );

        _exit(127);
    }

    (void)close(result_fd);

    if (
        waitpid(
            child,
            &child_status,
            0
        ) < (pid_t)0 ||
        !WIFEXITED(
            child_status
        ) ||
        WEXITSTATUS(
            child_status
        ) != 0
    ) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    file =
        fopen(
            RESULT_PATH,
            "rb"
        );

    if (file == NULL) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    used =
        fread(
            result,
            1U,
            sizeof(result) - 1U,
            file
        );

    result[used] = '\0';

    (void)fclose(file);

    if (
        strstr(
            result,
            "PC_CATALOG_EXPORT=PASS"
        ) == NULL
    ) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    *output_count =
        sync_report_value(
            result,
            "exercises"
        );

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus sync_publish_pc_catalog(
    const TrainlogSyncOverview *overview,
    size_t *output_count,
    char *error_text,
    size_t error_text_size
)
{
    static const char *const LOCAL_PATH =
        "/tmp/trainlog-pc-catalog-v1.json";

    uint32_t download_id = 0U;
    uint32_t trainlog_id = 0U;
    uint32_t existing_id = 0U;
    uint32_t uploaded_id = 0U;
    uint64_t ignored_size = 0U;
    TrainlogStatus status;

    if (overview == NULL ||
        output_count == NULL ||
        error_text == NULL ||
        error_text_size == 0U ||
        !overview->connected ||
        !overview->storage_ready) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    error_text[0] = '\0';

    status =
        sync_run_pc_catalog_export(
            output_count
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)snprintf(
            error_text,
            error_text_size,
            "%s",
            "PC → Android : export catalogue échoué"
        );

        return status;
    }

    status =
        sync_find_mtp_child(
            &overview->device,
            overview->storage.storage_id,
            UINT32_MAX,
            "Download",
            true,
            &download_id,
            &ignored_size
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)snprintf(
            error_text,
            error_text_size,
            "%s",
            "PC → Android : dossier Download introuvable"
        );

        return status;
    }

    status =
        sync_find_mtp_child(
            &overview->device,
            overview->storage.storage_id,
            download_id,
            "Trainlog",
            true,
            &trainlog_id,
            &ignored_size
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)snprintf(
            error_text,
            error_text_size,
            "%s",
            "PC → Android : dossier Download/Trainlog introuvable"
        );

        return status;
    }

    status =
        sync_find_mtp_child(
            &overview->device,
            overview->storage.storage_id,
            trainlog_id,
            "trainlog-pc-catalog-v1.json",
            false,
            &existing_id,
            &ignored_size
        );

    if (status == TRAINLOG_STATUS_OK) {
        status =
            trainlog_mtp_delete_object(
                overview->device.bus_number,
                overview->device.device_number,
                existing_id
            );

        if (status != TRAINLOG_STATUS_OK) {
            (void)snprintf(
                error_text,
                error_text_size,
                "%s",
                "PC → Android : suppression ancien catalogue échouée"
            );

            return status;
        }
    } else if (status != TRAINLOG_STATUS_NOT_FOUND) {
        (void)snprintf(
            error_text,
            error_text_size,
            "%s",
            "PC → Android : lecture du dossier Trainlog échouée"
        );

        return status;
    }

    status =
        trainlog_mtp_send_text_file(
            overview->device.bus_number,
            overview->device.device_number,
            overview->storage.storage_id,
            trainlog_id,
            LOCAL_PATH,
            "trainlog-pc-catalog-v1.json",
            &uploaded_id
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)snprintf(
            error_text,
            error_text_size,
            "%s",
            "PC → Android : envoi MTP du catalogue échoué"
        );

        return status;
    }

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus sync_bidirectional(
    const TrainlogSyncOverview *overview,
    TrainlogMobileImportReport *report,
    char *raw_output,
    size_t raw_output_size,
    uint64_t *output_download_size
)
{
    TrainlogStatus status;
    size_t published = 0U;
    char pc_error[256];
    int saved_stdout = -1;
    int saved_stderr = -1;
    int null_fd = -1;

    if (report == NULL ||
        raw_output == NULL ||
        raw_output_size == 0U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    pc_error[0] = '\0';

    (void)fflush(stdout);
    (void)fflush(stderr);

    saved_stdout =
        dup(STDOUT_FILENO);

    saved_stderr =
        dup(STDERR_FILENO);

    null_fd =
        open(
            "/dev/null",
            O_WRONLY | O_CLOEXEC
        );

    if (saved_stdout >= 0 &&
        saved_stderr >= 0 &&
        null_fd >= 0) {
        (void)dup2(
            null_fd,
            STDOUT_FILENO
        );

        (void)dup2(
            null_fd,
            STDERR_FILENO
        );
    }

    status =
        sync_import_mobile_export(
            overview,
            report,
            raw_output,
            raw_output_size,
            output_download_size
        );

    if (status == TRAINLOG_STATUS_OK) {
        raw_output[0] = '\0';

        status =
            sync_publish_pc_catalog(
                overview,
                &published,
                pc_error,
                sizeof(pc_error)
            );

        if (status == TRAINLOG_STATUS_OK) {
            report->catalog_published =
                published;
        } else {
            (void)snprintf(
                raw_output,
                raw_output_size,
                "%s",
                pc_error[0] != '\0'
                    ? pc_error
                    : "PC → Android : échec inconnu"
            );
        }
    }

    (void)fflush(stdout);
    (void)fflush(stderr);

    if (saved_stdout >= 0) {
        (void)dup2(
            saved_stdout,
            STDOUT_FILENO
        );

        (void)close(
            saved_stdout
        );
    }

    if (saved_stderr >= 0) {
        (void)dup2(
            saved_stderr,
            STDERR_FILENO
        );

        (void)close(
            saved_stderr
        );
    }

    if (null_fd >= 0) {
        (void)close(
            null_fd
        );
    }

    clearok(
        stdscr,
        TRUE
    );

    return status;
}

static void screen_sync(
    TrainlogDatabase *database
)
{
    size_t selected = 0U;
    int nav_selected = 5;
    int focus = 1;
    TrainlogSyncOverview overview;
    bool refresh_overview = true;

    (void)memset(
        &overview,
        0,
        sizeof(overview)
    );

    for (;;) {
        TrainlogSyncHistoryEntry
            history[SYNC_HISTORY_CAPACITY];

        size_t history_count = 0U;
        bool large_layout =
            COLS >= 100 &&
            LINES >= 30;

        int key;

        if (refresh_overview) {
            sync_load_overview_silenced(
                database,
                &overview
            );

            refresh_overview = false;
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

        erase();
        box(stdscr, 0, 0);

        if (large_layout) {
            size_t index;
            size_t top = 0U;
            int history_top = 18;
            int history_bottom =
                LINES - 4;

            int visible_rows =
                history_bottom -
                history_top -
                2;

            section_ascii_header(
                ":: S Y N C ::"
            );

            primary_top_navbar(
                5,
                nav_selected,
                focus == 0
            );

            focused_panel(
                11,
                2,
                16,
                COLS - 3,
                "APPAREIL CONNECTE",
                false
            );

            if (overview.connected) {
                mvprintw(
                    12,
                    5,
                    "✓ MTP direct connecté"
                );

                mvprintw(
                    13,
                    5,
                    "%s %s",
                    overview.device.vendor,
                    overview.device.model
                );

                if (
                    overview.storage_ready
                ) {
                    mvprintw(
                        14,
                        5,
                        "Stockage interne : %.2f GiB libres / %.2f GiB",
                        sync_bytes_to_gib(
                            overview.storage.free_space_bytes
                        ),
                        sync_bytes_to_gib(
                            overview.storage.max_capacity_bytes
                        )
                    );
                }
            } else {
                mvprintw(
                    13,
                    5,
                    "Aucun appareil MTP Trainlog détecté."
                );
            }

            focused_panel(
                history_top,
                2,
                history_bottom,
                COLS - 3,
                "HISTORIQUE DES SYNCHRONISATIONS",
                focus == 1
            );

            if (history_count == 0U) {
                mvprintw(
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
                    index < (size_t)visible_rows &&
                    top + index < history_count;
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
                        attron(
                            A_REVERSE |
                            trainlog_theme_attribute(
                                TRAINLOG_COLOR_ACCENT
                            )
                        );
                    }

                    mvprintw(
                        row,
                        5,
                        " %-16s  %c  %-*.*s ",
                        history[absolute].timestamp,
                        history[absolute].success
                            ? '+'
                            : '!',
                        COLS - 28,
                        COLS - 28,
                        history[absolute].summary
                    );

                    if (
                        focus == 1 &&
                        absolute == selected
                    ) {
                        attroff(
                            A_REVERSE |
                            trainlog_theme_attribute(
                                TRAINLOG_COLOR_ACCENT
                            )
                        );
                    }
                }
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
                "Tab zone  ←→ menu  ↑↓ historique  s synchroniser les 2 sens  r actualiser  b/Échap retour"
            );

            attroff(
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_MUTED
                )
            );
        } else {
            draw_shell(
                "TRAINLOG — Sync",
                "s synchroniser les 2 sens  r actualiser  b/Échap retour"
            );

            if (overview.connected) {
                mvprintw(
                    4,
                    4,
                    "✓ %s %s",
                    overview.device.vendor,
                    overview.device.model
                );
            } else {
                mvprintw(
                    4,
                    4,
                    "Aucun appareil MTP."
                );
            }

            if (history_count == 0U) {
                mvprintw(
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
                    mvprintw(
                        7 + (int)index,
                        4,
                        "%-16s %c %.*s",
                        history[index].timestamp,
                        history[index].success
                            ? '+'
                            : '!',
                        COLS - 25,
                        history[index].summary
                    );
                }
            }
        }

        refresh();
        key = getch();

        if (
            key == 's' ||
            key == 'S'
        ) {
            TrainlogMobileImportReport report;
            char import_output[2048];
            char message[256];
            uint64_t download_size = 0U;
            TrainlogStatus status;

            (void)memset(
                &report,
                0,
                sizeof(report)
            );

            (void)memset(
                import_output,
                0,
                sizeof(import_output)
            );

            status_line(
                "Synchronisation bidirectionnelle en cours...",
                TRAINLOG_COLOR_WARNING
            );

            refresh();

            status =
                sync_bidirectional(
                    &overview,
                    &report,
                    import_output,
                    sizeof(import_output),
                    &download_size
                );

            if (
                status ==
                TRAINLOG_STATUS_OK
            ) {
                (void)snprintf(
                    message,
                    sizeof(message),
                    "Android→PC +%zu séance(s), +%zu exercice(s), +%zu mesure(s) · PC→Android catalogue %zu exercice(s)",
                    report.sessions_imported,
                    report.exercises_imported +
                        report.exercises_reconciled,
                    report.body_imported,
                    report.catalog_published
                );

                sync_history_append(
                    true,
                    message
                );

                status_line(
                    message,
                    TRAINLOG_COLOR_SUCCESS
                );
            } else {
                const char *failure =
                    import_output[0] != '\0'
                        ? import_output
                        : "Synchronisation bidirectionnelle échouée.";

                sync_history_append(
                    false,
                    failure
                );

                (void)snprintf(
                    message,
                    sizeof(message),
                    "%.*s",
                    (int)sizeof(message) - 1,
                    failure
                );

                status_line(
                    message,
                    TRAINLOG_COLOR_ERROR
                );
            }

            refresh();
            (void)getch();

            refresh_overview = true;
            continue;
        }

        if (
            key == 'r' ||
            key == 'R'
        ) {
            refresh_overview = true;
            continue;
        }

        if (
            key == 'b' ||
            key == 'B' ||
            key == 27
        ) {
            return;
        }

        if (
            large_layout &&
            (
                key == '\t' ||
                key == KEY_BTAB
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
            if (key == KEY_LEFT) {
                nav_selected =
                    nav_selected > 0
                        ? nav_selected - 1
                        : 5;
            } else if (
                key == KEY_RIGHT
            ) {
                nav_selected =
                    nav_selected < 5
                        ? nav_selected + 1
                        : 0;
            } else if (
                key == '\n' ||
                key == KEY_ENTER
            ) {
                if (
                    nav_selected == 5
                ) {
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
            key == KEY_UP &&
            history_count > 0U
        ) {
            selected =
                selected > 0U
                    ? selected - 1U
                    : history_count - 1U;
        } else if (
            key == KEY_DOWN &&
            history_count > 0U
        ) {
            selected =
                selected + 1U <
                    history_count
                    ? selected + 1U
                    : 0U;
        } else if (
            key == '0' ||
            key == KEY_HOME
        ) {
            return;
        } else if (
            key == '1' ||
            key == KEY_F(1) ||
            key == '2' ||
            key == KEY_F(2) ||
            key == '3' ||
            key == KEY_F(3) ||
            key == '4' ||
            key == KEY_F(4)
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
        case DASHBOARD_SYNC:
            screen_sync(database);
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
