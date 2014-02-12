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
#include "http-request.h"

using namespace std;

int main (int argc, char *argv[])
{
    // command line parsing
    if (argc != 1) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        return 1;
    }

    int sockfd, new_fd;
    struct sigaction sa;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET_ADDRSTRLEN];

    char buf[BUF_SIZE];
    int numbytes;

    // Test client connection
    sockfd = client_connect("www.google.com","80");
    HttpRequest req;
    req.SetHost("www.google.com");
    req.SetPort(80);
    req.SetMethod(HttpRequest::GET);
    req.SetPath("/");
    req.SetVersion("1.0");
    req.AddHeader("Accept-Language", "en-US");
    size_t req_len = req.GetTotalLength();
    char test_buf[req_len];
    req.FormatRequest(test_buf);
    if ((send(sockfd, test_buf, req_len, 0)) == -1) {
        fprintf(stderr, "client: send error\n");
        return 1;
    }

    if ((numbytes = recv(sockfd, buf, BUF_SIZE-1, 0)) == -1) {
        fprintf(stderr, "client: recv error\n");
        return 1;
    }
    buf[numbytes] = '\0';
    printf("client: received %s\n", buf);
    close(sockfd);

    // Make server
    sockfd = create_server(SERVER_LISTEN_PORT);
    if (sockfd < 0) {
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
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            fprintf(stderr, "server: accept\n");
            return -1;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // done with the listener
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
