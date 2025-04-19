#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include <signal.h>
#include <stdatomic.h>
#include "logger.h"

#define CONFIG_FILE "server.conf"

static server_t server;

volatile sig_atomic_t server_running = 1;

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        log_message("Server shutting down due to signal %d", signal);
        server_running = 0;
    }
}

int main() {

    if (server_init(&server, CONFIG_FILE) != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return EXIT_FAILURE;
    }

    if (server_start(&server) != 0) {
        fprintf(stderr, "Failed to start server\n");
        server_cleanup(&server);
        return EXIT_FAILURE;
    }
    return 0;
}