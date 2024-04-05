#include "wthr.h"
#include <pthread.h>

#define SERVERPORT 9000

void _bind(SOCKET sock, int port);
void _listen(SOCKET sock, int maxconn);
SOCKET _accept(SOCKET sock, struct sockaddr_in *client_addr);

int main(int argc, char const *argv[])
{
    int retval;

    SOCKET listen_sock = create_socket();
    _bind(listen_sock, SERVERPORT);
    _listen(listen_sock, SOMAXCONN);

    SOCKET client_sock;
    struct sockaddr_in client_addr;
    pthread_t wthr;

    while (1)
    {
        client_sock = _accept(listen_sock, &client_addr);
        retval = pthread_create(&wthr, NULL, process_client, (void *)(long long)client_sock);
        if (retval != 0)
        {
            close(client_sock);
        }
    }

    close(listen_sock);
    printf("[TCP 서버] 서버 종료\n");

    return 0;
}

void _bind(SOCKET sock, int port)
{
    int retval;
    struct sockaddr_in serveraddr;

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    retval = bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        err_quit("bind()");
}

void _listen(SOCKET sock, int maxconn)
{
    int retval;

    retval = listen(sock, maxconn);
    if (retval == SOCKET_ERROR)
    {
        err_quit("listen()");
    }
}

SOCKET _accept(SOCKET sock, struct sockaddr_in *client_addr)
{
    SOCKET client_sock;
    int addrlen = sizeof(client_addr);

    client_sock = accept(sock, (struct sockaddr *)client_addr, &addrlen);
    if (client_sock == INVALID_SOCKET)
    {
        err_display("accept()");
    }

    return client_sock;
}