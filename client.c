#include "Common.h"

char *SERVERIP = (char *) "127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE 512

int getoption(void);
int getoffset(void);
int getlength(void);
void getdata(char* data);
void getfilename(char* filename);

int main(int argc, char *argv[])
{
    int retval;

    // 명령행 인수가 있으면 IP 주소로 사용
    if (argc > 1) SERVERIP = argv[1];

    // 소켓 생성
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) err_quit("socket()");

    // connect()
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("connect()");
    
    // 데이터 통신에 사용할 변수
    char buf[BUFSIZE + 1];
    int len;
    
    
    
    char option;
    char filename[100];
    int offset;
    int length;
    char data[100];
    char request[300];

    while(1) {
        option = getoption();
        
        if (option == LIST) {
            sprintf(request, "%s:%c", OPTION, option);
        }

        else if (option == READ) {
            getfilename(filename);
            offset = getoffset();
            length = getlength();
            sprintf(request, "%s:%c\n%s:%s\n%s:%d\n%s:%d", OPTION, option, FILENAME, filename, OFFSET, offset, LENGTH, length);
        }

        else if (option == WRITE) {
            getfilename(filename);
            offset = getoffset();
            getdata(data);
            sprintf(request, "%s:%c\n%s:%s\n%s:%d\n%s:%s", OPTION, option, FILENAME, filename, OFFSET, offset, DATA, data);
        } 
        
        else if (option == DELETE) {
            // 파일 제거
        }
        
        else {
            break;
        }

        retval = send(sock, request, (int)strlen(request), 0);
        if (retval == SOCKET_ERROR) {
            err_display("send()");
            break;
        }

        retval = recv(sock, buf, retval, MSG_WAITALL);
        if (retval == SOCKET_ERROR) {
            err_display("recv()");
            break;
        }

        // 받은 데이터 출력
        buf[retval] = '\0';
        printf("[받은 데이터] %s\n", buf); 
    }  

    close(sock);
    return 0;
}   

int getoption(void) {
    char option;
    do {
        printf("[파일 리스트 조회: %c / 파일 읽기 : %c / 파일 쓰기 : %c / 파일 삭제 : %c / 종료 : %c]: ", LIST, READ, WRITE, DELETE, QUIT);
        scanf("%c", &option);
        getchar();
    } while (option != LIST && option != READ && option != WRITE && option != DELETE && option != QUIT);

    return option;
}

int getoffset(void) {
    int offset;
    printf("[오프셋]: ");
    scanf("%d", &offset);
    getchar();
    return offset;
}

int getlength(void) {
    int length;
    printf("[읽을 길이]: ");
    scanf("%d", &length);
    getchar();
    return length;
}

void getdata(char* data) {
    printf("[쓸 내용]: ");
    fgets(data, sizeof(data), stdin);
    data[strlen(data) - 1] = '\0';
}

void getfilename(char* filename) {
    printf("[파일 이름]: ");
    fgets(filename, sizeof(filename), stdin);
    filename[strlen(filename) - 1] = '\0';
}