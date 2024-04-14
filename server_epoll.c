#include "wthr.h"
#include <sys/epoll.h>
#include <sys/select.h>
#include <fcntl.h> 
#include <stdbool.h>

void _bind(SOCKET sock, int port);
void _listen(SOCKET sock, int maxconn);
SOCKET _accept(SOCKET sock, struct sockaddr_in *client_addr);
bool AddSocketInfo(SOCKET sock);
void remove_socket_info(int index);
void print_connected_client(struct sockaddr_in* client_addr);
void print_disconnected_client(struct sockaddr_in* client_addr);

typedef struct SOCKETINFO {
    SOCKET sock;
    char buf[BUFSIZE];
    int recvbytes;
} SOCKETINFO;

int pipefd[2];

int main(int argc, char const *argv[])
{   
    int retval;
    SOCKET server_sock = create_socket();
    _bind(server_sock, SERVERPORT);
    _listen(server_sock, SOMAXCONN);

    int epollfd = epoll_create(1);
    if (epollfd < 0) {
        perror("epoll_create()");
        exit(EXIT_FAILURE);
    }

    register_event(epollfd, server_sock, EPOLLIN);
    struct epoll_event events[FD_SETSIZE];
    int nready;

    if (pipe(pipefd) < 0)
    {
        perror("pipe");
        return 0;
    }

    Queue queue;
    init_wthr_pool(&queue, pipefd[1]);

    SOCKET client_sock;
    struct sockaddr_in client_addr;

    while (1)
    {

        nready = epoll_wait(epollfd, events, FD_SETSIZE, -1);
        if (nready < 0) {
            perror("epoll_wait()");
            exit(EXIT_FAILURE);
        }


        for (int i = 0; i < nready; i++) {
            SOCKETINFO *ptr = (SOCKETINFO *)events[i].data.ptr;
            
            if (ptr->sock == server_sock) {
                client_sock = _accept(server_sock, &client_addr);
                print_connected_client(&client_addr);
                register_event(epollfd, client_sock, EPOLLIN);
                if (--nready <= 0)
                {
                    break;
                }
            }
            else {
                if (events[i].events & EPOLLIN) {
                    retval = recv(ptr->sock, ptr->buf, BUFSIZE, MSG_WAITALL);
                    if (retval == SOCKET_ERROR) {
                        err_display("recv()");
                        print_disconnected_client(ptr->sock);
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, ptr->sock, NULL);
                        close(ptr->sock);
                        free(ptr);
                        break;
                    }
                    else if (retval == 0) {
                        print_disconnected_client(ptr->sock);
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, ptr->sock, NULL);
                        close(ptr->sock);
                        free(ptr);
                        break;
                    }
                    else {
                        ptr->recvbytes = retval;
                        ptr->buf[ptr->recvbytes] = '\0';
                        Job job = {sock, ptr->buf};
                        enqueue(&queue, &job);
                        modify_events(epollfd, events[i], EPOLLOUT);
                    }
                }

                if (events[i].events & EPOLLOUT) {
                    read(pipefd[0], ptr->buf, BUFSIZE);
                    ptr->buf[strlen(ptr->buf)] = '\0';
                    retval = send(sock, ptr->buf, BUFSIZE, MSG_WAITALL);
                    if (retval == SOCKET_ERROR)
                    {
                        err_display("send()");
                        epoll_ctl(epollfd, EPOLL_CTL_DEL, ptr->sock, NULL);
                        close(ptr->sock);
                        free(ptr);
                    } 
                    else 
                    {
                        if (ptr->recvbytes > 0) {
                            ptr->recvbytes = 0;
                            modify_events(epollfd, events[i], EPOLLIN);
                        }
                    }
                }
            }
		}
    }

    freeQueue(&queue); 
    close(server_sock);
    close(epollfd);
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
    socklen_t addrlen = sizeof(client_addr);

    client_sock = accept(sock, (struct sockaddr *)client_addr, &addrlen);
    if (client_sock == INVALID_SOCKET)
    {
        err_display("accept()");
    } 
    
    else 
    {
        if (!AddSocketInfo(client_sock))
        {
            close(client_sock);
        }
    }

    return client_sock;
}

void register_event(int epollfd, SOCKET sock, uint32_t events) {
    SOCKETINFO *ptr;
    ptr->sock = sock;
    ptr->recvbytes = 0;
    ptr->sendbytes = 0;

    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = ptr;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) < 0) { // 소켓 이벤트 Epoll 인스턴스에 등록
        perror("epoll_ctl()");
        exit(EXIT_FAILURE);
    }
}

// Epoll 인스턴스에 등록된 소켓에 새로운 이벤트를 등록하는 함수
void modify_events(int epollfd, struct epoll_event ev, uint32_t events) {
    ev.events = events; // 새로운 이벤트 넣는다.
    SOCKETINFO *ptr = (SOCKETINFO *)ev.data.ptr;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, ptr->sock, &ev) < 0) { // epoll_event 구조체에 들어있는 소켓과 epoll_event 구조체를 활용해 소켓 이벤트 정보 변경
        perror("epoll_ctl()");
        exit(EXIT_FAILURE);
    }
}

void print_connected_client(struct sockaddr_in* client_addr) {
    char client_ip[20];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n", client_ip, ntohs(client_addr->sin_port));
}

void print_disconnected_client(SOCKET* sock) {
    struct sockaddr_in clientaddr;
	socklen_t addrlen = sizeof(clientaddr);
	getpeername(sock, (struct sockaddr *)&clientaddr, &addrlen);

    char client_ip[20];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("[TCP 서버] 클라이언트 접속 종료: IP 주소=%s, 포트 번호=%d\n", client_ip, ntohs(client_addr->sin_port));
}