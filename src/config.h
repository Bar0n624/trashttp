#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int port;
    int max_clients;
    int num_workers;
    int buffer_size;
    char ssl_cert[256];
    char ssl_key[256];
    char base_dir[256];
    int enable_work_stealing;
} server_config_t;

int load_config(const char *filename, server_config_t *config);

#endif
