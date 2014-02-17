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
#include <cerrno>
#include <iostream>
#include <sstream>

#include "compat.h"
#include "http-request.h"
#include "http-response.h"

using namespace std;

/*
 * Gets HTTP request from client and forwards to remote server
 */
int process_request(int client_fd) {
    string client_buf = "";
    char buf[BUF_SIZE];

    //if persistant connection we'll restart a loop here
    persistent:

    //Boolean checking whether or not we have a persistent connection
    bool p_connection = false; //Assume it's initially false
    int a;

    // Receive request until "\r\n\r\n"
    while (true) {
	   //Clear what's currently in the buffer
	   bzero(buf,BUF_SIZE);

        if (rcvTimeout(client_fd, buf, BUF_SIZE-1) < 0) {
            fprintf(stderr,"server: recv error\n");
            return -1;
        }
        client_buf.append(buf);
        //If we reach \r\n\r\n then we exit
        if(memmem(client_buf.c_str(),client_buf.length(),"\r\n\r\n",4) != NULL)
        	break;
    }
	
    cout << "Processing the request: " << client_buf << endl;
    // Parse the request, prepare our own request to send to remote
    HttpRequest client_req;
    try {
        client_req.ParseRequest(client_buf.c_str(),client_buf.length());
        string version_check1 = "1.0";
        string version_check2 = "1.1";
        if(version_check1.compare(client_req.GetVersion()) == 0){ 
            //We are serving HTTP 1.0
            printf("This is a HTTP/1.0:Non-Persistent Connection\n");
            p_connection = false;
        }
        else if(version_check2.compare(client_req.GetVersion()) == 0){ 
            //We are serving HTTP 1.1
            printf("This is a HTTP/1.1:Persistent Connection\n");
            p_connection = true;
        }
        else{//Unsupported version
            fprintf(stderr,"Error: Unsupported HTTP version (supports 1.0 and 1.1 connections only)\n");
            return -1;
        }
    }
    // Send error response back to client
    catch (ParseException exn) {
        fprintf(stderr, "Parse Exception: %s\n", exn.what());
        
        // Assume version 1.1
        string res = "HTTP/1.1 ";

        const char *cmp1 = "Request is not GET";
        const char *cmp2 = "Only GET method is supported";
        // Set proper status code
        if (strcmp(exn.what(),cmp1) == 0 || strcmp(exn.what(),cmp2) == 0) {
            res += "501 Not Implemented\r\n\r\n";
        }
        else {
            res += "400 Bad Request\r\n\r\n";
        }

	   cout << "Sending response " << endl;
        // Send response
        if (send_all(client_fd, res.c_str(), res.length()) == -1) {
            fprintf(stderr, "server: send error\n");
            return -1;
        }
    }
    
    // Prepare our request
    size_t remote_len = client_req.GetTotalLength() + 1 ; //Need the + 1 to account for the null byte
    char *remote_req = (char *) malloc(remote_len);
    client_req.FormatRequest(remote_req);

    string remote_host = client_req.GetHost();
    // Get port number, convert to string
    a = client_req.GetPort();
    stringstream ss;
    ss << a;
    string remote_port = ss.str();
	
    cout << "Getting port number " << remote_port << endl;
    // if (remote_req in our cache) { get from cache }
    // else { get from remote server: below }
    // Connect to remote server
    int remote_fd = client_connect(remote_host.c_str(), remote_port.c_str());
    if (remote_fd < 0) {
        fprintf(stderr, "client: couldn't connect to remote host %s on port %s\n", remote_host.c_str(), remote_port.c_str());
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
    
    string remote_res = "";
    cout << "Trying to get response from server" << endl;
    a = client_receive(&client_req, remote_fd, remote_res);
    cout << "Got response from server" << endl;
    //cout << "Response contains:\n " << remote_res << endl;

    //Check in case of errors
    if (a < 0 && (errno != EWOULDBLOCK && errno != EAGAIN)) {
        fprintf(stderr, "client: couldn't get data from remote host %s on port %s\n", remote_host.c_str(), remote_port.c_str());
        free(remote_req);
        close(remote_fd);
        return -1;
    }

    // Send response back to client
    if (send_all(client_fd, remote_res.c_str(), remote_res.length()) == -1) {
        fprintf(stderr, "server: send error\n");
        free(remote_req);
        return -1;
    }

    // Close connection if concurrent, otherwise leave connection open
    if(!p_connection){
        cout << "Closing connection" << endl;
        close(remote_fd);
    }
    else
    {
        cout << "Persistent connection, waiting for next input" << endl;
        client_buf = "";
        goto persistent;
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

    //Initialize and set up concurrent connections
    pid_t plist[CONCURRENT_CONN];
    for(int i=0; i<CONCURRENT_CONN; i++){
        plist[i] = -1;
    }

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

    printf("server: waiting for connections on port %s...\n", PROXY_SERVER_PORT);

    // Main loop
    while (true) {
        int running = -1; //Current running processes
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            //Restart loop
            continue;
        }
        //If we haven't run out of processes to allocate:
        for(int i =0; i<CONCURRENT_CONN; i++){
            if(plist[i] < 0){ //free process slot
                running = i;
                break;
            }
            else{ //Wait for a process
                int wait = 0;
                pid_t result;
                if((result = waitpid(plist[i],&wait,WNOHANG)) < 1) { 
                    //Go to next process
                    continue;   
                }
            else{ //process is free, so take it
                running = i;
                break;
                }
            }
        }

        //If we can't have free process, we drop it since we've reached our max 
        if(running < 0){
            close(new_fd);
            continue; 
        }


        //Here, we fork for each of our connections
        pid_t pid = fork();

        //Fork worked
        if(pid > 0)
        { 
            //Parent thread
            plist[running] = pid;
            close(new_fd); 
        }
        else if(pid == 0)
        {
            //Child thread
            inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
            printf("server: got connection from %s\n", s);
            
            if (process_request(new_fd) < 0) 
            {
                //Error in processing our request
                fprintf(stderr, "proxy: couldn't forward HTTP request\n");
            }
            close(new_fd);
            exit(0);
        }
        else
        {
            //Fork failed, so we retry again
            plist[running] = -1;
            close(new_fd);
            continue;
        }
    }
}
