/** @file test_feedback_correction.c
 *  @brief Stable-entry feedback preservation across completed-session edits. */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "trainlog/database.h"
#include "../src/database_internal.h"

#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #c); return false; \
} } while (0)

static bool exec_sql(TrainlogDatabase *db, const char *sql)
{ return sqlite3_exec(db->connection, sql, NULL, NULL, NULL) == SQLITE_OK; }

static sqlite3_int64 scalar(TrainlogDatabase *db, const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    sqlite3_int64 value = -1;
    if (sqlite3_prepare_v2(db->connection, sql, -1, &stmt, NULL) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) value = sqlite3_column_int64(stmt, 0);
    (void)sqlite3_finalize(stmt);
    return value;
}

static void bind_entry(TrainlogSessionExerciseInput *entry, const char *entry_id,
                       const char *exercise_id, TrainlogSetInput *set, int reps)
{
    (void)memset(entry, 0, sizeof(*entry));
    (void)memset(set, 0, sizeof(*set));
    (void)snprintf(entry->entry_id, sizeof(entry->entry_id), "%s", entry_id);
    (void)snprintf(entry->exercise_id, sizeof(entry->exercise_id), "%s", exercise_id);
    entry->recording_mode = TRAINLOG_RECORDING_SETS;
    entry->sets = set;
    entry->set_count = 1U;
    set->reps = reps;
}

static bool matches(const TrainlogFeedbackView *item, const char *id,
                    const char *entry, const char *at, const char *text)
{
    return strcmp(item->stable_id, id) == 0 && strcmp(item->entry_id, entry) == 0 &&
        strcmp(item->observed_at, at) == 0 && strcmp(item->raw_text, text) == 0;
}

static bool test_correction(void)
{
    TrainlogDatabase *db = NULL;
    TrainlogSessionInput session;
    TrainlogSessionExerciseInput original[2], replacement[2];
    TrainlogSetInput original_sets[2], replacement_sets[2];
    TrainlogFeedbackView feedback[8], followups[4];
    size_t feedback_count = 0U, followup_count = 0U;
    sqlite3_int64 old_a, old_b, new_a, new_b;

    CHECK(trainlog_database_open(":memory:", &db) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise(db, "ex_press", "Presse", "presse",
        TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise(db, "ex_plank", "Gainage", "gainage",
        TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    bind_entry(&original[0], "sxe_a", "ex_press", &original_sets[0], 10);
    bind_entry(&original[1], "sxe_b", "ex_plank", &original_sets[1], 20);
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", "se_feedback_edit");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
                   "2026-09-01T10:00:00+02:00");
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s",
                   "2026-09-01T11:00:00+02:00");
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = original;
    session.exercise_count = 2U;
    CHECK(trainlog_database_insert_session(db, &session) == TRAINLOG_STATUS_OK);
    CHECK(exec_sql(db,
        "INSERT INTO exercise_feedback(feedback_id,session_exercise_row_id,observed_at,raw_text) SELECT 'fb_a1',id,'2026-09-01T10:20:00+02:00','A première' FROM session_exercises WHERE entry_id='sxe_a';"
        "INSERT INTO exercise_feedback(feedback_id,session_exercise_row_id,observed_at,raw_text) SELECT 'fb_a2',id,'2026-09-01T10:30:00+02:00','A deuxième' FROM session_exercises WHERE entry_id='sxe_a';"
        "INSERT INTO exercise_feedback(feedback_id,session_exercise_row_id,observed_at,raw_text) SELECT 'fb_b1',id,'2026-09-01T10:40:00+02:00','B unique' FROM session_exercises WHERE entry_id='sxe_b';"
        "INSERT INTO session_followups(followup_id,session_row_id,observed_at,raw_text) SELECT 'fu_1',id,'2026-09-01T19:00:00+02:00','suivi inchangé' FROM sessions WHERE session_id='se_feedback_edit';"));
    /* Reproduce the pre-fix primitive: deleting reconstructed parents really
     * does cascade all feedback. Roll back, then exercise the guarded API. */
    CHECK(exec_sql(db, "BEGIN; DELETE FROM session_exercises WHERE session_row_id=(SELECT id FROM sessions WHERE session_id='se_feedback_edit');"));
    CHECK(scalar(db, "SELECT COUNT(*) FROM exercise_feedback;") == 0);
    CHECK(exec_sql(db, "ROLLBACK;"));
    CHECK(scalar(db, "SELECT COUNT(*) FROM exercise_feedback;") == 3);
    old_a = scalar(db, "SELECT id FROM session_exercises WHERE entry_id='sxe_a';");
    old_b = scalar(db, "SELECT id FROM session_exercises WHERE entry_id='sxe_b';");

    /* Reorder and change A's exercise while retaining both logical IDs. */
    bind_entry(&replacement[0], "sxe_b", "ex_plank", &replacement_sets[0], 21);
    bind_entry(&replacement[1], "sxe_a", "ex_plank", &replacement_sets[1], 11);
    CHECK(trainlog_database_replace_session_exercises(db, "se_feedback_edit",
        replacement, 2U) == TRAINLOG_STATUS_OK);
    new_a = scalar(db, "SELECT id FROM session_exercises WHERE entry_id='sxe_a';");
    new_b = scalar(db, "SELECT id FROM session_exercises WHERE entry_id='sxe_b';");
    CHECK(old_a != new_a && old_b != new_b);
    CHECK(scalar(db, "SELECT COUNT(*) FROM exercise_feedback f JOIN session_exercises se ON se.id=f.session_exercise_row_id WHERE se.entry_id='sxe_a' AND se.exercise_row_id=(SELECT id FROM exercises WHERE exercise_id='ex_plank');") == 2);
    CHECK(trainlog_database_list_training_feedback(db, "se_feedback_edit", feedback,
        8U, &feedback_count, followups, 4U, &followup_count) == TRAINLOG_STATUS_OK);
    CHECK(feedback_count == 3U && followup_count == 1U);
    CHECK(matches(&feedback[0], "fb_a1", "sxe_a", "2026-09-01T10:20:00+02:00", "A première"));
    CHECK(matches(&feedback[1], "fb_a2", "sxe_a", "2026-09-01T10:30:00+02:00", "A deuxième"));
    CHECK(matches(&feedback[2], "fb_b1", "sxe_b", "2026-09-01T10:40:00+02:00", "B unique"));
    CHECK(strcmp(followups[0].stable_id, "fu_1") == 0 &&
          strcmp(followups[0].raw_text, "suivi inchangé") == 0);

    /* Remove A and add C: B survives, A cascades, C inherits nothing. */
    bind_entry(&replacement[0], "sxe_b", "ex_plank", &replacement_sets[0], 22);
    bind_entry(&replacement[1], "sxe_c", "ex_press", &replacement_sets[1], 12);
    CHECK(trainlog_database_replace_session_exercises(db, "se_feedback_edit",
        replacement, 2U) == TRAINLOG_STATUS_OK);
    CHECK(scalar(db, "SELECT COUNT(*) FROM exercise_feedback;") == 1);
    CHECK(scalar(db, "SELECT COUNT(*) FROM exercise_feedback f JOIN session_exercises se ON se.id=f.session_exercise_row_id WHERE se.entry_id='sxe_b';") == 1);
    CHECK(scalar(db, "SELECT COUNT(*) FROM exercise_feedback f JOIN session_exercises se ON se.id=f.session_exercise_row_id WHERE se.entry_id='sxe_c';") == 0);
    CHECK(scalar(db, "SELECT COUNT(*) FROM session_followups;") == 1);

    /* Inject reattachment failure; rollback restores parent row, work and histories. */
    new_b = scalar(db, "SELECT id FROM session_exercises WHERE entry_id='sxe_b';");
    CHECK(exec_sql(db, "CREATE TRIGGER fail_correction_feedback BEFORE INSERT ON exercise_feedback BEGIN SELECT RAISE(ABORT,'synthetic correction failure'); END;"));
    bind_entry(&replacement[0], "sxe_b", "ex_press", &replacement_sets[0], 99);
    CHECK(trainlog_database_replace_session_exercises(db, "se_feedback_edit",
        replacement, 1U) == TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(scalar(db, "SELECT id FROM session_exercises WHERE entry_id='sxe_b';") == new_b);
    CHECK(scalar(db, "SELECT reps FROM performed_sets WHERE session_exercise_row_id=(SELECT id FROM session_exercises WHERE entry_id='sxe_b');") == 22);
    CHECK(scalar(db, "SELECT COUNT(*) FROM exercise_feedback WHERE feedback_id='fb_b1';") == 1);
    CHECK(scalar(db, "SELECT COUNT(*) FROM session_followups WHERE followup_id='fu_1';") == 1);
    CHECK(exec_sql(db, "DROP TRIGGER fail_correction_feedback;"));
    CHECK(scalar(db, "SELECT COUNT(*) FROM exercise_feedback f LEFT JOIN session_exercises se ON se.id=f.session_exercise_row_id WHERE se.id IS NULL;") == 0);
    trainlog_database_close(db);
    return true;
}

int main(void)
{
    if (!test_correction()) return 1;
    (void)puts("Completed-session feedback correction tests passed.");
    return 0;
}
