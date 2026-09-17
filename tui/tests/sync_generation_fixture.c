/* Test-only producer fixture through the public desktop persistence API. */
#include <stdio.h>
#include <string.h>

#include "trainlog/database.h"

int main(int argc, char **argv)
{
    TrainlogDatabase *database = NULL;
    TrainlogSetInput set = { .reps = 9, .has_weight = true, .weight_kg = 32.5 };
    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;

    if (argc != 2 || trainlog_database_open(argv[1], &database) != TRAINLOG_STATUS_OK) {
        return 2;
    }
    if (trainlog_database_insert_exercise_profiled(
            database, "ex_77777777-7777-4777-8777-777777777777",
            "Desktop generation fixture", "desktop generation fixture",
            TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U) != TRAINLOG_STATUS_OK) {
        trainlog_database_close(database); return 3;
    }
    (void)memset(&exercise, 0, sizeof(exercise));
    (void)snprintf(exercise.entry_id, sizeof(exercise.entry_id), "%s",
        "sxe_88888888-8888-4888-8888-888888888888");
    (void)snprintf(exercise.exercise_id, sizeof(exercise.exercise_id), "%s",
        "ex_77777777-7777-4777-8777-777777777777");
    exercise.recording_mode = TRAINLOG_RECORDING_SETS;
    exercise.tracking_mode = TRAINLOG_TRACKING_REPS;
    exercise.sets = &set;
    exercise.set_count = 1U;
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_99999999-9999-4999-8999-999999999999");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
        "2026-09-17T10:00:00+02:00");
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s",
        "2026-09-17T10:30:00+02:00");
    session.exercises = &exercise;
    session.exercise_count = 1U;
    if (trainlog_database_insert_session(database, &session) != TRAINLOG_STATUS_OK) {
        trainlog_database_close(database); return 4;
    }
    trainlog_database_close(database);
    (void)puts("SYNC_GENERATION_FIXTURE=PASS");
    return 0;
}
