#include "Common.h"

char *SERVERIP = (char *) "127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE 10000

void _connect_to_server(SOCKET sock, char* ip, int port);
void _get_list(char* request);
void _get_read(char* request);
void _get_write(char* request);
int _get_option(void);
int _get_offset(void);
int _get_length(void);
void _get_data(char* data);
void _get_filename(char* filename);

int main(int argc, char *argv[])
{
    int retval;

    SOCKET sock = create_socket();
    _connect_to_server(sock, SERVERIP, SERVERPORT);
    
    char buf[BUFSIZE + 1];
    char option;
    char request[512];

    while(1) {
        option = _get_option();
        
        if (option == LIST) {
            _get_list(request);
        }

        else if (option == READ) {
            _get_read(request);
        }

        else if (option == WRITE) {
            _get_write(request);
        } 
        
        else if (option == DELETE) {
        }
        
        else {
            break;
        }

        retval = send(sock, request, (int)strlen(request), 0);
        if (retval == SOCKET_ERROR) {
            err_display("send()");
            break;
        }

        retval = recv(sock, buf, BUFSIZE, 0);
        if (retval == SOCKET_ERROR) {
            err_display("recv()");
            break;
        }

        buf[retval] = '\0';
        printf("[받은 데이터]:\n%s\n", buf); 
    }  

    close(sock);
    return 0;
}   

void _connect_to_server(SOCKET sock, char* ip, int port) {
    int retval;
    struct sockaddr_in serveraddr;

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(port);
    retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("connect()");
}

int _get_option(void) {
    char option;
    do {
        printf("[파일 리스트 조회: %c / 파일 읽기 : %c / 파일 쓰기 : %c / 파일 삭제 : %c / 종료 : %c]: ", LIST, READ, WRITE, DELETE, QUIT);
        scanf("%c", &option);
        getchar();
    } while (option != LIST && option != READ && option != WRITE && option != DELETE && option != QUIT);

    return option;
}

void _get_list(char* request) {
    sprintf(request, "%s%s%c", OPTION, TOKEN_PARSER, LIST);
}

void _get_read(char* request) {
    char filename[100];
    int offset;
    int length;

    _get_filename(filename);
    offset = _get_offset();
    length = _get_length();
    sprintf(request, "%s%s%c%s%s%s%s%s%s%s%d%s%s%s%d", OPTION, TOKEN_PARSER, READ, LINE_PARSER, FILENAME, TOKEN_PARSER, filename, LINE_PARSER, OFFSET, TOKEN_PARSER, offset, LINE_PARSER, LENGTH, TOKEN_PARSER, length);
}

void _get_write(char* request) {
    char filename[1000];
    int offset;
    int length;
    char data[1000];

    _get_filename(filename);
    offset = _get_offset();
    _get_data(data);
    sprintf(request, "%s%s%c%s%s%s%s%s%s%s%d%s%s%s%s", OPTION, TOKEN_PARSER, WRITE, LINE_PARSER, FILENAME, TOKEN_PARSER, filename, LINE_PARSER, OFFSET, TOKEN_PARSER, offset, LINE_PARSER, DATA, TOKEN_PARSER, data);
}

int _get_offset(void) {
    int offset;
    printf("[오프셋]: ");
    scanf("%d", &offset);
    getchar();
    return offset;
}

int _get_length(void) {
    int length;
    printf("[읽을 길이]: ");
    scanf("%d", &length);
    getchar();
    return length;
}

void _get_data(char* data) {
    printf("[쓸 내용]: ");
    fgets(data, sizeof(data), stdin); // todo 여기 수정
    printf("data: %s\n", data);
}

void _get_filename(char* filename) {
    printf("[파일 이름]: ");
    scanf("%s", filename);
    getchar();
}