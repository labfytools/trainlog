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
    static const char *const blocks[] = {
        "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
    };
    double minimum;
    double maximum;
    size_t start;
    size_t index;
    int column = 2;

    if (count == 0U) {
        mvprintw(row, 2, "Poids : aucune donnée");
        return;
    }

    start = count > 40U ? count - 40U : 0U;
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

    mvprintw(
        row,
        2,
        "Poids %.1f kg  min %.1f  max %.1f  ",
        points[count - 1U].body_weight_kg,
        minimum,
        maximum
    );
    column = getcurx(stdscr);

    attron(trainlog_theme_attribute(TRAINLOG_COLOR_GRAPH));
    for (index = start; index < count && column < COLS - 2; ++index) {
        size_t bucket = 3U;

        if (maximum > minimum) {
            double ratio =
                (points[index].body_weight_kg - minimum) /
                (maximum - minimum);
            double scaled = ratio * 7.0;
            bucket = (size_t)(scaled + 0.5);
            if (bucket > 7U) {
                bucket = 7U;
            }
        }

        mvprintw(row, column, "%s", blocks[bucket]);
        ++column;
    }
    attroff(trainlog_theme_attribute(TRAINLOG_COLOR_GRAPH));
}

static void screen_dashboard(TrainlogDatabase *database)
{
    size_t session_count = 0U;
    size_t exercise_count = 0U;
    TrainlogWeightPoint points[MAX_WEIGHT_POINTS];
    size_t weight_count = 0U;

    erase();
    title("TRAINLOG — Dashboard");

    (void)trainlog_database_session_count(database, &session_count);
    (void)trainlog_database_exercise_count(database, &exercise_count);
    (void)trainlog_database_list_weight_points(
        database,
        points,
        MAX_WEIGHT_POINTS,
        &weight_count
    );

    mvprintw(4, 2, "Séances enregistrées : %zu", session_count);
    mvprintw(5, 2, "Exercices connus     : %zu", exercise_count);

    draw_weight_sparkline(7, points, weight_count);

    mvprintw(10, 2, "1  Nouvelle séance");
    mvprintw(11, 2, "2  Historique");
    mvprintw(12, 2, "3  Exercices");
    mvprintw(13, 2, "4  Corps / mensurations");
    mvprintw(14, 2, "q  Quitter");

    refresh();
}

static void screen_exercises(TrainlogDatabase *database)
{
    TrainlogExercise exercises[MAX_EXERCISES];
    size_t count = 0U;
    size_t index;
    int key;

    for (;;) {
        erase();
        title("TRAINLOG — Exercices");

        if (trainlog_database_list_exercises(
                database,
                exercises,
                MAX_EXERCISES,
                &count
            ) != TRAINLOG_STATUS_OK) {
            status_line("Erreur base de données.", TRAINLOG_COLOR_ERROR);
            wait_key();
            return;
        }

        if (count == 0U) {
            mvprintw(4, 2, "Aucun exercice.");
        } else {
            for (index = 0U;
                 index < count && 4 + (int)index < LINES - 5;
                 ++index) {
                mvprintw(
                    4 + (int)index,
                    2,
                    "%3zu  %-35s  [%s]",
                    index + 1U,
                    exercises[index].name,
                    exercises[index].tracking_mode == TRAINLOG_TRACKING_REPS
                        ? "reps"
                        : "durée"
                );
            }
        }

        mvprintw(LINES - 3, 2, "a Ajouter    b Retour");
        refresh();

        key = getch();
        if (key == 'b' || key == 27) {
            return;
        }

        if (key == 'a') {
            char name[TRAINLOG_NAME_MAX + 1U];
            int mode = 1;
            TrainlogExercise created;
            TrainlogStatus status;

            erase();
            title("Nouvel exercice");

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
                status_line("Exercice ajouté.", TRAINLOG_COLOR_SUCCESS);
            } else if (status == TRAINLOG_STATUS_CONFLICT) {
                status_line(
                    "Doublon détecté : nom ou identité déjà présent.",
                    TRAINLOG_COLOR_WARNING
                );
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
    size_t index;
    int selected = 0;

    if (trainlog_database_list_exercises(
            database,
            exercises,
            MAX_EXERCISES,
            &count
        ) != TRAINLOG_STATUS_OK ||
        count == 0U) {
        return false;
    }

    erase();
    title("Choisir un exercice");

    for (index = 0U;
         index < count && 4 + (int)index < LINES - 5;
         ++index) {
        mvprintw(
            4 + (int)index,
            2,
            "%3zu  %s",
            index + 1U,
            exercises[index].name
        );
    }

    if (!prompt_int_value(
            LINES - 4,
            "Numéro",
            1,
            (int)count,
            1,
            &selected
        )) {
        return false;
    }

    *output = exercises[(size_t)selected - 1U];
    return true;
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

static void screen_history(TrainlogDatabase *database)
{
    TrainlogSessionSummary sessions[MAX_SESSIONS];
    size_t count = 0U;
    size_t index;

    erase();
    title("TRAINLOG — Historique");

    if (trainlog_database_list_sessions(
            database,
            sessions,
            MAX_SESSIONS,
            &count
        ) != TRAINLOG_STATUS_OK) {
        status_line("Erreur base de données.", TRAINLOG_COLOR_ERROR);
        wait_key();
        return;
    }

    if (count == 0U) {
        mvprintw(4, 2, "Aucune séance.");
    } else {
        for (index = 0U;
             index < count && 4 + (int)index < LINES - 4;
             ++index) {
            mvprintw(
                4 + (int)index,
                2,
                "%-25s  %2zu exercice(s)",
                sessions[index].started_at,
                sessions[index].exercise_count
            );
        }
    }

    wait_key();
}

static void prompt_body_metric(
    int row,
    const char *label,
    bool *present,
    double *value
)
{
    (void)prompt_optional_double(row, label, present, value);
}

static void screen_body(TrainlogDatabase *database)
{
    TrainlogBodyObservationInput observation;
    char id[TRAINLOG_GENERATED_ID_CAPACITY];
    char timestamp[TRAINLOG_TIMESTAMP_MAX + 1U];
    TrainlogStatus status;
    int row = 4;

    (void)memset(&observation, 0, sizeof(observation));

    if (trainlog_id_generate("bo", id, sizeof(id)) != TRAINLOG_STATUS_OK ||
        trainlog_time_now_rfc3339(timestamp, sizeof(timestamp)) !=
            TRAINLOG_STATUS_OK) {
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

    erase();
    title("TRAINLOG — Corps / mensurations");
    mvprintw(3, 2, "Laissez vide ce que vous ne mesurez pas aujourd'hui.");

    prompt_body_metric(
        row++,
        "Poids kg : ",
        &observation.has_body_weight,
        &observation.body_weight_kg
    );
    prompt_body_metric(
        row++,
        "Tour de taille cm : ",
        &observation.has_waist,
        &observation.waist_cm
    );
    prompt_body_metric(
        row++,
        "Poitrine cm : ",
        &observation.has_chest,
        &observation.chest_cm
    );
    prompt_body_metric(
        row++,
        "Épaules cm : ",
        &observation.has_shoulders,
        &observation.shoulders_cm
    );
    prompt_body_metric(
        row++,
        "Bras gauche cm : ",
        &observation.has_left_arm,
        &observation.left_arm_cm
    );
    prompt_body_metric(
        row++,
        "Bras droit cm : ",
        &observation.has_right_arm,
        &observation.right_arm_cm
    );
    prompt_body_metric(
        row++,
        "Cuisse gauche cm : ",
        &observation.has_left_thigh,
        &observation.left_thigh_cm
    );
    prompt_body_metric(
        row++,
        "Cuisse droite cm : ",
        &observation.has_right_thigh,
        &observation.right_thigh_cm
    );
    prompt_body_metric(
        row++,
        "Mollet gauche cm : ",
        &observation.has_left_calf,
        &observation.left_calf_cm
    );
    prompt_body_metric(
        row++,
        "Mollet droit cm : ",
        &observation.has_right_calf,
        &observation.right_calf_cm
    );

    status = trainlog_database_insert_body_observation(
        database,
        &observation
    );

    if (status == TRAINLOG_STATUS_OK) {
        status_line("✓ Mesures enregistrées.", TRAINLOG_COLOR_SUCCESS);
    } else if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
        status_line(
            "Aucune mesure saisie : rien n'a été enregistré.",
            TRAINLOG_COLOR_WARNING
        );
    } else {
        status_line(
            "Impossible d'enregistrer les mesures.",
            TRAINLOG_COLOR_ERROR
        );
    }

    wait_key();
}

int trainlog_tui_run(TrainlogDatabase *database)
{
    int key;

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
        if (LINES < 20 || COLS < 72) {
            erase();
            mvprintw(
                1,
                2,
                "Terminal trop petit — minimum 72x20."
            );
            mvprintw(3, 2, "q pour quitter");
            refresh();

            key = getch();
            if (key == 'q') {
                break;
            }
            continue;
        }

        screen_dashboard(database);
        key = getch();

        switch (key) {
        case '1':
            screen_new_session(database);
            break;
        case '2':
            screen_history(database);
            break;
        case '3':
            screen_exercises(database);
            break;
        case '4':
            screen_body(database);
            break;
        case 'q':
        case 'Q':
            endwin();
            return 0;
        default:
            break;
        }
    }

    endwin();
    return 0;
}
