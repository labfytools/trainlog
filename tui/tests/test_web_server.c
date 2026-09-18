#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

#include "database_internal.h"
#include "trainlog/database.h"
#include "trainlog/dashboard_layout.h"
#include "trainlog/web_server.h"
#include "trainlog/web_sessions.h"
#include "web_assets.h"

#define CHECK(value)                                                                               \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #value);      \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static volatile sig_atomic_t child_stop;

static void stop_child(int signal_number) {
    (void)signal_number;
    child_stop = 1;
}

static int reserve_port(uint16_t *port, int keep_open) {
    struct sockaddr_in address;
    socklen_t length = (socklen_t)sizeof(address);
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1;
    }
    (void)memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socket_fd, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        getsockname(socket_fd, (struct sockaddr *)&address, &length) != 0 ||
        (keep_open != 0 && listen(socket_fd, 1) != 0)) {
        (void)close(socket_fd);
        return -1;
    }
    *port = ntohs(address.sin_port);
    if (keep_open == 0) {
        (void)close(socket_fd);
        return -2;
    }
    return socket_fd;
}

static int connect_with_retry(uint16_t port) {
    struct sockaddr_in address;
    struct timespec delay = {0, 10000000L};
    size_t attempt;
    (void)memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    for (attempt = 0U; attempt < 200U; ++attempt) {
        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd >= 0 &&
            connect(socket_fd, (const struct sockaddr *)&address, sizeof(address)) == 0) {
            return socket_fd;
        }
        if (socket_fd >= 0) {
            (void)close(socket_fd);
        }
        (void)nanosleep(&delay, NULL);
    }
    return -1;
}

static bool exchange(uint16_t port, const char *request, char *response, size_t capacity) {
    size_t sent = 0U;
    size_t used = 0U;
    size_t length = strlen(request);
    int socket_fd = connect_with_retry(port);
    CHECK(socket_fd >= 0);
    while (sent < length) {
        ssize_t count = send(socket_fd, request + sent, length - sent, 0);
        CHECK(count > 0);
        sent += (size_t)count;
    }
    (void)shutdown(socket_fd, SHUT_WR);
    while (used + 1U < capacity) {
        ssize_t count = recv(socket_fd, response + used, capacity - used - 1U, 0);
        if (count == 0) {
            break;
        }
        CHECK(count > 0);
        used += (size_t)count;
    }
    response[used] = '\0';
    (void)close(socket_fd);
    return true;
}

static bool asset_path(const char *html, const char *suffix, char *output, size_t capacity) {
    const char *start = html;
    size_t suffix_length = strlen(suffix);
    while ((start = strstr(start, "/assets/")) != NULL) {
        const char *end = start;
        size_t length;
        while (*end != '\0' && *end != '"' && *end != '\'' && *end != '<' && *end != '>') {
            ++end;
        }
        length = (size_t)(end - start);
        if (length >= suffix_length && memcmp(end - suffix_length, suffix, suffix_length) == 0 &&
            length < capacity) {
            (void)memcpy(output, start, length);
            output[length] = '\0';
            return true;
        }
        start = end;
    }
    return false;
}

static bool response_header(const char *response, const char *name, char *output, size_t capacity) {
    const char *start = strstr(response, name);
    const char *end;
    size_t length;
    if (start == NULL) {
        return false;
    }
    start += strlen(name);
    end = strstr(start, "\r\n");
    if (end == NULL) {
        return false;
    }
    length = (size_t)(end - start);
    if (length >= capacity) {
        return false;
    }
    (void)memcpy(output, start, length);
    output[length] = '\0';
    return true;
}

static bool json_string(const char *json, const char *key, char *output, size_t capacity) {
    char marker[96];
    const char *start;
    const char *end;
    size_t length;

    CHECK(snprintf(marker, sizeof(marker), "\"%s\":\"", key) > 0);
    start = strstr(json, marker);
    CHECK(start != NULL);
    start += strlen(marker);
    end = strchr(start, '"');
    CHECK(end != NULL);
    length = (size_t)(end - start);
    CHECK(length > 0U && length < capacity);
    (void)memcpy(output, start, length);
    output[length] = '\0';
    return true;
}

static bool test_sync_accepted_serialization(void) {
    static const char REQUEST_ID[] = "sy_11111111-1111-4111-8111-111111111111";
    static const char RUN_ID[] = "sy_22222222-2222-4222-8222-222222222222";
    char output[256];
    char exact[256];
    char too_small[256];
    size_t output_size;

    CHECK(trainlog_web_sync_accepted_serialize(
        REQUEST_ID, RUN_ID, output, sizeof(output), &output_size));
    CHECK(output_size == strlen(output));
    CHECK(strstr(output, "\"request_id\":\"sy_11111111-1111-4111-8111-111111111111\"") != NULL);
    CHECK(strstr(output, "\"run_id\":\"sy_22222222-2222-4222-8222-222222222222\"") != NULL);
    CHECK(strstr(output, "\"result\":\"running\"") != NULL);
    CHECK(trainlog_web_sync_accepted_serialize(
        REQUEST_ID, RUN_ID, exact, output_size + 1U, &output_size));
    CHECK(strcmp(exact, output) == 0);
    CHECK(!trainlog_web_sync_accepted_serialize(
        REQUEST_ID, RUN_ID, too_small, output_size, &output_size));
    CHECK(too_small[0] == '\0');
    CHECK(!trainlog_web_sync_accepted_serialize(
        "sy_invalid", RUN_ID, output, sizeof(output), &output_size));
    return true;
}

static bool test_http_contract(TrainlogDatabase *database) {
    static const char READY_BODY[] =
        "{\"title\":\"Préparation HTTP\",\"session_type\":\"training\","
        "\"planned_for\":\"2026-09-20\",\"notes\":null,\"editing_state\":\"ready\","
        "\"occurrences\":[{\"exercise_id\":\"ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\","
        "\"equipment_id\":null,\"load_mode\":\"none\",\"rest_seconds\":60,"
        "\"target_sets\":2,\"target_reps\":8,\"target_duration_seconds\":null,"
        "\"target_weight_kg\":null,\"notes\":null}]}";
    char response[32768];
    char large_request[12000];
    char request[8192];
    char javascript_path[256];
    char stylesheet_path[256];
    uint16_t port;
    pid_t child;
    int status;
    struct sigaction action;
    char *created = NULL;
    size_t created_size = 0U;
    char preparation_id[128];
    char revision_id[128];

    CHECK(trainlog_database_insert_exercise_profiled(database,
                                                     "ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
                                                     "Exercice HTTP",
                                                     "exercice http",
                                                     TRAINLOG_TRACKING_REPS,
                                                     TRAINLOG_RECORDING_SETS,
                                                     0U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_web_sessions_save_json(database,
                                          NULL,
                                          "",
                                          "request-http-create",
                                          READY_BODY,
                                          strlen(READY_BODY),
                                          &created,
                                          &created_size) == TRAINLOG_STATUS_OK);
    CHECK(created_size > 0U);
    CHECK(json_string(created, "preparation_id", preparation_id, sizeof(preparation_id)));
    CHECK(json_string(created, "revision_id", revision_id, sizeof(revision_id)));
    free(created);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO ai_session_drafts("
                       "draft_id,created_at,session_type,title,archive_status) VALUES("
                       "'aid_66666666-6666-4666-8666-666666666666',"
                       "'2026-09-18T10:00:00Z','training','Proposal HTTP','archived');"
                       "INSERT INTO ai_session_draft_imports VALUES("
                       "last_insert_rowid(),'aid_66666666-6666-4666-8666-666666666666',"
                       "'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',"
                       "'2026-09-18T10:00:00Z');",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(database->connection,
                       "INSERT INTO programs(program_id,title,state,created_at,updated_at,"
                       "revision_id,source_format,source_version,source_payload_sha256) VALUES("
                       "'pg_77777777-7777-4777-8777-777777777777','HTTP Program','active',"
                       "'2026-09-18T10:00:00Z','2026-09-18T10:00:00Z','pgr_http',"
                       "'trainlog-program',1,"
                       "'cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc');"
                       "INSERT INTO program_sessions(program_session_id,program_id,position,title,"
                       "session_type) VALUES('pgs_88888888-8888-4888-8888-888888888888',"
                       "'pg_77777777-7777-4777-8777-777777777777',0,'HTTP execution','training');"
                       "INSERT INTO sessions(session_id,started_at,ended_at,session_type) VALUES("
                       "'se_99999999-9999-4999-8999-999999999999','2026-09-18T12:00:00Z',"
                       "'2026-09-18T13:00:00Z','training'),("
                       "'se_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','2026-09-18T14:00:00Z',"
                       "'2026-09-18T15:00:00Z','training');"
                       "INSERT INTO sync_causal_state VALUES("
                       "'session','se_99999999-9999-4999-8999-999999999999','lv_http',0,NULL),("
                       "'session','se_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','lv_failure',0,NULL);"
                       "INSERT INTO program_session_executions VALUES("
                       "'pgs_88888888-8888-4888-8888-888888888888',"
                       "'pg_77777777-7777-4777-8777-777777777777',"
                       "'se_99999999-9999-4999-8999-999999999999','completed',"
                       "'2026-09-18T13:00:00Z');"
                       "CREATE TRIGGER inject_session_delete_failure BEFORE DELETE ON sessions "
                       "WHEN OLD.session_id='se_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa' BEGIN "
                       "SELECT RAISE(ABORT,'injected session deletion failure');END;",
                       NULL,
                       NULL,
                       NULL) == SQLITE_OK);
    (void)unlink("/tmp/trainlog-web-test.db.sync-run.json");
    CHECK(reserve_port(&port, 0) == -2);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        char diagnostic[256];
        (void)memset(&action, 0, sizeof(action));
        action.sa_handler = stop_child;
        (void)sigemptyset(&action.sa_mask);
        (void)sigaction(SIGTERM, &action, NULL);
        child_stop = 0;
        _exit(trainlog_web_server_run(database,
                                      "/tmp/trainlog-web-test.db",
                                      port,
                                      &child_stop,
                                      diagnostic,
                                      sizeof(diagnostic)) == 0
                  ? 0
                  : 1);
    }
    CHECK(exchange(port,
                   "GET /api/v1/health HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                   response,
                   sizeof(response)));
    CHECK(strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "Content-Type: application/json; charset=utf-8") != NULL);
    CHECK(strstr(response, "X-Content-Type-Options: nosniff") != NULL);
    CHECK(strstr(response, "Content-Security-Policy: default-src 'none'") != NULL);
    CHECK(strstr(response, "Access-Control-Allow-Origin") == NULL);
    CHECK(strstr(response,
                 "{\"api_version\":1,\"status\":\"ok\",\"product\":\"trainlog\","
                 "\"version\":\"0.1.2\"}") != NULL);
    {
        char token[80];
        CHECK(exchange(port,
                       "GET /api/v1/sync/status HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL);
        CHECK(strstr(response, "\"phase\":\"idle\"") != NULL);
        CHECK(response_header(response, "X-Trainlog-CSRF-Token: ", token, sizeof(token)) &&
              strlen(token) == 64U);
        (void)snprintf(
            request,
            sizeof(request),
            "POST /api/v1/sync HTTP/1.1\r\nHost: 127.0.0.1\r\n"
            "Origin: http://127.0.0.1:%u\r\nContent-Type: application/json\r\n"
            "X-Trainlog-CSRF-Token: %s\r\nContent-Length: 72\r\n\r\n"
            "{\"request_id\":\"sy_11111111-1111-4111-8111-111111111111\",\"trigger\":\"web\"}",
            (unsigned int)port,
            token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 409") != NULL &&
              strstr(response, "full_generation_disabled") != NULL);
        CHECK(exchange(port,
                       "GET /api/v1/sync HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 405") != NULL);
        (void)snprintf(
            request,
            sizeof(request),
            "DELETE /api/v1/sessions/preparation/%s HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\nOrigin: http://127.0.0.1:%u\r\n"
            "Content-Type: application/json\r\nIf-Match: \"%s\"\r\n"
            "X-Trainlog-CSRF-Token: wrong\r\nX-Trainlog-Request-ID: request-http-delete\r\n"
            "Content-Length: 2\r\n\r\n{}",
            preparation_id,
            (unsigned int)port,
            revision_id);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 403") != NULL);
        (void)snprintf(request,
                       sizeof(request),
                       "DELETE /api/v1/sessions/preparation/%s HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\nOrigin: http://127.0.0.1:%u\r\n"
                       "Content-Type: application/json\r\nIf-Match: \"%s\"\r\n"
                       "X-Trainlog-CSRF-Token: %s\r\nX-Trainlog-Request-ID: request-http-delete\r\n"
                       "Content-Length: 2\r\n\r\n{}",
                       preparation_id,
                       (unsigned int)port,
                       revision_id,
                       token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"android_cancellation\":\"not_required\"") != NULL);
        (void)snprintf(request,
                       sizeof(request),
                       "GET /api/v1/sessions/preparation/%s HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n\r\n",
                       preparation_id);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"state\":\"withdrawn\"") != NULL);
        (void)snprintf(request,
                       sizeof(request),
                       "DELETE /api/v1/sessions/proposal/"
                       "aid_66666666-6666-4666-8666-666666666666 HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\nOrigin: http://127.0.0.1:%u\r\n"
                       "Content-Type: application/json\r\nIf-Match: \"stale\"\r\n"
                       "X-Trainlog-CSRF-Token: %s\r\n"
                       "X-Trainlog-Request-ID: request-http-delete-proposal-stale\r\n"
                       "Content-Length: 2\r\n\r\n{}",
                       (unsigned int)port,
                       token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 409") != NULL);
        (void)snprintf(
            request,
            sizeof(request),
            "DELETE /api/v1/sessions/proposal/"
            "aid_66666666-6666-4666-8666-666666666666 HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\nOrigin: http://127.0.0.1:%u\r\n"
            "Content-Type: application/json\r\n"
            "If-Match: \"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"\r\n"
            "X-Trainlog-CSRF-Token: %s\r\n"
            "X-Trainlog-Request-ID: request-http-delete-proposal\r\n"
            "Content-Length: 2\r\n\r\n{}",
            (unsigned int)port,
            token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"state\":\"deleted\"") != NULL);
        CHECK(exchange(port,
                       "GET /api/v1/sessions/proposal/"
                       "aid_66666666-6666-4666-8666-666666666666 HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 404") != NULL);
        CHECK(exchange(port,
                       "GET /api/v1/sessions/history/"
                       "se_99999999-9999-4999-8999-999999999999 HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"revision_id\":\"lv_http\"") != NULL);
        (void)snprintf(
            request,
            sizeof(request),
            "DELETE /api/v1/sessions/history/"
            "se_99999999-9999-4999-8999-999999999999 HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\nOrigin: http://127.0.0.1:%u\r\n"
            "Content-Type: application/json\r\nIf-Match: \"stale\"\r\n"
            "X-Trainlog-CSRF-Token: %s\r\nX-Trainlog-Request-ID: request-http-history-stale\r\n"
            "Content-Length: 2\r\n\r\n{}",
            (unsigned int)port,
            token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 409") != NULL &&
              strstr(response, "deletion_conflict") != NULL);
        (void)snprintf(
            request,
            sizeof(request),
            "DELETE /api/v1/sessions/history/"
            "se_99999999-9999-4999-8999-999999999999 HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\nOrigin: http://127.0.0.1:%u\r\n"
            "Content-Type: application/json\r\nIf-Match: \"lv_http\"\r\n"
            "X-Trainlog-CSRF-Token: %s\r\nX-Trainlog-Request-ID: request-http-history-delete\r\n"
            "Content-Length: 2\r\n\r\n{}",
            (unsigned int)port,
            token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"state\":\"deleted\"") != NULL);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL);
        CHECK(exchange(port,
                       "GET /api/v1/sessions/program/"
                       "pg_77777777-7777-4777-8777-777777777777 HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"execution_state\":\"deleted\"") != NULL &&
              strstr(response, "\"execution_session_id\":null") != NULL);
        CHECK(exchange(port,
                       "GET /api/v1/sessions/history/"
                       "se_99999999-9999-4999-8999-999999999999 HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 404") != NULL);
        (void)snprintf(
            request,
            sizeof(request),
            "DELETE /api/v1/sessions/history/not/a/session HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\nOrigin: http://127.0.0.1:%u\r\n"
            "Content-Type: application/json\r\nIf-Match: \"revision\"\r\n"
            "X-Trainlog-CSRF-Token: %s\r\nX-Trainlog-Request-ID: request-http-invalid-delete\r\n"
            "Content-Length: 2\r\n\r\n{}",
            (unsigned int)port,
            token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 422") != NULL &&
              strstr(response, "invalid_deletion") != NULL);
        (void)snprintf(
            request,
            sizeof(request),
            "DELETE /api/v1/sessions/history/"
            "se_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\nOrigin: http://127.0.0.1:%u\r\n"
            "Content-Type: application/json\r\nIf-Match: \"lv_failure\"\r\n"
            "X-Trainlog-CSRF-Token: %s\r\nX-Trainlog-Request-ID: request-http-db-failure\r\n"
            "Content-Length: 2\r\n\r\n{}",
            (unsigned int)port,
            token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 500") != NULL &&
              strstr(response, "sessions_unavailable") != NULL);
        CHECK(exchange(port,
                       "GET /api/v1/sessions/history/"
                       "se_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa HTTP/1.1\r\n"
                       "Host: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"revision_id\":\"lv_failure\"") != NULL);
    }
    CHECK(exchange(port,
                   "GET /api/v1/dashboard HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "Content-Type: application/json; charset=utf-8") != NULL);
    CHECK(strstr(response, "\"api_version\":1") != NULL);
    CHECK(strstr(response, "\"no_persisted_executable_plan\"") != NULL);
    CHECK(strstr(response, "\"no_cardio_data_source\"") != NULL);
    CHECK(strstr(response, "\"window_days\":90") != NULL);
    CHECK(strstr(response, "\"partial\":false") != NULL);
    CHECK(exchange(port,
                   "GET /api/v1/prepared-items HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "\"api_version\":1") != NULL);
    CHECK(strstr(response, "\"items\":[]") != NULL);
    {
        char token[80];
        char etag[32];
        CHECK(exchange(port,
                       "GET /api/v1/web-preferences HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"date_format\":\"fr\"") != NULL &&
              strstr(response, "\"source\":\"default\"") != NULL);
        CHECK(response_header(response, "ETag: ", etag, sizeof(etag)) &&
              strcmp(etag, "\"0\"") == 0);
        CHECK(response_header(response, "X-Trainlog-CSRF-Token: ", token, sizeof(token)));
        (void)snprintf(request,
                       sizeof(request),
                       "PUT /api/v1/web-preferences HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                       "Origin: http://127.0.0.1:%u\r\nContent-Type: application/json\r\n"
                       "If-Match: \"0\"\r\nX-Trainlog-CSRF-Token: %s\r\nContent-Length: 82\r\n\r\n"
                       "{\"format\":\"trainlog-web-preferences\",\"version\":1,\"revision\":0,"
                       "\"date_format\":\"iso\"}",
                       (unsigned int)port,
                       token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"revision\":1") != NULL &&
              strstr(response, "\"date_format\":\"iso\"") != NULL &&
              strstr(response, "\"source\":\"persisted\"") != NULL);
        CHECK(exchange(port,
                       "GET /api/v1/web-preferences HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"revision\":1") != NULL &&
              strstr(response, "\"date_format\":\"iso\"") != NULL);
        {
            char path[4096];
            FILE *stream;
            const char *config = getenv("XDG_CONFIG_HOME");

            CHECK(config != NULL);
            CHECK(snprintf(path, sizeof(path), "%s/trainlog/web/preferences-v1.json", config) > 0);
            stream = fopen(path, "w");
            CHECK(stream != NULL);
            CHECK(fputs("invalid preference\n", stream) >= 0);
            CHECK(fclose(stream) == 0);
        }
        CHECK(exchange(port,
                       "GET /api/v1/web-preferences HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"date_format\":\"fr\"") != NULL &&
              strstr(response, "\"source\":\"invalid_persisted\"") != NULL);
    }
    CHECK(exchange(port,
                   "POST /api/v1/prepared-items HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Content-Length: 0\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 405") != NULL);
    {
        TrainlogDashboardLayout layout;
        char body[TRAINLOG_DASHBOARD_LAYOUT_JSON_CAPACITY];
        char token[80];
        char etag[32];
        size_t body_size;
        CHECK(exchange(port,
                       "GET /api/v1/dashboard-layout HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL);
        CHECK(strstr(response, "\"source\":\"default\"") != NULL);
        CHECK(response_header(response, "ETag: ", etag, sizeof(etag)) &&
              strcmp(etag, "\"0\"") == 0);
        CHECK(response_header(response, "X-Trainlog-CSRF-Token: ", token, sizeof(token)) &&
              strlen(token) == 64U);
        trainlog_dashboard_layout_default(&layout);
        layout.tiles[3].y = 14U;
        CHECK(trainlog_dashboard_layout_serialize(
            &layout, TRAINLOG_DASHBOARD_LAYOUT_DEFAULT, false, body, sizeof(body), &body_size));
        (void)snprintf(request,
                       sizeof(request),
                       "PUT /api/v1/dashboard-layout HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                       "Origin: http://127.0.0.1:%u\r\nContent-Type: application/json\r\n"
                       "If-Match: \"0\"\r\nX-Trainlog-CSRF-Token: wrong\r\n"
                       "Content-Length: %zu\r\n\r\n%s",
                       (unsigned int)port,
                       body_size,
                       body);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 403") != NULL);
        (void)snprintf(request,
                       sizeof(request),
                       "PUT /api/v1/dashboard-layout HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                       "Origin: http://127.0.0.1:%u\r\nContent-Type: text/plain\r\n"
                       "If-Match: \"0\"\r\nX-Trainlog-CSRF-Token: %s\r\n"
                       "Content-Length: %zu\r\n\r\n%s",
                       (unsigned int)port,
                       token,
                       body_size,
                       body);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 415") != NULL);
        (void)snprintf(
            request,
            sizeof(request),
            "PUT /api/v1/dashboard-layout HTTP/1.1\r\nHost: 127.0.0.1\r\n"
            "Origin: http://127.0.0.1:%u\r\nContent-Type: application/json\r\n"
            "If-Match: \"0\"\r\nX-Trainlog-CSRF-Token: %s\r\nContent-Length: %zu\r\n\r\n%s",
            (unsigned int)port,
            token,
            body_size,
            body);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL);
        CHECK(strstr(response, "\"revision\":1") != NULL &&
              strstr(response, "\"source\":\"persisted\"") != NULL);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 412") != NULL);
        (void)snprintf(request,
                       sizeof(request),
                       "DELETE /api/v1/dashboard-layout HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                       "Origin: http://trainlog.perf\r\nIf-Match: \"0\"\r\n"
                       "X-Trainlog-CSRF-Token: %s\r\nContent-Length: 0\r\n\r\n",
                       token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 412") != NULL);
        (void)snprintf(request,
                       sizeof(request),
                       "DELETE /api/v1/dashboard-layout HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                       "Origin: http://trainlog.perf\r\nIf-Match: \"1\"\r\n"
                       "X-Trainlog-CSRF-Token: %s\r\nContent-Length: 0\r\n\r\n",
                       token);
        CHECK(exchange(port, request, response, sizeof(response)) &&
              strstr(response, "HTTP/1.1 200") != NULL &&
              strstr(response, "\"source\":\"default\"") != NULL);
        CHECK(exchange(port,
                       "PUT /api/v1/dashboard-layout HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                       "Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}",
                       response,
                       sizeof(response)) &&
              strstr(response, "HTTP/1.1 403") != NULL);
    }
    CHECK(
        exchange(port,
                 "POST /api/v1/dashboard HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 0\r\n\r\n",
                 response,
                 sizeof(response)) &&
        strstr(response, "HTTP/1.1 405") != NULL && strstr(response, "Allow: GET") != NULL);
    CHECK(exchange(port, "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", response, sizeof(response)) &&
          strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "Content-Type: text/html; charset=utf-8") != NULL);
    CHECK(strstr(response, "Cache-Control: no-cache") != NULL);
    CHECK(strstr(response, "Content-Security-Policy: default-src 'self'") != NULL);
    CHECK(strstr(response, "unsafe-inline") == NULL);
    CHECK(strstr(response, "unsafe-eval") == NULL);
    CHECK(strstr(response, "<title>Trainlog</title>") != NULL);
    CHECK(asset_path(response, ".js", javascript_path, sizeof(javascript_path)));
    CHECK(asset_path(response, ".css", stylesheet_path, sizeof(stylesheet_path)));
    (void)snprintf(
        request, sizeof(request), "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", javascript_path);
    CHECK(exchange(port, request, response, sizeof(response)) &&
          strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "Content-Type: text/javascript; charset=utf-8") != NULL);
    CHECK(strstr(response, "Cache-Control: public, max-age=31536000, immutable") != NULL);
    CHECK(strstr(response, "ETag: \"sha256-") != NULL);
    (void)snprintf(
        request, sizeof(request), "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", stylesheet_path);
    CHECK(exchange(port, request, response, sizeof(response)) &&
          strstr(response, "Content-Type: text/css; charset=utf-8") != NULL);
    CHECK(
        exchange(
            port, "GET /analyse HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", response, sizeof(response)) &&
        strstr(response, "<title>Trainlog</title>") != NULL);
    CHECK(exchange(port,
                   "GET /seances/preparation/sp_11111111-1111-4111-8111-111111111111 "
                   "HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 200") != NULL &&
          strstr(response, "<title>Trainlog</title>") != NULL);
    CHECK(exchange(port,
                   "GET /api/v1/unknown HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 404") != NULL &&
          strstr(response, "<title>Trainlog</title>") == NULL);
    CHECK(exchange(port,
                   "GET /assets/unknown.js HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 404") != NULL);
    CHECK(exchange(port,
                   "GET /assets/../index.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 404") != NULL);
    (void)snprintf(
        request, sizeof(request), "HEAD %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", javascript_path);
    CHECK(exchange(port, request, response, sizeof(response)) &&
          strstr(response, "HTTP/1.1 200") != NULL &&
          strstr(response, "Content-Type: text/javascript; charset=utf-8") != NULL);
    CHECK(exchange(port,
                   "POST /api/v1/health HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 0\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 405") != NULL);
    CHECK(strstr(response, "Allow: GET") != NULL);
    CHECK(exchange(port,
                   "GET /api/v1/health HTTP/1.1\r\nHost: foreign.example\r\n\r\n",
                   response,
                   sizeof(response)) &&
          strstr(response, "HTTP/1.1 400") != NULL);
    (void)snprintf(large_request,
                   sizeof(large_request),
                   "POST /api/v1/health HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Content-Length: 5000\r\n\r\n%05000d",
                   0);
    CHECK(exchange(port, large_request, response, sizeof(response)) &&
          strstr(response, "HTTP/1.1 413") != NULL);
    (void)memset(large_request, 'a', sizeof(large_request));
    (void)snprintf(
        large_request, 128U, "GET /api/v1/health HTTP/1.1\r\nHost: 127.0.0.1\r\nX-Large: ");
    {
        size_t prefix = strlen(large_request);
        (void)memset(large_request + prefix, 'a', 8500U);
        (void)memcpy(large_request + prefix + 8500U, "\r\n\r\n", 5U);
    }
    CHECK(exchange(port, large_request, response, sizeof(response)) &&
          (strstr(response, "HTTP/1.1 431") != NULL || strstr(response, "HTTP/1.1 400") != NULL));
    CHECK(kill(child, SIGTERM) == 0);
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return true;
}

static bool test_port_in_use(TrainlogDatabase *database) {
    volatile sig_atomic_t stop = 0;
    char diagnostic[256];
    uint16_t port;
    int occupied = reserve_port(&port, 1);
    CHECK(occupied >= 0);
    CHECK(trainlog_web_server_run(
              database, "/tmp/trainlog-web-test.db", port, &stop, diagnostic, sizeof(diagnostic)) !=
          0);
    CHECK(strstr(diagnostic, "127.0.0.1") != NULL);
    (void)close(occupied);
    return true;
}

static bool test_dashboard_core_error_translation(void) {
    char path[] = "/tmp/trainlog-web-error-XXXXXX";
    char response[4096];
    TrainlogDatabase *database = NULL;
    sqlite3 *raw = NULL;
    uint16_t port;
    pid_t child;
    int fd = mkstemp(path);
    int status;
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    trainlog_database_close(database);
    database = NULL;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "DROP TABLE sessions;", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(reserve_port(&port, 0) == -2);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        volatile sig_atomic_t stop = 0;
        char diagnostic[256];
        _exit(trainlog_web_server_run(
                  database, path, port, &stop, diagnostic, sizeof(diagnostic)) == 0
                  ? 0
                  : 1);
    }
    CHECK(exchange(port,
                   "GET /api/v1/dashboard HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
                   response,
                   sizeof(response)));
    CHECK(strstr(response, "HTTP/1.1 500") != NULL);
    CHECK(strstr(response, "\"dashboard_unavailable\"") != NULL);
    CHECK(kill(child, SIGTERM) == 0);
    CHECK(waitpid(child, &status, 0) == child);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return true;
}

int main(void) {
    char config_root[] = "/tmp/trainlog-web-config-XXXXXX";
    TrainlogDatabase *database = NULL;
    bool passed;
    if (mkdtemp(config_root) == NULL || setenv("XDG_CONFIG_HOME", config_root, 1) != 0) {
        return 1;
    }
    if (trainlog_database_open(":memory:", &database) != TRAINLOG_STATUS_OK) {
        return 1;
    }
    if (!trainlog_web_assets_available()) {
        volatile sig_atomic_t stop = 0;
        char diagnostic[256];
        passed = trainlog_web_server_run(database,
                                         "/tmp/trainlog-web-test.db",
                                         8080U,
                                         &stop,
                                         diagnostic,
                                         sizeof(diagnostic)) != 0 &&
                 strstr(diagnostic, "frontend") != NULL;
    } else {
        passed = test_sync_accepted_serialization() && test_http_contract(database) &&
                 test_port_in_use(database) && test_dashboard_core_error_translation();
    }
    trainlog_database_close(database);
    return passed ? 0 : 1;
}
