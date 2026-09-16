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

#include "trainlog/database.h"
#include "trainlog/web_server.h"
#include "web_assets.h"

#define CHECK(value) do { if (!(value)) {                                  \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                 \
        __FILE__, __LINE__, #value); return false; } } while (0)

static volatile sig_atomic_t child_stop;

static void stop_child(int signal_number)
{
    (void)signal_number;
    child_stop = 1;
}

static int reserve_port(uint16_t *port, int keep_open)
{
    struct sockaddr_in address;
    socklen_t length = (socklen_t)sizeof(address);
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return -1;
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

static int connect_with_retry(uint16_t port)
{
    struct sockaddr_in address;
    struct timespec delay = {0, 10000000L};
    size_t attempt;
    (void)memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    for (attempt = 0U; attempt < 200U; ++attempt) {
        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd >= 0 && connect(socket_fd,
                (const struct sockaddr *)&address, sizeof(address)) == 0)
            return socket_fd;
        if (socket_fd >= 0) (void)close(socket_fd);
        (void)nanosleep(&delay, NULL);
    }
    return -1;
}

static bool exchange(uint16_t port, const char *request, char *response,
    size_t capacity)
{
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
        if (count == 0) break;
        CHECK(count > 0);
        used += (size_t)count;
    }
    response[used] = '\0';
    (void)close(socket_fd);
    return true;
}

static bool asset_path(const char *html, const char *suffix, char *output,
    size_t capacity)
{
    const char *start = html;
    size_t suffix_length = strlen(suffix);
    while ((start = strstr(start, "/assets/")) != NULL) {
        const char *end = start;
        size_t length;
        while (*end != '\0' && *end != '"' && *end != '\'' && *end != '<' &&
            *end != '>') ++end;
        length = (size_t)(end - start);
        if (length >= suffix_length &&
            memcmp(end - suffix_length, suffix, suffix_length) == 0 &&
            length < capacity) {
            (void)memcpy(output, start, length);
            output[length] = '\0';
            return true;
        }
        start = end;
    }
    return false;
}

static bool test_http_contract(TrainlogDatabase *database)
{
    char response[32768];
    char large_request[12000];
    char request[512];
    char javascript_path[256];
    char stylesheet_path[256];
    uint16_t port;
    pid_t child;
    int status;
    struct sigaction action;
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
        _exit(trainlog_web_server_run(database, port, &child_stop,
            diagnostic, sizeof(diagnostic)) == 0 ? 0 : 1);
    }
    CHECK(exchange(port,
        "GET /api/v1/health HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
        response, sizeof(response)));
    CHECK(strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "Content-Type: application/json; charset=utf-8") != NULL);
    CHECK(strstr(response, "X-Content-Type-Options: nosniff") != NULL);
    CHECK(strstr(response, "Content-Security-Policy: default-src 'none'") != NULL);
    CHECK(strstr(response, "Access-Control-Allow-Origin") == NULL);
    CHECK(strstr(response,
        "{\"api_version\":1,\"status\":\"ok\",\"product\":\"trainlog\","
        "\"version\":\"0.1.2\"}") != NULL);
    CHECK(exchange(port,
        "GET /api/v1/dashboard HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
        response, sizeof(response)) && strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "Content-Type: application/json; charset=utf-8") != NULL);
    CHECK(strstr(response, "\"api_version\":1") != NULL);
    CHECK(strstr(response, "\"no_persisted_executable_plan\"") != NULL);
    CHECK(strstr(response, "\"no_cardio_data_source\"") != NULL);
    CHECK(strstr(response, "\"window_days\":90") != NULL);
    CHECK(strstr(response, "\"partial\":false") != NULL);
    CHECK(exchange(port,
        "POST /api/v1/dashboard HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 0\r\n\r\n",
        response, sizeof(response)) && strstr(response, "HTTP/1.1 405") != NULL &&
        strstr(response, "Allow: GET") != NULL);
    CHECK(exchange(port, "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
        response, sizeof(response)) && strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "Content-Type: text/html; charset=utf-8") != NULL);
    CHECK(strstr(response, "Cache-Control: no-cache") != NULL);
    CHECK(strstr(response, "Content-Security-Policy: default-src 'self'") != NULL);
    CHECK(strstr(response, "unsafe-inline") == NULL);
    CHECK(strstr(response, "unsafe-eval") == NULL);
    CHECK(strstr(response, "<title>Trainlog</title>") != NULL);
    CHECK(asset_path(response, ".js", javascript_path, sizeof(javascript_path)));
    CHECK(asset_path(response, ".css", stylesheet_path, sizeof(stylesheet_path)));
    (void)snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", javascript_path);
    CHECK(exchange(port, request, response, sizeof(response)) &&
        strstr(response, "HTTP/1.1 200") != NULL);
    CHECK(strstr(response, "Content-Type: text/javascript; charset=utf-8") != NULL);
    CHECK(strstr(response,
        "Cache-Control: public, max-age=31536000, immutable") != NULL);
    CHECK(strstr(response, "ETag: \"sha256-") != NULL);
    (void)snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", stylesheet_path);
    CHECK(exchange(port, request, response, sizeof(response)) &&
        strstr(response, "Content-Type: text/css; charset=utf-8") != NULL);
    CHECK(exchange(port,
        "GET /analyse HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
        response, sizeof(response)) && strstr(response, "<title>Trainlog</title>") != NULL);
    CHECK(exchange(port,
        "GET /api/v1/unknown HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
        response, sizeof(response)) && strstr(response, "HTTP/1.1 404") != NULL &&
        strstr(response, "<title>Trainlog</title>") == NULL);
    CHECK(exchange(port,
        "GET /assets/unknown.js HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
        response, sizeof(response)) && strstr(response, "HTTP/1.1 404") != NULL);
    CHECK(exchange(port,
        "GET /assets/../index.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
        response, sizeof(response)) && strstr(response, "HTTP/1.1 404") != NULL);
    (void)snprintf(request, sizeof(request),
        "HEAD %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", javascript_path);
    CHECK(exchange(port, request, response, sizeof(response)) &&
        strstr(response, "HTTP/1.1 200") != NULL &&
        strstr(response, "Content-Type: text/javascript; charset=utf-8") != NULL);
    CHECK(exchange(port,
        "POST /api/v1/health HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 0\r\n\r\n",
        response, sizeof(response)) && strstr(response, "HTTP/1.1 405") != NULL);
    CHECK(strstr(response, "Allow: GET") != NULL);
    CHECK(exchange(port,
        "GET /api/v1/health HTTP/1.1\r\nHost: foreign.example\r\n\r\n",
        response, sizeof(response)) && strstr(response, "HTTP/1.1 400") != NULL);
    (void)snprintf(large_request, sizeof(large_request),
        "POST /api/v1/health HTTP/1.1\r\nHost: 127.0.0.1\r\n"
        "Content-Length: 5000\r\n\r\n%05000d", 0);
    CHECK(exchange(port, large_request, response, sizeof(response)) &&
        strstr(response, "HTTP/1.1 413") != NULL);
    (void)memset(large_request, 'a', sizeof(large_request));
    (void)snprintf(large_request, 128U,
        "GET /api/v1/health HTTP/1.1\r\nHost: 127.0.0.1\r\nX-Large: ");
    {
        size_t prefix = strlen(large_request);
        (void)memset(large_request + prefix, 'a', 8500U);
        (void)memcpy(large_request + prefix + 8500U, "\r\n\r\n", 5U);
    }
    CHECK(exchange(port, large_request, response, sizeof(response)) &&
        (strstr(response, "HTTP/1.1 431") != NULL ||
         strstr(response, "HTTP/1.1 400") != NULL));
    CHECK(kill(child, SIGTERM) == 0);
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return true;
}

static bool test_port_in_use(TrainlogDatabase *database)
{
    volatile sig_atomic_t stop = 0;
    char diagnostic[256];
    uint16_t port;
    int occupied = reserve_port(&port, 1);
    CHECK(occupied >= 0);
    CHECK(trainlog_web_server_run(database, port, &stop, diagnostic,
        sizeof(diagnostic)) != 0);
    CHECK(strstr(diagnostic, "127.0.0.1") != NULL);
    (void)close(occupied);
    return true;
}

int main(void)
{
    TrainlogDatabase *database = NULL;
    bool passed;
    if (trainlog_database_open(":memory:", &database) != TRAINLOG_STATUS_OK)
        return 1;
    if (!trainlog_web_assets_available()) {
        volatile sig_atomic_t stop = 0;
        char diagnostic[256];
        passed = trainlog_web_server_run(database, 8080U, &stop, diagnostic,
            sizeof(diagnostic)) != 0 && strstr(diagnostic, "frontend") != NULL;
    } else {
        passed = test_http_contract(database) && test_port_in_use(database);
    }
    trainlog_database_close(database);
    return passed ? 0 : 1;
}
