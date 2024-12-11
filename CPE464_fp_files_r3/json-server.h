#ifndef JSON_SERVER_H
#define JSON_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

// Macros
#define BUFFER_SIZE 4096
#define BACKLOG 10

// Function prototypes
void handle_request(int client_fd);
void *thread_handler(void *arg);
void graceful_exit(int signum);

// Global variables
extern int server_socket; // Used for cleanup in the signal handler

#endif // JSON_SERVER_H
