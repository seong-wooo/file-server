#include "wthr.h"
#include <poll.h>
#include <sys/select.h>
#include <fcntl.h> 
#include <stdbool.h>

void _bind(SOCKET sock, int port);
void _listen(SOCKET sock, int maxconn);
SOCKET _accept(SOCKET sock, struct sockaddr_in *client_addr);
bool AddSocketInfo(SOCKET sock);
void remove_socket_info(int index);
void _print_connected_client(struct sockaddr_in* client_addr);
void _print_disconnected_client(struct sockaddr_in* client_addr);

typedef struct SOCKETINFO {
    char buf[BUFSIZE];
    int recvbytes;
    int sendbytes;
} SOCKETINFO;
int total_fds = 0;

SOCKETINFO *socket_info[FD_SETSIZE];
struct pollfd poll_fds[FD_SETSIZE];
int pipefd[2];

int main(int argc, char const *argv[])
{   
    int retval;
    SOCKET listen_sock = create_socket();
    _bind(listen_sock, SERVERPORT);
    _listen(listen_sock, SOMAXCONN);

    poll_fds[0].fd = listen_sock;
    poll_fds[0].events = POLLIN;
    ++total_fds;

    if (pipe(pipefd) < 0)
    {
        perror("pipe");
        return 0;
    }

    poll_fds[1].fd = pipefd[0];
    poll_fds[1].events = POLLIN;
    ++total_fds;

    WTHR_QUEUE jobs;
    init_wthr_pool(&jobs, pipefd[1]);

	int nready;
    SOCKET client_sock;
    struct sockaddr_in client_addr;
    pthread_t wthr;

    while (1)
    {
        for (int i = 2; i < total_fds; i++) {
			if (socket_info[i]->recvbytes > socket_info[i]->sendbytes)
			{
                poll_fds[i].events = POLLOUT; 
            }
			else
				poll_fds[i].events = POLLIN; 
		}

        nready = poll(poll_fds, total_fds, -1);
        if (nready == SOCKET_ERROR)
        {
            perror("poll");
            break;
        }

        if (poll_fds[0].revents & POLLIN) {
            client_sock = _accept(listen_sock, &client_addr);
            _print_connected_client(&client_addr);

            if (--nready <= 0)
                continue;
        }

        for (int i = 2; i < total_fds; i++) {
            SOCKETINFO *ptr = socket_info[i];
            SOCKET sock = poll_fds[i].fd;

            if (poll_fds[i].revents & POLLIN) {
                retval = recv(sock, ptr->buf, BUFSIZE, MSG_WAITALL);
                if (retval == SOCKET_ERROR) {
                    err_display("recv()");
                    remove_socket_info(i);
                    break;
                }
                else if (retval == 0) {
                    remove_socket_info(i);
                }
                else {
                    ptr->recvbytes = retval;
					ptr->buf[ptr->recvbytes] = '\0';
                    Job job = {sock, ptr->buf};
                    enqueue(&jobs, job);
                }
            }
            else if (poll_fds[i].revents & POLLOUT) 
            {
                if (poll_fds[1].revents & POLLIN) {
                    read(pipefd[0], ptr->buf, BUFSIZE);
                    ptr->buf[strlen(ptr->buf)] = '\0';
                    retval = send(sock, ptr->buf, BUFSIZE, MSG_WAITALL);
                    if (retval == SOCKET_ERROR)
                    {
                        err_display("send()");
                        remove_socket_info(i);
                        break;
                    }
                    else 
                    {
                        ptr->recvbytes = ptr->sendbytes = 0;
                    }
                }
            }
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
    
    else 
    {
        if (!AddSocketInfo(client_sock))
        {
            close(client_sock);
        }
    }

    return client_sock;
}

bool AddSocketInfo(SOCKET sock)
{
	if (total_fds >= FD_SETSIZE) {
		printf("[오류] 소켓 정보를 추가할 수 없습니다!\n");
		return false;
	}
	SOCKETINFO *ptr = (SOCKETINFO *)malloc(sizeof(SOCKETINFO));
	if (ptr == NULL) {
		printf("[오류] 메모리가 부족합니다!\n");
		return false;
	}
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	socket_info[total_fds] = ptr;

	poll_fds[total_fds].fd = sock;
	poll_fds[total_fds].events = POLLIN;

	++total_fds;
	return true;
}

void remove_socket_info(int index)
{
	SOCKETINFO *ptr = socket_info[index];
	SOCKET sock = poll_fds[index].fd;

	struct sockaddr_in clientaddr;
	socklen_t addrlen = sizeof(clientaddr);
	getpeername(sock, (struct sockaddr *)&clientaddr, &addrlen);
    _print_disconnected_client(&clientaddr);

	close(sock);
	free(ptr);

	if (index != (total_fds - 1)) {
		socket_info[index] = socket_info[total_fds - 1];
		poll_fds[index] = poll_fds[total_fds - 1];
	}
	--total_fds;
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