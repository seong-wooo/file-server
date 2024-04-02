#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

typedef int SOCKET;

void err_quit(const char *msg) {
    char *msgbuf = strerror(errno);
    printf("[%s] %s \n", msg, msgbuf);
    exit(1);
};

void err_display(const char *msg) {
    char *msgbuf = strerror(errno);
    printf("[%s] %s\n", msg, msgbuf);
}

