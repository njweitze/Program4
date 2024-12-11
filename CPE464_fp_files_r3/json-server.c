#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "json-server.h"
#include "smartalloc.h"

// Global variable for the server socket
int server_socket;

// Function prototypes
void handle_request(int client_fd);
void *thread_handler(void *arg);
void graceful_exit(int signum);

int main() {
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    // Handle SIGINT to enable clean shutdown
    signal(SIGINT, graceful_exit);

    // Create a TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
    server_addr.sin_port = 0;                 // Dynamically allocate port

    // Bind the socket to the address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Retrieve and display the dynamically assigned port
    if (getsockname(server_socket, (struct sockaddr *)&server_addr, &addr_len) < 0) {
        perror("Failed to retrieve socket name");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("HTTP server is using TCP port %d\n", ntohs(server_addr.sin_port));
    printf("HTTPS server is using TCP port -1\n");
    fflush(stdout);

    // Start listening for incoming connections
    if (listen(server_socket, BACKLOG) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is ready to accept connections...\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Accept a new client connection
        int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Failed to accept connection");
            continue; // Ignore this connection and keep running
        }

        // Handle the client connection in a new thread
        pthread_t thread_id;
        int *client_fd_ptr = malloc(sizeof(int));
        if (!client_fd_ptr) {
            perror("Failed to allocate memory for client socket");
            close(client_fd);
            continue;
        }

        *client_fd_ptr = client_fd;
        if (pthread_create(&thread_id, NULL, thread_handler, client_fd_ptr) != 0) {
            perror("Failed to create thread");
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

void handle_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        perror("Error reading from client");
        return;
    }

    buffer[bytes_read] = '\0'; // Null-terminate the request

    // Hardcoded JSON responses
    const char *implemented_json =
        "[\n"
        "  {\"feature\": \"about\", \"URL\": \"/json/about\"},\n"
        "  {\"feature\": \"quit\", \"URL\": \"/foo/DIE\"}\n"
        "]";
    const char *about_json =
        "{\n"
        "  \"author\": \"Noah Weitzel\",\n"
        "  \"email\": \"njweitze@calpoly.edu\",\n"
        "  \"major\": \"CPE\"\n"
        "}";
    const char *quit_json =
        "{\n"
        "  \"result\": \"success\"\n"
        "}";

    // Determine the requested endpoint
    if (strncmp(buffer, "GET /json/implemented.json", 26) == 0) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n"
                 "%s",
                 strlen(implemented_json), implemented_json);
    } else if (strncmp(buffer, "GET /json/about", 15) == 0) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n"
                 "%s",
                 strlen(about_json), about_json);
    } else if (strncmp(buffer, "GET /foo/DIE", 12) == 0) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n"
                 "%s",
                 strlen(quit_json), quit_json);
        write(client_fd, response, strlen(response));
        kill(getpid(), SIGINT); // Trigger server shutdown
        return;
    } else {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 13\r\n"
                 "\r\n"
                 "404 Not Found");
    }

    // Send the response to the client
    write(client_fd, response, strlen(response));
}

void graceful_exit(int signum) {
    printf("\nServer exiting cleanly.\n");
    close(server_socket); // Close the server socket
    exit(0);
}
