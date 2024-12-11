#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

// Global variable for the server socket
int server_socket;

// Function prototypes
void handle_request(int client_fd);
void *thread_handler(void *arg);
void graceful_exit(int signum);

int main(int argc, char *argv[]) {
    struct sockaddr_in6 server_addr;
    socklen_t addr_len = sizeof(server_addr);

    // Handle SIGINT to enable clean shutdown
    signal(SIGINT, graceful_exit);

    // Check for optional command-line argument
    const char *bind_address = NULL;
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [<bind_address>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (argc == 2) {
        bind_address = argv[1];
    }

    // Create an IPv6 socket (dual-stack capable)
    server_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Allow both IPv4 and IPv6 connections on the socket
    int opt = 0; // 0 = allow both IPv4 and IPv6
    if (setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
        perror("Error setting socket options");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(0); // Dynamically allocate port

    if (bind_address) {
        // Convert the bind address to binary form
        if (inet_pton(AF_INET6, bind_address, &server_addr.sin6_addr) <= 0) {
            fprintf(stderr, "Invalid IPv6 address: %s\n", bind_address);
            close(server_socket);
            exit(EXIT_FAILURE);
        }
    } else {
        server_addr.sin6_addr = in6addr_any; // Bind to all IPv4 and IPv6 interfaces
    }

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

    printf("HTTP server is using TCP port %d\n", ntohs(server_addr.sin6_port));
    printf("HTTPS server is using TCP port -1\n");
    fflush(stdout);

    // Start listening for incoming connections
    if (listen(server_socket, 10) < 0) { // 10 is a placeholder for the backlog size
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is ready to accept connections...\n");

    while (1) {
        struct sockaddr_in6 client_addr;
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

        pthread_detach(thread_id); // Detach the thread to avoid memory leaks
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
    char buffer[1024];
    char response[1024];

    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        perror("Error reading from client");
        return;
    }

    buffer[bytes_read] = '\0'; // Null-terminate the request

    // Respond with a simple hardcoded message
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: 13\r\n"
             "\r\n"
             "Hello, World!");

    // Send the response
    write(client_fd, response, strlen(response));
}

void graceful_exit(int signum) {
    printf("\nServer exiting cleanly.\n");
    close(server_socket); // Close the server socket
    exit(0);
}
