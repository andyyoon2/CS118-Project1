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

#include "compat.h"

using namespace std;

int main (int argc, char *argv[])
{
    // command line parsing
    if (argc != 1) {
        fprintf(stdout, "Usage: %s\n", argv[0]);
        return 1;
    }

    int sock_fd, new_fd;
    struct sigaction sa;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET_ADDRSTRLEN];
    
    sock_fd = create_server(SERVER_LISTEN_PORT);
    if (sock_fd < 0) {
        fprintf(stderr, "server: creation error\n");
        return 1;
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
            if (send(new_fd, "Hello, world!\n", 14, 0) == -1) {
                fprintf(stderr, "server: send");
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
    return 0;
}
