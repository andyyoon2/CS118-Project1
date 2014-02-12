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

#define PROXY_SERVER_PORT "14886"   // Port users will connect to
#define REMOTE_SERVER_PORT "80"     // Port our proxy will connect to
#define BACKLOG 20                  // Number of requests allowed in queue
#define BUF_SIZE 1086               // Number of bytes we can get at once
#include <string>

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int create_server(const char* port);
int client_connect(const char* host, const char* port);
int client_receive(int sockfd, std::string &res);
int send_all(int sockfd, const char *buf, int len);
#endif
