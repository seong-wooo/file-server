#include "Common.h"

char *SERVERIP = (char *) "127.0.0.1";

void connect_to_server(SOCKET sock, char* ip, int port);
PACKET *create_packet(char option);
void get_list_packet(PACKET *packet);
void get_read_packet(PACKET *packet);
void get_write_packet(PACKET *packet);
void get_delete_packet(PACKET *packet);
void send_packet(SOCKET sock, PACKET *packet);

void get_list(char* buf);
void get_read(char* buf);
void get_write(char* buf);
void get_delete(char* buf);
int get_option(void);
int get_offset(void);
int get_length(void);
char* get_data(void);
char* get_filename(void);

int main(int argc, char *argv[])
{
    int retval;

    SOCKET sock = create_socket();
    connect_to_server(sock, SERVERIP, SERVERPORT);

    char buf[BUFSIZE];
    while(1) {
        char option = get_option();
        PACKET *packet = create_packet(option);
        send_packet(sock, packet);
        recieve_packet(sock, packet);
        free(packet);

        switch (option) 
        {
            case LIST:
                get_list(buf);
                break;
            case READ:
                get_read(buf);
                break;
            case WRITE:
                get_write(buf);
                break;
            case DELETE:
                get_delete(buf);
                break;
            default:
                break;
        }

        if (option == QUIT) {
            printf("프로그램을 종료합니다.\n");
            break;
        }

        retval = send(sock, buf, BUFSIZE, MSG_WAITALL);
        if (retval == SOCKET_ERROR) {
            err_display("send()");
            break;
        }

        retval = recv(sock, buf, BUFSIZE, MSG_WAITALL);
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

void connect_to_server(SOCKET sock, char* ip, int port) {
    int retval;
    struct sockaddr_in serveraddr;

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serveraddr.sin_addr);
    serveraddr.sin_port = htons(port);
    retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("connect()");
}

int get_option(void) {
    char option;
    do {
        fseek(stdin, 0, SEEK_END);
        printf("[파일 리스트 조회: %c / 파일 읽기 : %c / 파일 쓰기 : %c / 파일 삭제 : %c / 종료 : %c]: ", LIST, READ, WRITE, DELETE, QUIT);
        option = getchar();
        getchar();
    } while (option != LIST && option != READ && option != WRITE && option != DELETE && option != QUIT);

    return option;
}

PACKET *create_packet(char option) {
    PACKET *packet = (PACKET *)malloc(sizeof(PACKET));
    if (packet == NULL) {
        fprintf(stderr, "메모리 할당 실패\n");
        return NULL;
    }

    memset(packet, 0, sizeof(PACKET));

    switch (option) 
        {
            case LIST:
                get_list_packet(packet);
                break;

            case READ:
                get_read_packet(packet);
                break;
            
            case WRITE:
                get_write_packet(packet);
                break;
            case DELETE:
                get_delete_packet(packet);
                break;
            default:
                break;
        }
    printf("[보낼 데이터]:\n%s%s\n", packet->header, packet->body);
    return packet;
}
void get_list_packet(PACKET *packet) {
    packet->bodySize = strlen(OPTION) + strlen(TOKEN_PARSER) + sizeof(char);
    packet->body = (char *)malloc(packet->bodySize + 1);
    sprintf(packet->body, "%s%s%c", OPTION, TOKEN_PARSER, LIST);
    sprintf(packet->header, "%s%s%zu\n", BODY_SIZE_HEADER, TOKEN_PARSER, packet->bodySize);
}

void get_read_packet(PACKET *packet) {
    char temp[1000];
    char *filename = get_filename();
    int offset = get_offset();
    int length = get_length();
    sprintf(temp, "%s%s%c%s%s%s%s%s%s%s%d%s%s%s%d", OPTION, TOKEN_PARSER, READ, LINE_PARSER, FILENAME, TOKEN_PARSER, filename, LINE_PARSER, OFFSET, TOKEN_PARSER, offset, LINE_PARSER, LENGTH, TOKEN_PARSER, length);
    
    free(filename);

    packet->bodySize = strlen(temp);
    packet->body = (char *)malloc(packet->bodySize + 1);
    strcpy(packet->body, temp);
    sprintf(packet->header, "%s%s%zu\n", BODY_SIZE_HEADER, TOKEN_PARSER, packet->bodySize);
    
}

void get_write_packet(PACKET *packet) {
    char temp[1000];
    char *filename = get_filename();
    int offset = get_offset();
    char* data = get_data();
    sprintf(temp, "%s%s%c%s%s%s%s%s%s%s%d%s%s%s%s", OPTION, TOKEN_PARSER, WRITE, LINE_PARSER, FILENAME, TOKEN_PARSER, filename, LINE_PARSER, OFFSET, TOKEN_PARSER, offset, LINE_PARSER, DATA, TOKEN_PARSER, data);
    
    free(filename);
    free(data);
    
    packet->bodySize = strlen(temp);
    packet->body = (char *)malloc(packet->bodySize + 1);
    strcpy(packet->body, temp);
    sprintf(packet->header, "%s%s%zu\n", BODY_SIZE_HEADER, TOKEN_PARSER, packet->bodySize);
}

void get_delete_packet(PACKET *packet) {

    char temp[1000];
    char *filename = get_filename();
    sprintf(temp, "%s%s%c%s%s%s%s", OPTION, TOKEN_PARSER, DELETE, LINE_PARSER, FILENAME, TOKEN_PARSER, filename);
    
    free(filename);
    
    packet->bodySize = strlen(temp);
    packet->body = (char *)malloc(packet->bodySize + 1);
    strcpy(packet->body, temp);
    sprintf(packet->header, "%s%s%zu\n", BODY_SIZE_HEADER, TOKEN_PARSER, packet->bodySize);
}

void send_packet(SOCKET sock, PACKET *packet) {
    size_t totalSize = strlen(packet->header) + packet->bodySize;
    char *buf = (char *)malloc(totalSize + 1);
    if (!buf) {
        fprintf(stderr, "버퍼 할당 실패\n");
        free_packet(packet);
        return;
    }
    sprintf(buf, "%s%s", packet->header, packet->body);

    int retval = send(sock, buf, totalSize, 0);
    if (retval == SOCKET_ERROR) {
        err_display("send()");
        free_packet(packet);
        free(buf);  
        return;
    }
}

void recieve_packet(SOCKET sock, PACKET *packet) {

}

void free_packet(PACKET *packet) {
    if (packet) {
        if (packet->body) {
            free(packet->body);
        }
        free(packet);
    }
}











void get_list(char* buf) {
    sprintf(buf, "%s%s%c", OPTION, TOKEN_PARSER, LIST);
}

void get_read(char* buf) {
    char* filename;
    int offset;
    int length;

    filename = get_filename();
    offset = get_offset();
    length = get_length();
    sprintf(buf, "%s%s%c%s%s%s%s%s%s%s%d%s%s%s%d", OPTION, TOKEN_PARSER, READ, LINE_PARSER, FILENAME, TOKEN_PARSER, filename, LINE_PARSER, OFFSET, TOKEN_PARSER, offset, LINE_PARSER, LENGTH, TOKEN_PARSER, length);
    free(filename);
}

void get_write(char* buf) {
    char* filename;
    int offset;
    int length;
    char* data;

    filename = get_filename();
    offset = get_offset();
    data = get_data();
    sprintf(buf, "%s%s%c%s%s%s%s%s%s%s%d%s%s%s%s", OPTION, TOKEN_PARSER, WRITE, LINE_PARSER, FILENAME, TOKEN_PARSER, filename, LINE_PARSER, OFFSET, TOKEN_PARSER, offset, LINE_PARSER, DATA, TOKEN_PARSER, data);
    
    free(filename);
    free(data);
}

void get_delete(char* buf) {
    char* filename;

    filename = get_filename();
    sprintf(buf, "%s%s%c%s%s%s%s", OPTION, TOKEN_PARSER, DELETE, LINE_PARSER, FILENAME, TOKEN_PARSER, filename);
    free(filename);
}

int get_offset(void) {
    int offset;
    printf("[오프셋]: ");
    scanf("%d", &offset);
    getchar();
    
    return offset;
}

int get_length(void) {
    int length;
    printf("[읽을 길이]: ");
    scanf("%d", &length);
    getchar();
    
    return length;
}

char* get_data(void) {
    char temp[1000];
    char* data;

    printf("[쓸 내용]: ");
    fgets(temp, sizeof(temp), stdin);
    temp[strlen(temp) - 1] = '\0';
    data = (char *)malloc(strlen(temp) + 1);
    strcpy(data, temp);
    
    return data;
}

char* get_filename(void) {
    char temp[1000];
    char* filename;

    printf("[파일명]: ");
    fgets(temp, sizeof(temp), stdin);
    temp[strlen(temp) - 1] = '\0';
    filename = (char *)malloc(strlen(temp) + 1);
    strcpy(filename, temp);
    
    return filename;
}