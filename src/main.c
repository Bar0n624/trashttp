#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "server.h"

#define CONFIG_FILE "server.conf"

static server_t server;

void handle_signal(int sig) {
    printf("Shutting down server...\n");
    server_cleanup(&server);
    exit(0);
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (server_init(&server, CONFIG_FILE) != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return EXIT_FAILURE;
    }

    if (server_start(&server) != 0) {
        fprintf(stderr, "Failed to start server\n");
        server_cleanup(&server);
        return EXIT_FAILURE;
    }

    server_cleanup(&server);
    return 0;
}