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
 * Creates socket, binds to port, starts listening
 * Returns sockfd, or <0 if error
 */
int create_server(const char* port) {
    int sockfd;
    struct addrinfo hints, *res, *p;
    int status;

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

    // Initialize address struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
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

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            fprintf(stderr, "client: connect error\n");
            continue;
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
int client_receive(int sockfd, std::string &res) {
    char buf[BUF_SIZE];
    int numbytes;
    while (true) {
        if ((numbytes = recv(sockfd, buf, BUF_SIZE-1, 0)) == -1) {
            fprintf(stderr, "client: recv error\n");
            return -1;
        }

        // Server is done sending
        else if (numbytes == 0) {
            break;
        }

        // Append to response
        res.append(buf, numbytes);
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
