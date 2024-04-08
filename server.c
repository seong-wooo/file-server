#include "wthr.h"
#include <pthread.h>

#define SERVERPORT 9000
#define BUFSIZE 10000

void _bind(SOCKET sock, int port);
void _listen(SOCKET sock, int maxconn);
SOCKET _accept(SOCKET sock, struct sockaddr_in *client_addr);
void _print_connected_client(struct sockaddr_in* client_addr);
void _print_disconnected_client(struct sockaddr_in* client_addr);

int main(int argc, char const *argv[])
{
    int retval;
    char buf[BUFSIZE];
    SOCKET listen_sock = create_socket();
    _bind(listen_sock, SERVERPORT);
    _listen(listen_sock, SOMAXCONN);

    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        perror("pipe");
        return 0;
    }

    SOCKET client_sock;
    struct sockaddr_in client_addr;
    pthread_t wthr;

    while (1)
    {
        client_sock = _accept(listen_sock, &client_addr);
        _print_connected_client(&client_addr);
        
        while(1) {
            retval = recv(client_sock, buf, BUFSIZE, 0);
            if (retval == SOCKET_ERROR)
            {
                err_display("recv()");
                break;
            }

            else if (retval == 0)
            {
                break;
            }

            Job job = {client_sock, buf, pipefd[1]};
            
            retval = pthread_create(&wthr, NULL, process_client, (void *) &job);
            if (retval != 0)
            {
                close(client_sock);
            }
            printf("대기중!");
            pthread_join(wthr, NULL);
            printf("종료!");
            
            read(pipefd[0], buf, sizeof(buf));

            retval = send(client_sock, buf, BUFSIZE, 0);
            if (retval == SOCKET_ERROR)
            {
                err_display("send()");
                break;
            }
        }
        close(client_sock);
        _print_disconnected_client(&client_addr);
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

void _print_connected_client(struct sockaddr_in* client_addr) {
    char client_ip[20];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n", client_ip, ntohs(client_addr->sin_port));
}

void _print_disconnected_client(struct sockaddr_in* client_addr) {
    char client_ip[20];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("[TCP 서버] 클라이언트 접속 종료: IP 주소=%s, 포트 번호=%d\n", client_ip, ntohs(client_addr->sin_port));
}