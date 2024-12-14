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

// macros
#define BUFFER_SIZE 4096
#define BACKLOG 10

// functions
void handle_request(int client_fd);
void *thread_handler(void *arg);
void exit_server(int signum);

// global variable
extern int server_socket; // Used for signal handler

#endif
