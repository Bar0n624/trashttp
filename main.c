#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <regex.h>

#define PORT 8080
#define NUM_WORKERS 10
#define MAX_CLIENTS 45
#define BUFFER_SIZE 1024



typedef struct server {
    int server_fd;
    struct sockaddr_in address;
} SERVER;


void *handle_client(void *arg) {
    int client_fd = *(int *)arg;

    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if (bytes_read > 0) {
        regex_t regex;
        regcomp(&regex, "^GET /([^ ]*) HTTP/1.1", REG_EXTENDED);
        regmatch_t match[2];

        if (regexec(&regex, buffer, 2, match, 0) == 0) {
            char *filename = strndup(buffer + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
            printf("Requested file: %s\n", filename);

            FILE *file = fopen(filename, "r");
            if (file) {
                char file_buffer[BUFFER_SIZE];
                ssize_t bytes_read;
                while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
                    send(client_fd, file_buffer, bytes_read, 0);
                }
                fclose(file);
            } else {
                const char *not_found_response = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(client_fd, not_found_response, strlen(not_found_response), 0);
            }
            free(filename);
        } else {
            const char *bad_request_response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            send(client_fd, bad_request_response, strlen(bad_request_response), 0);
        }
    }



    free(buffer);
    close(client_fd);
    return NULL;
}

int main(void) {
    SERVER server;
    server.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.server_fd <= 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_port = htons(PORT);

    int res = bind(server.server_fd, (struct sockaddr *) &server.address, sizeof(server.address));

    if (res < 0) {
        perror("Could not bind socket to port");
        exit(EXIT_FAILURE);
    }

    res = listen(server.server_fd, 45);

    if (res < 0) {
        perror("Could not listen on socket");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
    while(1) {

        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        *client_fd = accept(server.server_fd, (struct sockaddr *) &client_addr, &addrlen);

        if (*client_fd < 0) {
            perror("Could not accept connection");
            continue;
        }

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *) client_fd);
        pthread_detach(thread_id);
    }

    close(server.server_fd);
    return 0;
}