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

#include <curses.h>

#include "trainlog/catalog.h"
#include "trainlog/id.h"
#include "trainlog/theme.h"
#include "trainlog/timeutil.h"

#define MAX_EXERCISES 128U
#define MAX_SESSION_EXERCISES 32U
#define MAX_SETS_PER_EXERCISE 64U
#define MAX_SESSIONS 128U
#define MAX_WEIGHT_POINTS 256U

/* TRAINLOG_TUI_V02_POLISH */

typedef enum DashboardAction {
    DASHBOARD_NEW_SESSION = 0,
    DASHBOARD_HISTORY,
    DASHBOARD_EXERCISES,
    DASHBOARD_BODY,
    DASHBOARD_QUIT
} DashboardAction;

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

static void draw_weight_sparkline(
    int row,
    const TrainlogWeightPoint *points,
    size_t count
)
{
    double minimum;
    double maximum;
    size_t start;
    size_t index;
    int width;
    int height = 6;

    if (count == 0U) {
        mvprintw(row, 4, "Aucune donnée de poids.");
        return;
    }

    width = COLS - 12;
    if (width < 10) {
        return;
    }

    start = count > (size_t)width
        ? count - (size_t)width
        : 0U;

    minimum = points[start].body_weight_kg;
    maximum = points[start].body_weight_kg;

    for (index = start + 1U; index < count; ++index) {
        if (points[index].body_weight_kg < minimum) {
            minimum = points[index].body_weight_kg;
        }
        if (points[index].body_weight_kg > maximum) {
            maximum = points[index].body_weight_kg;
        }
    }

    /*
     * A flat series has no useful vertical scale. Showing the same value at
     * both ends of the axis is visually misleading, so render one centered
     * reference value instead.
     */
    if (maximum == minimum) {
        int graph_row = row + (height / 2);

        mvprintw(graph_row, 2, "%.1f kg", minimum);

        attron(trainlog_theme_attribute(TRAINLOG_COLOR_GRAPH));

        for (index = start; index < count; ++index) {
            int x = 10 + (int)(index - start);

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
            (points[index].body_weight_kg - minimum) /
            (maximum - minimum);
        int y = row + height - 1 -
            (int)(ratio * (double)(height - 1));
        int x = 8 + (int)(index - start);

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

static DashboardAction screen_dashboard(TrainlogDatabase *database)
{
    static const char *const labels[] = {
        "Nouvelle séance",
        "Historique",
        "Exercices",
        "Corps / mensurations"
    };
    size_t session_count = 0U;
    size_t exercise_count = 0U;
    TrainlogWeightPoint points[MAX_WEIGHT_POINTS];
    size_t weight_count = 0U;
    int selected = 0;

    for (;;) {
        int key;
        int index;

        (void)trainlog_database_session_count(database, &session_count);
        (void)trainlog_database_exercise_count(database, &exercise_count);
        (void)trainlog_database_list_weight_points(
            database,
            points,
            MAX_WEIGHT_POINTS,
            &weight_count
        );

        draw_shell(
            "TRAINLOG — Dashboard",
            "↑↓ naviguer  Entrée ouvrir  F1 séance  F2 historique  F3 exercices  F4 corps  q quitter"
        );

        mvprintw(
            3,
            4,
            "Séances : %-6zu   Exercices : %-6zu",
            session_count,
            exercise_count
        );

        if (weight_count > 0U) {
            double latest = points[weight_count - 1U].body_weight_kg;
            double delta = latest - points[0].body_weight_kg;

            mvprintw(
                4,
                4,
                "Poids : %.1f kg   évolution enregistrée : %+.1f kg",
                latest,
                delta
            );
        } else {
            mvprintw(4, 4, "Poids : aucune donnée");
        }

        draw_weight_sparkline(6, points, weight_count);

        for (index = 0; index < 4; ++index) {
            int menu_row = 13 + index;

            if (index == selected) {
                attron(
                    A_REVERSE |
                    trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT)
                );
            }

            mvprintw(
                menu_row,
                4,
                " %d  %-28s ",
                index + 1,
                labels[index]
            );

            if (index == selected) {
                attroff(
                    A_REVERSE |
                    trainlog_theme_attribute(TRAINLOG_COLOR_ACCENT)
                );
            }
        }

        refresh();
        key = getch();

        switch (key) {
        case KEY_UP:
            selected = selected > 0 ? selected - 1 : 3;
            break;
        case KEY_DOWN:
            selected = selected < 3 ? selected + 1 : 0;
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

static void screen_exercises(TrainlogDatabase *database)
{
    TrainlogExercise exercises[MAX_EXERCISES];
    size_t selected = 0U;

    for (;;) {
        size_t count = 0U;
        size_t top = 0U;
        size_t index;
        int visible_rows = LINES - 7;
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

        if (count > 0U && selected >= count) {
            selected = count - 1U;
        }

        if (count > 0U &&
            selected >= (size_t)visible_rows) {
            top = selected - (size_t)visible_rows + 1U;
        }

        draw_shell(
            "TRAINLOG — Exercices",
            "↑↓ naviguer  a ajouter  b/Échap retour"
        );

        if (count == 0U) {
            mvprintw(4, 4, "Aucun exercice.");
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

        if (key == 'b' || key == 27) {
            return;
        }

        if (count > 0U && key == KEY_UP) {
            selected = selected > 0U ? selected - 1U : count - 1U;
        } else if (count > 0U && key == KEY_DOWN) {
            selected = selected + 1U < count ? selected + 1U : 0U;
        } else if (key == 'a') {
            char name[TRAINLOG_NAME_MAX + 1U];
            int mode = 1;
            TrainlogExercise created;
            TrainlogStatus status;

            draw_shell("Nouvel exercice", "Entrée valide chaque champ");

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

            status = trainlog_catalog_create_exercise(
                database,
                name,
                mode == 1
                    ? TRAINLOG_TRACKING_REPS
                    : TRAINLOG_TRACKING_DURATION,
                &created
            );

            if (status == TRAINLOG_STATUS_OK) {
                status_line("✓ Exercice ajouté.", TRAINLOG_COLOR_SUCCESS);
            } else if (status == TRAINLOG_STATUS_CONFLICT) {
                status_line("Doublon détecté.", TRAINLOG_COLOR_WARNING);
            } else {
                status_line("Impossible d'ajouter l'exercice.", TRAINLOG_COLOR_ERROR);
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
    int target_metric = 10;
    int rest_seconds = 60;
    int actual_sets;
    size_t set_index;
    bool target_has_weight = false;
    double target_weight = 0.0;

    if (!choose_exercise(database, &exercise)) {
        return false;
    }

    erase();
    title(exercise.name);

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
            "Nombre de séries prévues",
            1,
            (int)set_capacity,
            3,
            &target_sets
        )) {
        return false;
    }

    if (!prompt_int_value(
            7,
            exercise.tracking_mode == TRAINLOG_TRACKING_REPS
                ? "Répétitions cibles"
                : "Durée cible (secondes)",
            1,
            10000,
            exercise.tracking_mode == TRAINLOG_TRACKING_REPS ? 10 : 45,
            &target_metric
        )) {
        return false;
    }

    if (!prompt_int_value(
            8,
            "Repos prévu (secondes)",
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
        char label[128];

        (void)memset(&set_storage[set_index], 0, sizeof(set_storage[set_index]));

        erase();
        title(exercise.name);
        mvprintw(
            3,
            2,
            "Série %zu / %zu",
            set_index + 1U,
            output->set_count
        );

        (void)snprintf(
            label,
            sizeof(label),
            "%s réalisé",
            exercise.tracking_mode == TRAINLOG_TRACKING_REPS
                ? "Répétitions"
                : "Durée (secondes)"
        );

        if (!prompt_int_value(
                5,
                label,
                exercise.tracking_mode == TRAINLOG_TRACKING_REPS ? 0 : 1,
                10000,
                target_metric,
                &actual_metric
            )) {
            return false;
        }

        if (exercise.tracking_mode == TRAINLOG_TRACKING_REPS) {
            set_storage[set_index].reps = actual_metric;
        } else {
            set_storage[set_index].duration_seconds = actual_metric;
        }

        if (target_has_weight) {
            bool has_weight = false;
            double weight = target_weight;
            char prompt[128];

            (void)snprintf(
                prompt,
                sizeof(prompt),
                "Charge kg [%.1f, vide = cible] : ",
                target_weight
            );

            {
                char buffer[64];
                if (!prompt_text(6, prompt, buffer, sizeof(buffer), true)) {
                    return false;
                }

                if (buffer[0] == '\0') {
                    has_weight = true;
                    weight = target_weight;
                } else if (parse_double_positive(buffer, &weight)) {
                    has_weight = true;
                } else {
                    status_line("Charge invalide.", TRAINLOG_COLOR_ERROR);
                    wait_key();
                    return false;
                }
            }

            set_storage[set_index].has_weight = has_weight;
            set_storage[set_index].weight_kg = weight;
        }
    }

    return true;
}

static void screen_new_session(TrainlogDatabase *database)
{
    TrainlogSessionExerciseInput exercise_inputs[MAX_SESSION_EXERCISES];
    TrainlogSetInput set_storage[MAX_SESSION_EXERCISES][MAX_SETS_PER_EXERCISE];
    TrainlogSessionInput session;
    char session_id[TRAINLOG_GENERATED_ID_CAPACITY];
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char ended_at[TRAINLOG_TIMESTAMP_MAX + 1U];
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
        mvprintw(8, 2, "Exercices : %zu", exercise_count);
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
        int key;

        draw_shell(
            "TRAINLOG — Détail séance",
            "←→ ou ↑↓ exercice précédent/suivant  b/Échap retour"
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

        if (count == 0U) {
            mvprintw(
                7,
                4,
                "Aucun exercice dans cette séance."
            );
        } else {
            TrainlogPersistedExerciseDetail *exercise =
                &exercises[selected];

            attron(
                A_BOLD |
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
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
                trainlog_theme_attribute(
                    TRAINLOG_COLOR_ACCENT
                )
            );

            mvprintw(
                8,
                4,
                "Mode : %-12s   Charge : %-10s   Repos : %ds",
                exercise->tracking_mode ==
                    TRAINLOG_TRACKING_REPS
                    ? "répétitions"
                    : "durée",
                session_detail_load_label(
                    exercise->load_mode
                ),
                exercise->rest_seconds
            );

            if (exercise->tracking_mode ==
                TRAINLOG_TRACKING_REPS) {
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
                    "Cible : %d série(s) × %ds",
                    exercise->target_sets,
                    exercise->target_duration_seconds
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
                mvprintw(
                    11,
                    4,
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
                13,
                4,
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
                15,
                4,
                "%.*s",
                COLS - 8,
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
                    17,
                    4,
                    "Assistance : plus de kg = davantage d'aide."
                );

                attroff(
                    trainlog_theme_attribute(
                        TRAINLOG_COLOR_WARNING
                    )
                );
            }
        }

        refresh();
        key = getch();

        if (key == 'b' || key == 27) {
            return;
        }

        if (count > 0U &&
            (key == KEY_RIGHT ||
             key == KEY_DOWN)) {
            selected =
                selected + 1U < count
                    ? selected + 1U
                    : 0U;
        } else if (count > 0U &&
                   (key == KEY_LEFT ||
                    key == KEY_UP)) {
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
                " %-25s  %2zu exercice(s) ",
                sessions[absolute].started_at,
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
    for (;;) {
        TrainlogWeightPoint points[MAX_WEIGHT_POINTS];
        size_t count = 0U;
        size_t start;
        size_t index;
        int key;

        (void)trainlog_database_list_weight_points(
            database,
            points,
            MAX_WEIGHT_POINTS,
            &count
        );

        draw_shell(
            "TRAINLOG — Corps",
            "a ajouter une mesure  b/Échap retour"
        );

        if (count > 0U) {
            double latest = points[count - 1U].body_weight_kg;
            double delta = latest - points[0].body_weight_kg;

            mvprintw(
                3,
                4,
                "Poids actuel : %.1f kg   évolution : %+.1f kg",
                latest,
                delta
            );
        } else {
            mvprintw(3, 4, "Aucune mesure de poids.");
        }

        draw_weight_sparkline(5, points, count);

        mvprintw(12, 4, "Dernières pesées :");
        start = count > 5U ? count - 5U : 0U;

        for (index = start; index < count; ++index) {
            mvprintw(
                13 + (int)(index - start),
                6,
                "%-25s %.1f kg",
                points[index].observed_at,
                points[index].body_weight_kg
            );
        }

        refresh();
        key = getch();

        if (key == 'b' || key == 27) {
            return;
        }

        if (key == 'a') {
            TrainlogBodyObservationInput observation;
            char id[TRAINLOG_GENERATED_ID_CAPACITY];
            char timestamp[TRAINLOG_TIMESTAMP_MAX + 1U];
            int row = 4;

            (void)memset(&observation, 0, sizeof(observation));

            if (trainlog_id_generate(
                    "bo",
                    id,
                    sizeof(id)
                ) != TRAINLOG_STATUS_OK ||
                trainlog_time_now_rfc3339(
                    timestamp,
                    sizeof(timestamp)
                ) != TRAINLOG_STATUS_OK) {
                continue;
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
                "Nouvelle mesure",
                "Laissez vide les valeurs non mesurées"
            );

#define BODY_PROMPT(label_, flag_, value_)                                   \
            do {                                                             \
                (void)prompt_optional_double(                                \
                    row++,                                                   \
                    (label_),                                                \
                    &(flag_),                                                \
                    &(value_)                                                \
                );                                                           \
            } while (0)

            BODY_PROMPT(
                "Poids kg : ",
                observation.has_body_weight,
                observation.body_weight_kg
            );
            BODY_PROMPT(
                "Tour de taille cm : ",
                observation.has_waist,
                observation.waist_cm
            );
            BODY_PROMPT(
                "Poitrine cm : ",
                observation.has_chest,
                observation.chest_cm
            );
            BODY_PROMPT(
                "Épaules cm : ",
                observation.has_shoulders,
                observation.shoulders_cm
            );
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
                status_line("✓ Mesures enregistrées.", TRAINLOG_COLOR_SUCCESS);
            } else {
                status_line(
                    "Aucune mesure valide enregistrée.",
                    TRAINLOG_COLOR_WARNING
                );
            }

            wait_key();
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
