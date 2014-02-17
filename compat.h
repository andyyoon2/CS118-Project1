#ifndef COMPAT_H
#define COMPAT_H

#include <stddef.h>

#ifndef HAVE_MEMMEM
/* Return the first occurrence of NEEDLE in HAYSTACK.  */
void *
memmem (const void *haystack, size_t haystack_len, const void *needle,
	size_t needle_len);
#endif

#ifndef HAVE_STPNCPY
char *
stpncpy(char *s1, const char *s2, size_t n);
#endif

#define PROXY_SERVER_PORT "34567"   // Port users will connect to
#define CONCURRENT_CONN 20 			//
#define BACKLOG 20                  // Number of requests allowed in queue
#define BUF_SIZE 1024               // Number of bytes we can get at once
#include <string>
#include "http-request.h"
#include "http-response.h"

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int create_server(const char* port);
int client_connect(const char* host, const char* port);
int client_receive(HttpRequest* obj, int sockfd, std::string &res);
int rcvTimeout(int i, char* buffer, int length);
int send_all(int sockfd, const char *buf, int len);
#endif
