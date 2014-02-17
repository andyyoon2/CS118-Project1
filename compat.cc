#include "compat.h"

#ifndef HAVE_MEMMEM

#ifndef _LIBC
# define __builtin_expect(expr, val)   (expr)
#endif
#include <string.h>

/* Return the first occurrence of NEEDLE in HAYSTACK.  */
void *
memmem (const void *haystack, size_t haystack_len, const void *needle,
	size_t needle_len)
{
  const char *begin;
  const char *const last_possible
    = (const char *) haystack + haystack_len - needle_len;

  if (needle_len == 0)
    /* The first occurrence of the empty string is deemed to occur at
       the beginning of the string.  */
    return (void *) haystack;

  /* Sanity check, otherwise the loop might search through the whole
     memory.  */
  if (__builtin_expect (haystack_len < needle_len, 0))
    return NULL;

  for (begin = (const char *) haystack; begin <= last_possible; ++begin)
    if (begin[0] == ((const char *) needle)[0] &&
	!memcmp ((const void *) &begin[1],
		 (const void *) ((const char *) needle + 1),
		 needle_len - 1))
      return (void *) begin;

  return NULL;
}
#endif

#ifndef HAVE_STPNCPY
#include <string.h>
#include <stddef.h>

char *
stpncpy (char *dst, const char *src, size_t len)
{
  size_t n = strlen (src);
  if (n > len)
    n = len;
  return strncpy (dst, src, len) + n;
}
#endif

/* 
 * A bunch of helper functions as well as our cache implementation
 */

#include <string>
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
#include <iostream>

//Includes for cache
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/filesystem.hpp>
#include <sys/file.h>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>

//Includes for checking time
#include <boost/date_time/posix_time/posix_time.hpp>
#include "boost/date_time/time_zone_base.hpp"
#include <boost/date_time.hpp>
#include <clocale>
#include <ctime>


using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::local_time;
using namespace boost::filesystem;
using namespace boost::interprocess;
using namespace std;



void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

/*
 * gets socket addr, only need to support IPv4
 */
void *get_in_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
}


/*
 * Helper function to deal with timeouts. Default timeout is set at 30 seconds
 */
int rcvTimeout(int i, char* buffer, int length){
    fd_set fd;
    struct timeval tv;
    
    //Set the fds
    FD_ZERO(&fd);
    FD_SET(i, &fd);
    
    tv.tv_sec = 30; //seconds
    tv.tv_usec = 0; //msecs
    
    int timer = select(i+1,&fd,NULL,NULL,&tv);
    if(timer <= 0){
        cout << "TIMEOUT" << endl;
        return -1; //There was a timeout
    }
    return recv(i,buffer,length,0); //data has arrived, so use regular recv() function
}

/* 
 * Creates socket, binds to port, starts listening
 * Returns sockfd, or <0 if error
 */
int create_server(const char* port) {
    //From Beej's guide
    int sockfd;
    struct addrinfo hints, *res, *p;
    int status;
    int temp = 1;

    // Initialize address struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // we only need to support IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        fprintf(stderr, "server: getaddrinfo error %s\n", gai_strerror(status));
        return -1;
    }

    // Loop through results, bind to the first one we can
    for (p = res; p != NULL; p = p->ai_next) {
        // Create the socket
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            fprintf(stderr, "server: socket error\n");
            continue;
        }

        //setup socket options
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(int)) == -1){
            fprintf(stderr, "server: setsockopt error\n");
            exit(1);            
        }

        // Bind the socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) != 0) {
            close(sockfd);
            fprintf(stderr, "server: bind error\n");
            continue;
        }

        break;
    }

    // Couldn't bind to anything
    if (p == NULL) {
        fprintf(stderr, "server: bind failed\n");
        return -1;
    }
                
    // All done with res struct
    freeaddrinfo(res);

    // Start listening
    if (listen(sockfd, BACKLOG) == -1) {
        fprintf(stderr, "server: listen error\n");
        return -1;
    }

    //Everything else is good, return our socket
    return sockfd;
}

/*
 * Creates socket, connects to remote host
 * Returns sockfd, or <0 if error
 */
int client_connect(const char* host, const char* port) {
    int sockfd;
    struct addrinfo hints, *res, *p;
    char s[INET_ADDRSTRLEN];
    int status;
	
    cout << "Client connecting" << endl;
    // Initialize address struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
        fprintf(stderr, "client: getaddrinfo error %s\n", gai_strerror(status));
        return -1;
    }

    // Loop through results, connect to the first one we can
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            fprintf(stderr, "client: socket error\n");
            continue;
        }
	
	int connect_status = connect(sockfd,p->ai_addr,p->ai_addrlen);
        if (connect_status < 0) {
            close(sockfd);
            fprintf(stderr, "client: connect error\n");
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: connection failed\n");
        return -1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(res);

    return sockfd;
}

/*
 * Gets all data from the specified source
 */
int client_receive(HttpRequest* obj, int sockfd, std::string &res) {
    bool valid = false; //set it true inside
    int timedout = 0; //Check our rcvTimeout function to see if connection has timed out
    int rnrn = 0; //Keeps track of whether or not we hit \r\n\r\n
    char cur; //Our current byte that we're receiving
    for(;;)
    {
        cur = '\0';
        //Reading one byte at a time, we check if we see a pattern of \r\n\r\n
        //If we do, then it means it's the end of the header
        if((timedout = rcvTimeout(sockfd,&cur,1)) == 1){ 
            res += cur;
            //If rnrn = 3, and the current is \n, it means we're at the end
            if(cur == '\n' && rnrn == 3) 
                valid = true;
            else 
                if(cur == '\r' && rnrn == 2) 
                rnrn++;
            else 
                if(cur == '\n' && rnrn == 1) 
                rnrn++;
            else 
                if(cur == '\r')
                rnrn = 1;
            else // Didn't find \r\n\r\n so reset
                rnrn = 0;    
        }
            //If we reach here it means there are no more packets being sent
            else
                break;
    }
    if(!valid){
        fprintf(stderr, "client: recv error\n");
        return -1;
        }
    return 0;
}

/*
 * Ensure all data is sent
 */
int send_all(int sockfd, const char *buf, int len) {
    int sent = 0;   // Bytes we've sent
    int left = len; // Bytes we have left to send
    int n;

    while (sent < len) {
        n = send(sockfd, buf+sent, left, 0);
        if (n == -1) {
            fprintf(stderr, "send error, we only sent %d of %d bytes\n", sent, len);
            return -1;
        }
        sent += n;
        left -= n;
    }
    return 0;
}

/**************************
 *     CODE FOR CACHE     *
 **************************/

//Saves data into our catch
bool save_data(string id, string content){ 

    try{
        fstream file;
        replace( id.begin(), id.end(), '/', '_');
        //Opens up a file and writes the contents
        file.open(("local_cache/"+id).c_str(), fstream::out);
        file << content;
        file.close();
        cout << "Successfully stores into cache" << endl;  
        return true;
    }
    catch(...){
        fprintf(stderr, "cache: failed to store in cache\n");    
        return false;
    }

}

//Grabs data from our cache
string get_data(string id){
    try{
        replace( id.begin(), id.end(), '/', '_');
        //Code from tutorial
        ifstream file(("local_cache/" + id).c_str(), ios::in | ios::binary);
        if(file){
            string content((std::istreambuf_iterator<char>(file)), 
                            (std::istreambuf_iterator<char>())); 
            file.close();
            return content;
        }
        else
            return "";
    }
    catch(...){
        fprintf(stderr, "cache: failed to find in cache\n");
        return "";
    }
}   

//simple check if expired
bool expiration(string date){

    //Get current time and convert the date
    ptime current(second_clock::local_time());
    ptime convert; //Time we'll be converting
    const locale f_locale = locale(locale::classic(), 
                            new time_input_facet("%a, %d %b %Y %H:%M:%S %Z"));
    istringstream temp(date);
    //use our locale
    temp.imbue(f_locale);
    temp >> convert;
    if(convert <= current){
        cout << "The cached version is expired" << endl;
        return true;
    }
    else{
        cout << "The cached version is valid" << endl;
        return false;
    }
}

//Our cache is here, gets an obj and returns whether or not we found it in our cache
bool cache(HttpRequest* obj, string& returned){

    //first check if we have this request cached and not expired
    string data = get_data(obj->GetHost()+obj->GetPath());
    
    if(data.length() > 1){ //we found an object let's make sure it's not expired
        printf("Request found in cache, checking if cert has not expired\n");
        HttpResponse* cached = new HttpResponse;
        cached->ParseResponse(data.c_str(),data.length());
        if(!expiration(cached->FindHeader("Expires"))){
            printf("Using cached data\n");
            returned = data;
            delete cached;
            return true;
        }
        delete cached;
    }
    //It's not in the cache
    return false;
}   
       

