
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 50

// Structure for passing data to thread
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_data_t;

// Global variable for server control
volatile int server_running = 1;

// Function to decode URL encoding
void url_decode(char *dst, const char *src) {
    char *p = dst;
    char code[3] = {0};
    
    while (*src) {
        if (*src == '%' && strlen(src) >= 3) {
            memcpy(code, src + 1, 2);
            code[2] = '\0';
            *p++ = (char)strtol(code, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *p++ = ' ';
            src++;
        } else {
            *p++ = *src++;
        }
    }
    *p = '\0';
}

// Function to parse POST form data
void parse_post_data(char *data, char *name, size_t name_size, 
                     char *email, size_t email_size, 
                     char *message, size_t message_size) {
    char *token, *key, *value;
    char *saveptr1, *saveptr2;
    char temp_data[BUFFER_SIZE];
    
    // Clear output buffers
    memset(name, 0, name_size);
    memset(email, 0, email_size);
    memset(message, 0, message_size);
    
    if (!data || strlen(data) == 0) {
        return;
    }
    
    strncpy(temp_data, data, sizeof(temp_data) - 1);
    temp_data[sizeof(temp_data) - 1] = '\0';
    
    // Parse key=value pairs separated by &
    token = strtok_r(temp_data, "&", &saveptr1);
    while (token != NULL) {
        key = strtok_r(token, "=", &saveptr2);
        value = strtok_r(NULL, "=", &saveptr2);
        
        if (key && value) {
            if (strcmp(key, "name") == 0) {
                url_decode(name, value);
            } else if (strcmp(key, "email") == 0) {
                url_decode(email, value);
            } else if (strcmp(key, "message") == 0) {
                url_decode(message, value);
            }
        }
        token = strtok_r(NULL, "&", &saveptr1);
    }
}

// Function to handle GET requests
void handle_get_request(int client_socket, char *path) {
    char response[BUFFER_SIZE * 2];
    const char *html_content;
    int content_length;
    
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        // Main page with form
        html_content = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "    <title>HTTP Server in C</title>\n"
            "    <meta charset='UTF-8'>\n"
            "    <style>\n"
            "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
            "        .container { max-width: 600px; margin: 0 auto; }\n"
            "        input, textarea { width: 100%; padding: 10px; margin: 5px 0; box-sizing: border-box; }\n"
            "        button { background: #4CAF50; color: white; padding: 12px 20px; border: none; cursor: pointer; }\n"
            "        button:hover { background: #45a049; }\n"
            "    </style>\n"
            "</head>\n"
            "<body>\n"
            "    <div class='container'>\n"
            "        <h1>Welcome to HTTP Server written in C!</h1>\n"
            "        <p>This server handles GET and POST requests with multithreading.</p>\n"
            "        \n"
            "        <h2>Contact Form</h2>\n"
            "        <form method='POST' action='/submit'>\n"
            "            <label>Name:</label><br>\n"
            "            <input type='text' name='name' required><br><br>\n"
            "            \n"
            "            <label>Email:</label><br>\n"
            "            <input type='email' name='email' required><br><br>\n"
            "            \n"
            "            <label>Message:</label><br>\n"
            "            <textarea name='message' rows='4' required></textarea><br><br>\n"
            "            \n"
            "            <button type='submit'>Send</button>\n"
            "        </form>\n"
            "        \n"
            "        <hr>\n"
            "        <h3>Available endpoints:</h3>\n"
            "        <ul>\n"
            "            <li><a href='/'>/ - Home page</a></li>\n"
            "            <li><a href='/about'>/about - Server information</a></li>\n"
            "            <li><a href='/status'>/status - Server status</a></li>\n"
            "            <li>POST /submit - Form processing</li>\n"
            "        </ul>\n"
            "    </div>\n"
            "</body>\n"
            "</html>";
    } else if (strcmp(path, "/about") == 0) {
        html_content = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>About Server</title><meta charset='UTF-8'></head>\n"
            "<body>\n"
            "    <h1>About HTTP Server</h1>\n"
            "    <p>This server was written in C and supports:</p>\n"
            "    <ul>\n"
            "        <li>GET and POST requests</li>\n"
            "        <li>Multithreading (up to 50 concurrent clients)</li>\n"
            "        <li>HTML form parsing</li>\n"
            "        <li>URL decoding</li>\n"
            "    </ul>\n"
            "    <a href='/'>Back to home page</a>\n"
            "</body>\n"
            "</html>";
    } else if (strcmp(path, "/status") == 0) {
        html_content = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>Server Status</title><meta charset='UTF-8'></head>\n"
            "<body>\n"
            "    <h1>Server Status</h1>\n"
            "    <p>Server is running correctly!</p>\n"
            "    <p>Port: 8080</p>\n"
            "    <p>Maximum clients: 50</p>\n"
            "    <a href='/'>Back to home page</a>\n"
            "</body>\n"
            "</html>";
    } else {
        // 404 page
        html_content = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>404 - Not Found</title><meta charset='UTF-8'></head>\n"
            "<body>\n"
            "    <h1>404 - Page Not Found</h1>\n"
            "    <p>The requested page does not exist.</p>\n"
            "    <a href='/'>Back to home page</a>\n"
            "</body>\n"
            "</html>";
        
        content_length = strlen(html_content);
        snprintf(response, sizeof(response),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", content_length, html_content);
        
        send(client_socket, response, strlen(response), 0);
        return;
    }
    
    // Send HTTP 200 OK response
    content_length = strlen(html_content);
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s", content_length, html_content);
    
    send(client_socket, response, strlen(response), 0);
}

// Function to handle POST requests
void handle_post_request(int client_socket, char *body) {
    char name[256];
    char email[256];
    char message[1024];
    char response[BUFFER_SIZE * 2];
    char html_content[BUFFER_SIZE];
    int content_length;
    
    // Parse POST data
    parse_post_data(body, name, sizeof(name), email, sizeof(email), message, sizeof(message));
    
    // Create HTML response
    snprintf(html_content, sizeof(html_content),
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>Form Submitted</title>\n"
        "    <meta charset='UTF-8'>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
        "        .container { max-width: 600px; margin: 0 auto; }\n"
        "        .success { background: #dff0d8; padding: 15px; border: 1px solid #d6e9c6; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class='container'>\n"
        "        <div class='success'>\n"
        "            <h1>Form submitted successfully!</h1>\n"
        "            <h3>Received data:</h3>\n"
        "            <p><strong>Name:</strong> %s</p>\n"
        "            <p><strong>Email:</strong> %s</p>\n"
        "            <p><strong>Message:</strong> %s</p>\n"
        "        </div>\n"
        "        <br>\n"
        "        <a href='/'>Back to home page</a>\n"
        "    </div>\n"
        "</body>\n"
        "</html>", name, email, message);
    
    // Send HTTP response
    content_length = strlen(html_content);
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s", content_length, html_content);
    
    send(client_socket, response, strlen(response), 0);
    
    // Log on server
    printf("[POST] Data received - Name: %s, Email: %s, Message: %.50s%s\n", 
           name, email, message, strlen(message) > 50 ? "..." : "");
}

// Function to parse HTTP request
void parse_http_request(char *request, char *method, size_t method_size, 
                       char *path, size_t path_size, char *body, size_t body_size) {
    char *line, *saveptr;
    char temp_request[BUFFER_SIZE];
    
    // Initialize output buffers
    memset(method, 0, method_size);
    memset(path, 0, path_size);
    memset(body, 0, body_size);
    
    if (!request || strlen(request) == 0) {
        return;
    }
    
    strncpy(temp_request, request, sizeof(temp_request) - 1);
    temp_request[sizeof(temp_request) - 1] = '\0';
    
    // First line contains method and path
    line = strtok_r(temp_request, "\r\n", &saveptr);
    if (line) {
        sscanf(line, "%15s %255s", method, path);  // Limit sizes to prevent overflow
    }
    
    // Find end of headers (empty line)
    char *body_start = strstr(request, "\r\n\r\n");
    if (body_start) {
        body_start += 4; // Move past \r\n\r\n
        strncpy(body, body_start, body_size - 1);
        body[body_size - 1] = '\0';
    }
}

// Main client handling function (executed in separate thread)
void* handle_client(void* arg) {
    client_data_t *client_data = (client_data_t*)arg;
    int client_socket = client_data->client_socket;
    char buffer[BUFFER_SIZE] = {0};
    char method[16] = {0};
    char path[256] = {0};
    char body[BUFFER_SIZE] = {0};
    
    printf("[INFO] New client connected from %s\n", 
           inet_ntoa(client_data->client_addr.sin_addr));
    
    // Read HTTP request
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        printf("[ERROR] Error reading data from client\n");
        close(client_socket);
        free(client_data);
        return NULL;
    }
    
    buffer[bytes_read] = '\0';
    printf("[REQUEST] Received request:\n%s\n", buffer);
    
    // Parse HTTP request
    parse_http_request(buffer, method, sizeof(method), path, sizeof(path), body, sizeof(body));
    
    printf("[INFO] Method: %s, Path: %s\n", method, path);
    
    // Handle different HTTP methods
    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_socket, path);
    } else if (strcmp(method, "POST") == 0) {
        handle_post_request(client_socket, body);
    } else {
        // Unsupported method
        const char *error_response = 
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 47\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>405 Method Not Allowed</h1></body></html>";
        
        send(client_socket, error_response, strlen(error_response), 0);
    }
    
    // Close connection with client
    close(client_socket);
    free(client_data);
    printf("[INFO] Client connection closed\n");
    
    return NULL;
}

// Signal handler for SIGINT (Ctrl+C)
void signal_handler(int sig) {
    printf("\n[INFO] Received signal %d, shutting down server...\n", sig);
    server_running = 0;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;
    
    printf("=== HTTP Server in C ===\n");
    printf("Port: %d\n", PORT);
    printf("Maximum clients: %d\n\n", MAX_CLIENTS);
    
    // Set signal handler
    signal(SIGINT, signal_handler);
    
    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options (allow address reuse)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error setting socket options");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket to address
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Start listening
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Error listening");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("[INFO] Server listening on port %d...\n", PORT);
    printf("[INFO] Open browser and go to http://localhost:%d\n", PORT);
    printf("[INFO] Press Ctrl+C to stop server\n\n");
    
    // Main server loop
    while (server_running) {
        // Accept connection from client
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (server_running && errno != EINTR) {
                perror("Error accepting connection");
            }
            continue;
        }
        
        // Create client data structure
        client_data_t *client_data = malloc(sizeof(client_data_t));
        if (!client_data) {
            printf("[ERROR] Memory allocation error\n");
            close(client_socket);
            continue;
        }
        
        client_data->client_socket = client_socket;
        client_data->client_addr = client_addr;
        
        // Create new thread for client handling
        if (pthread_create(&thread_id, NULL, handle_client, client_data) != 0) {
            perror("Error creating thread");
            close(client_socket);
            free(client_data);
            continue;
        }
        
        // Detach thread (automatic cleanup after completion)
        pthread_detach(thread_id);
    }
    
    // Close server
    close(server_socket);
    printf("[INFO] Server has been shut down\n");
    
    return 0;
}