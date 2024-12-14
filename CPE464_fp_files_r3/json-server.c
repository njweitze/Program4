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

// global
int server_socket;

int main(int argc, char *argv[]) {
    struct sockaddr_in6 server_addr;
    socklen_t addr_len = sizeof(server_addr);

    // handle SIGINT for clean shutdown
    if (signal(SIGINT, exit_server) == SIG_ERR) {
        perror("Failed to set signal handler for SIGINT");
        exit(EXIT_FAILURE);
    }

    // create TCP socket
    server_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;

    // check if IP provided as a command-line parameter
    if (argc > 1) {
        if (inet_pton(AF_INET6, argv[1], &server_addr.sin6_addr) <= 0) {
            // if IPv6 conversion fails, try IPv4
            struct in_addr IP_v4;
            if (inet_pton(AF_INET, argv[1], &IP_v4) > 0) {
                char converted_IP_v6[INET6_ADDRSTRLEN] = "::ffff:";
                strncat(converted_IP_v6, argv[1], sizeof(converted_IP_v6) - strlen(converted_IP_v6) - 1);
                inet_pton(AF_INET6, converted_IP_v6, &server_addr.sin6_addr);
            } else {
                perror("Invalid IP address");
                exit(EXIT_FAILURE);
            }
        }
    } else {
        server_addr.sin6_addr = in6addr_any; // bind to all interfaces
    }



    server_addr.sin6_port = 0; // dynamically allocate port

    // bind socket to the address w/ err check
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // retrieve dynamically assigned port
    if (getsockname(server_socket, (struct sockaddr *)&server_addr, &addr_len) < 0) {
        perror("Failed to get socket name");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // print port
    printf("HTTP server is using TCP port %d\n", ntohs(server_addr.sin6_port));
    fflush(stdout);
    printf("HTTPS server is using TCP port -1\n");
    fflush(stdout);

    // listen for connections w/ err check
    if (listen(server_socket, BACKLOG) < 0) {
        perror("Failed to listen on socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is ready to accept connections...\n");
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // accept client connection w/ err check
        int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Failed to accept client connection");
            continue; // Ignore this connection and keep running
        }

        // handle client connection in a new thread w/ err check
        pthread_t thread_id;
        int *client_fd_ptr = malloc(sizeof(int));
        if (!client_fd_ptr) {
            perror("Failed to allocate memory for client FD");
            close(client_fd);
            continue;
        }

        *client_fd_ptr = client_fd;
        if (pthread_create(&thread_id, NULL, thread_handler, client_fd_ptr) != 0) {
            perror("Failed to create thread for client connection");
            free(client_fd_ptr);
            close(client_fd);
        }

        // detach the thread to avoid memory leaks w/ err check
        if (pthread_detach(thread_id) != 0) {
            perror("Failed to detach thread");
        }
    }

    return 0;
}

void *thread_handler(void *arg) {
    int client_fd = *(int *)arg;
    free(arg); // free the memory for the client

    handle_request(client_fd);

    close(client_fd); // close client connection
    return NULL;
}

void handle_request(int socket_fd) {
    char request_buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_received = 0;
    int total_bytes_received = 0;

    // read incoming request
    while ((bytes_received = recv(socket_fd, request_buffer + total_bytes_received, BUFFER_SIZE - total_bytes_received - 1, 0)) > 0) {
        total_bytes_received += bytes_received;
        request_buffer[total_bytes_received] = '\0'; // null-terminate the string

        // check if the full HTTP request has been received
        if (strstr(request_buffer, "\r\n\r\n") != NULL) {
            break;
        }
    }

    if (total_bytes_received == 0) {
        perror("Failed to receive valid data from client");
        return; // no valid data received
    }

    // hardcoded JSON responses
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

    // determine the requested endpoint
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
        if (write(socket_fd, http_response, strlen(http_response)) < 0) {
            perror("Failed to send quit response");
        }
        kill(getpid(), SIGINT); // trigger server shutdown
        return;
    } else {
        snprintf(http_response, sizeof(http_response),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 13\r\n"
                 "\r\n"
                 "404 Not Found");
    }

    // send response to the client
    if (write(socket_fd, http_response, strlen(http_response)) < 0) {
        perror("Failed to send HTTP response");
    }
}

// exit server
void exit_server(int signum) {
    printf("\nServer exiting cleanly.\n");
    if (close(server_socket) < 0) {
        perror("Failed to close server socket");
    }
    exit(0);
}
