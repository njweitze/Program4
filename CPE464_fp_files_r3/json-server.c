#include "smartalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "json-server.h"

#define BUFFER_SIZE 4096
#define BACKLOG 10 // Define backlog for listen()

// Global variable for the server socket
int server_socket;

// Function prototypes
void handle_request(int client_fd);
void *thread_handler(void *arg);
void graceful_exit(int signum);

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    // Handle SIGINT to enable clean shutdown
    signal(SIGINT, graceful_exit);

    // Create a TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;

    // Check if an IP address was provided as a command line parameter
    if (argc > 1) {
        if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
            exit(EXIT_FAILURE);
        }
    } else {
        server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
    }

    server_addr.sin_port = 0; // Dynamically allocate port

    // Bind the socket to the address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Retrieve and display the dynamically assigned port
    if (getsockname(server_socket, (struct sockaddr *)&server_addr, &addr_len) < 0) {
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("HTTP server is using TCP port %d\n", ntohs(server_addr.sin_port));
    fflush(stdout);
    printf("HTTPS server is using TCP port -1\n");
    fflush(stdout);

    // Start listening for incoming connections
    if (listen(server_socket, BACKLOG) < 0) {
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is ready to accept connections...\n");
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Accept a new client connection
        int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            continue; // Ignore this connection and keep running
        }

        // Handle the client connection in a new thread
        pthread_t thread_id;
        int *client_fd_ptr = malloc(sizeof(int));
        if (!client_fd_ptr) {
            close(client_fd);
            continue;
        }

        *client_fd_ptr = client_fd;
        if (pthread_create(&thread_id, NULL, thread_handler, client_fd_ptr) != 0) {
            free(client_fd_ptr);
            close(client_fd);
        }

        // Detach the thread to avoid memory leaks
        pthread_detach(thread_id);
    }

    return 0;
}

void *thread_handler(void *arg) {
    int client_fd = *(int *)arg;
    free(arg); // Free the dynamically allocated memory for the client FD

    handle_request(client_fd);

    close(client_fd); // Close the client connection
    return NULL;
}

void handle_request(int socket_fd) {
    char request_buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_received = 0;
    int total_bytes_received = 0;

    // Read the incoming request
    while ((bytes_received = recv(socket_fd, request_buffer + total_bytes_received, BUFFER_SIZE - total_bytes_received - 1, 0)) > 0) {
        total_bytes_received += bytes_received;
        request_buffer[total_bytes_received] = '\0'; // Null-terminate the string

        // Check if the full HTTP request has been received
        if (strstr(request_buffer, "\r\n\r\n") != NULL) {
            break;
        }
    }

    if (total_bytes_received == 0) {
        return; // No valid data received
    }

    // Hardcoded JSON responses
    const char *implemented_features_json =
        "[\n"
        "  {\"feature\": \"about\", \"URL\": \"/json/about\"},\n"
        "  {\"feature\": \"quit\", \"URL\": \"/foo/DIE\"}\n"
        "]";
    const char *about_info_json =
        "{\n"
        "  \"author\": \"Noah Weitzel\",\n"
        "  \"email\": \"njweitze@calpoly.edu\",\n"
        "  \"major\": \"CPE\"\n"
        "}";
    const char *quit_response_json =
        "{\n"
        "  \"result\": \"success\"\n"
        "}";

    char http_response[BUFFER_SIZE] = {0};

    // Determine the requested endpoint
    if (strncmp(request_buffer, "GET /json/implemented.json", 26) == 0) {
        snprintf(http_response, sizeof(http_response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n"
                 "%s",
                 strlen(implemented_features_json), implemented_features_json);
    } else if (strncmp(request_buffer, "GET /json/about", 15) == 0) {
        snprintf(http_response, sizeof(http_response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n"
                 "%s",
                 strlen(about_info_json), about_info_json);
    } else if (strncmp(request_buffer, "GET /foo/DIE", 12) == 0) {
        snprintf(http_response, sizeof(http_response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n"
                 "%s",
                 strlen(quit_response_json), quit_response_json);
        write(socket_fd, http_response, strlen(http_response));
        kill(getpid(), SIGINT); // Trigger server shutdown
        return;
    } else {
        snprintf(http_response, sizeof(http_response),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 13\r\n"
                 "\r\n"
                 "404 Not Found");
    }

    // Send the response to the client
    write(socket_fd, http_response, strlen(http_response));
}

void graceful_exit(int signum) {
    printf("\nServer exiting cleanly.\n");
    close(server_socket); // Close the server socket
    exit(0);
}
