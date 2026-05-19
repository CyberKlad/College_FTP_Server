//Korbin Gillette
#ifndef KG
#define KG

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SOCK_CHAR_MAX 5
#define PORTNUMSTR "49200"
#define PORTNUM atoi(PORTNUMSTR)
#define SOCK_CHAR_MAX 5
#define SOCK_BYTE_MAX 1024
#define BUFFSIZE (PATH_MAX+6)
#define LOADTIME 10000

#endif
