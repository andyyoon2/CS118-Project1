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
#include "http-response.h"

using namespace std;

/*
 * Gets HTTP request from client and forwards to remote server
 */
int process_request(int client_fd) {
    string client_buf;
    char buf[BUF_SIZE];

    // Receive request until "\r\n\r\n"
    while (memmem(client_buf.c_str(), client_buf.length(), 
           "\r\n\r\n", 4) == NULL) {
        if (recv(client_fd, buf, BUF_SIZE-1, 0) < 0) {
            fprintf(stderr,"server: recv error\n");
            return -1;
        }
        client_buf.append(buf);
    }

    // Parse the request, prepare our own request to send to remote
    HttpRequest client_req;
    try {
        client_req.ParseRequest(client_buf.c_str(),client_buf.length());
    }
    
    // Send error response back to client
    catch (ParseException exn) {
        fprintf(stderr, "Parse Exception: %s\n", exn.what());
        
        // FIXME
        // This initialization may not work in certain exns.
        // May have to assume version
        string res = client_req.GetVersion();

        const char *cmp1 = "Request is not GET";
        const char *cmp2 = "Only GET method is supported";
        // Set proper status code
        if (strcmp(exn.what(),cmp1) == 0 || strcmp(exn.what(),cmp2) == 0) {
            res += "501 Not Implemented\r\n\r\n";
        }
        else {
            res += "400 Bad Request\r\n\r\n";
        }

        // Send response
        if (send_all(client_fd, res.c_str(), res.length()) == -1) {
            fprintf(stderr, "server: send error\n");
            return -1;
        }
    }
    
    // Prepare our request
    size_t remote_len = client_req.GetTotalLength();
    char *remote_req = (char *) malloc(remote_len);
    client_req.FormatRequest(remote_req);

    string remote_host = client_req.GetHost();

    // if (remote_req in our cache) { get from cache }
    // else { get from remote server: below }
    // Connect to remote server
    int remote_fd = client_connect(remote_host.c_str(), REMOTE_SERVER_PORT);
    if (remote_fd < 0) {
        fprintf(stderr, "client: couldn't connect to remote host %s on port %s\n", remote_host.c_str(), REMOTE_SERVER_PORT);
        free(remote_req);
        return -1;
    }

    // Send the request
    if (send_all(remote_fd, remote_req, remote_len) == -1) {
        fprintf(stderr, "client: send error\n");
        free(remote_req);
        close(remote_fd);
        return -1;
    }
    
    string remote_res;
    if (client_receive(remote_fd, remote_res) < 0) {
        fprintf(stderr, "client: couldn't get data from remote host %s on port %s\n", remote_host.c_str(), REMOTE_SERVER_PORT);
        free(remote_req);
        close(remote_fd);
        return -1;
    }

    // Done getting data
    close(remote_fd);

    // Send response back to client
    if (send_all(client_fd, remote_res.c_str(), remote_res.length()) == -1) {
        fprintf(stderr, "server: send error\n");
        free(remote_req);
        return -1;
    }
    
    free(remote_req);
    return 0;
}

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

    // Make server
    sockfd = create_server(PROXY_SERVER_PORT);
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
            if (process_request(new_fd) < 0) {
                fprintf(stderr, "proxy: couldn't forward HTTP request\n");
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
    return 0;
}
