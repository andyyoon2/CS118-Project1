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

#define SERVER_LISTEN_PORT "14886"  // Port users will connect to
#define BACKLOG 20                  // Number of requests allowed in queue

void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
    }

// gets socket addr, only need to support IPv4
void *get_in_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
}
    
int create_server(const char* port) {
    int sock_fd;
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

    return sock_fd;
}
