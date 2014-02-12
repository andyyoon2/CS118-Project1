/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "http-request.h"

using namespace std;

#define SERVER_LISTEN_PORT "14886"  // Port users will connect to
#define BACKLOG 20                  // Number of requests allowed in queue

void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// gets socket addr, only need to support IPv4
void *get_in_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
}

int main (int argc, char *argv[])
{
    // command line parsing
    if (argc != 1) {
        fprintf(stdout, "Usage: %s\n", argv[0]);
        return 1;
    }

    // getaddrinfo();
    // socket();
    // bind();
    // listen();
    // accept();
    int sock_fd, new_fd;
    struct addrinfo hints, *res, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    char s[INET_ADDRSTRLEN];
    int status;

    // Initialize address struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // we only need to support IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, SERVER_LISTEN_PORT, &hints, &res)) != 0) {
        fprintf(stderr, "server: getaddrinfo error %s\n", gai_strerror(status));
        return -1;
    }
    
    // Loop through results, bind to the first one we can
    for (p = res; p != NULL; p = p->ai_next) {
        // Create the socket
        sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock_fd < 0) {
            fprintf(stderr, "server: socket error\n");
            continue;
        }

        // Bind the socket
        if (bind(sock_fd, res->ai_addr, res->ai_addrlen) != 0) {
            close(sock_fd);
            fprintf(stderr, "server: bind error\n");
            continue;
        }

        break;
    }
    
    // Couldn't bind to anything
    if (p == NULL) {
        fprintf(stderr, "server: bind error\n");
        return -1;
    }

    // All done with res struct
    freeaddrinfo(res);

    // Start listening
    if (listen(sock_fd, BACKLOG) == -1) {
        fprintf(stderr, "server: listen error\n");
        return -1;
    }

    // Reap zombies
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        fprintf(stderr, "server: sigaction\n");
        return -1;
    }

    printf("server: waiting for connections...\n");

    // Main loop
    while (true) {
        sin_size = sizeof their_addr;
        new_fd = accept(sock_fd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            fprintf(stderr, "server: accept\n");
            return -1;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sock_fd); // done with the listener
            if (send(new_fd, "Hello, world!", 13, 0) == -1) {
                fprintf(stderr, "server: send");
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
    return 0;
}
