#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 8000
#define MAX_CLIENTS 10

typedef struct {
    int client_socket;
} client_info;

volatile int server_running = 1;

void handle_get(int client_socket, char* path) {
    char response[BUFFER_SIZE];
    char* html_content;

    if (strcmp(path, "/") == 0) {
        html_content = 
            "<!DOCTYPE html>"
            "<html><head><title>HTTP Server</title></head>"
            "<body>"
            "<h1>HTTP Server</h1>"
            "<h2>Form</h2>"
            "<form method='POST' action='/submit'>"
            "Name: <input type='text' name='name'><br><br>"
            "Message: <textarea name='message'></textarea><br><br>"
            "<input type='submit' value='Send'>"
            "</form>"
            "<hr>"
            "</body></html>";
    } else {
        html_content = 
            "<!DOCTYPE html>"
            "<html><head><title>404</title></head>"
            "<body>"
            "<h1>404 - Page Not Found</h1>"
            "<p><a href='/'>Back</a></p>"
            "</body></html>";
    }

    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s", strlen(html_content), html_content);

    send(client_socket, response, strlen(response), 0);
}

void handle_post(int client_socket, char* body) {
    char response[BUFFER_SIZE];
    char name[100] = "";
    char message[500] = "";

    printf("[DEBUG] Raw POST body received:\n%s\n", body);

    char* name_start = strstr(body, "name=");
    if (name_start) {
        name_start += 5;
        char* name_end = strstr(name_start, "&");
        if (name_end) {
            int len = name_end - name_start;
            if (len < 99) {
                strncpy(name, name_start, len);
                name[len] = '\0';
            }
        }
    }

    char* msg_start = strstr(body, "message=");
    if (msg_start) {
        msg_start += 8;
        strncpy(message, msg_start, 499);
        message[499] = '\0';
    }

    char html_content[BUFFER_SIZE/2];
    snprintf(html_content, sizeof(html_content),
        "<!DOCTYPE html>"
        "<html><head><title>Form Submitted</title></head>"
        "<body>"
        "<h1>Data Received!</h1>"
        "<p><strong>Name:</strong> %s</p>"
        "<p><strong>Message:</strong> %s</p>"
        "<p><a href='/'>Back</a></p>"
        "</body></html>", name, message);

    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s", strlen(html_content), html_content);

    send(client_socket, response, strlen(response), 0);

    printf("[POST] Received: name=%s, message=%s\n", name, message);
}

void* handle_client(void* arg) {
    client_info* info = (client_info*)arg;
    int client_socket = info->client_socket;
    char buffer[BUFFER_SIZE];

    printf("[INFO] New client connected\n");

    int bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        free(info);
        return NULL;
    }
    buffer[bytes_read] = '\0';

    printf("[REQUEST] HTTP request received:\n%s\n", buffer);

    char method[10];
    char path[100];
    sscanf(buffer, "%s %s", method, path);

    printf("[INFO] Method: %s, Path: %s\n", method, path);

    if (strcmp(path, "/favicon.ico") == 0) {
        close(client_socket);
        free(info);
        return NULL;
    }

    if (strcmp(method, "GET") == 0) {
        handle_get(client_socket, path);
    } else if (strcmp(method, "POST") == 0) {
        char* body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            handle_post(client_socket, body);
        }
    } else {
        char* error_response = 
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<h1>405 - Method Not Allowed</h1>";
        send(client_socket, error_response, strlen(error_response), 0);
    }

    close(client_socket);
    free(info);
    printf("[INFO] Client connection closed\n");

    return NULL;
}

void signal_handler(int sig) {
    printf("\n[INFO] Stopping server...\n");
    server_running = 0;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    printf("=== Simple HTTP Server in C ===\n");
    printf("Port: %d\n", PORT);
    printf("Max clients: %d\n\n", MAX_CLIENTS);

    signal(SIGINT, signal_handler);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation error");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind error");
        close(server_socket);
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen error");
        close(server_socket);
        exit(1);
    }

    printf("[INFO] Server is listening on port %d\n", PORT);
    printf("[INFO] Open in browser: http://localhost:%d\n", PORT);
    printf("[INFO] Available paths:\n");
    printf("       - http://localhost:%d/ (home page)\n", PORT);
    printf("[INFO] Press Ctrl+C to stop\n\n");

    while (server_running) {
        fd_set readfds;
        struct timeval timeout;

        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(server_socket + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            perror("Select error");
            break;
        }

        if (!server_running) break;

        if (activity == 0) continue;

        if (FD_ISSET(server_socket, &readfds)) {
            client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);

            if (client_socket < 0) {
                if (server_running) {
                    perror("Accept error");
                }
                continue;
            }

            client_info* info = malloc(sizeof(client_info));
            if (!info) {
                printf("[ERROR] Memory allocation failed\n");
                close(client_socket);
                continue;
            }
            info->client_socket = client_socket;

            if (pthread_create(&thread_id, NULL, handle_client, info) != 0) {
                perror("Thread creation error");
                close(client_socket);
                free(info);
                continue;
            }

            pthread_detach(thread_id);
        }
    }

    close(server_socket);
    printf("[INFO] Server stopped\n");

    return 0;
}
