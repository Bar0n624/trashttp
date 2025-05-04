#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int load_config(const char *filename, server_config_t *config) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open config file");
        return -1;
    }

    config->port = 8080;
    config->max_clients = 45;
    config->num_workers = 10;
    config->buffer_size = 1024;
    strcpy(config->ssl_cert, "./ssl/cert.pem");
    strcpy(config->ssl_key, "./ssl/key.pem");
    strcpy(config->base_dir, "./html");
    config->enable_work_stealing = 0;
    strcpy(config->scheduler, "ROUND_ROBIN");
    strcpy(config->log_file, "server.log");

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (!key || !value) continue;

        if (strcmp(key, "port") == 0)
            config->port = atoi(value);
        else if (strcmp(key, "max_clients") == 0)
            config->max_clients = atoi(value);
        else if (strcmp(key, "num_workers") == 0)
            config->num_workers = atoi(value);
        else if (strcmp(key, "buffer_size") == 0)
            config->buffer_size = atoi(value);
        else if (strcmp(key, "ssl_cert") == 0)
            strncpy(config->ssl_cert, value, sizeof(config->ssl_cert) - 1);
        else if (strcmp(key, "ssl_key") == 0)
            strncpy(config->ssl_key, value, sizeof(config->ssl_key) - 1);
        else if (strcmp(key, "base_dir") == 0)
            strncpy(config->base_dir, value, sizeof(config->base_dir) - 1);
        else if (strcmp(key, "enable_work_stealing") == 0)
            config->enable_work_stealing = atoi(value);
        else if (strcmp(key, "scheduler") == 0)
            strncpy(config->scheduler, value, sizeof(config->scheduler) - 1);
        else if (strcmp(key, "log_file") == 0)
            strncpy(config->log_file, value, sizeof(config->log_file) - 1);
        else
            fprintf(stderr, "Unknown config key: %s\n", key);
    }

    fclose(file);
    return 0;
}