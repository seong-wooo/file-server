#include "Common.h"
#include "hash.h"
#include <dirent.h>

#define SERVERPORT 9000
#define BUFSIZE 10000

void _bind(SOCKET sock, int port);
void _listen(SOCKET sock, int maxconn);
SOCKET _accept(SOCKET sock, struct sockaddr_in* client_addr);
void _print_connected_client(struct sockaddr_in* client_addr);
void _print_disconnected_client(struct sockaddr_in* client_addr);
HashMap* _parse_body(int hash_size, char* buf);

int main(int argc, char const *argv[])
{
    int retval;
    
    SOCKET listen_sock = create_socket();
    _bind(listen_sock, SERVERPORT);
    _listen(listen_sock, SOMAXCONN);

    SOCKET client_sock;
    struct sockaddr_in client_addr;
    char buf[BUFSIZE + 1];
    int hash_size = 10;
    FILE* fp;

    while (1) {
        client_sock = _accept(listen_sock, &client_addr);
        _print_connected_client(&client_addr);

        while(1) {
            retval = recv(client_sock, buf, BUFSIZE, 0);
            if (retval == SOCKET_ERROR) {
                err_display("recv()");
                break;
            }
            
            else if (retval == 0) {
                break;
            }

            buf[retval] = '\0';
            HashMap* hashMap = _parse_body(hash_size, buf);

            char* option = get(hashMap, OPTION);
            if (*option == WRITE) {
                char* filename = get(hashMap, FILENAME);
                char* offset = get(hashMap, OFFSET);
                char* data = get(hashMap, DATA);
                
                fp = fopen(filename, "r+");
                if (fp == NULL) {
                    fp = fopen(filename, "w");
                    if (fp == NULL) {
                        perror("Error opening file");
                    }
                } else {
                    fseek(fp, atoi(offset), SEEK_SET);                
                }
                
                fwrite(data, sizeof(char), strlen(data), fp);
                fclose(fp);
                strcpy(buf, "Data written successfully");
            }

            else if (*option == READ) {
                char* filename = get(hashMap, FILENAME);
                char* offset = get(hashMap, OFFSET);
                char* length = get(hashMap, LENGTH);

                fp = fopen(filename, "r");
                if (fp == NULL) {
                    perror("Error opening file");
                }
                
                fseek(fp, atoi(offset), SEEK_SET);
                size_t result = fread(buf, 1, strtoul(length, NULL, 10), fp);
                buf[result] = '\0';
                fclose(fp);
            }

            else if (*option == DELETE) {
                if (unlink(get(hashMap, FILENAME)) == 0) {
                    strcpy(buf, "File deleted successfully");
                } else {
                    strcpy(buf, "Error deleting file");
                }
            }


            else if (*option == LIST) {
                DIR *dir;
                struct dirent *entry;

                dir = opendir(".");
                if (dir == NULL) {
                    perror("Error opening dir");
                }

                while ((entry = readdir(dir)) != NULL) {
                    strcat(buf, entry->d_name);
                    strcat(buf, "\n");
                }
                
                closedir(dir);
            }

            retval = send(client_sock, buf, BUFSIZE, 0);
            if (retval == SOCKET_ERROR) {
                err_display("send()");
                break;
            }

            freeHashMap(hashMap);
        }

        close(client_sock);
        _print_disconnected_client(&client_addr);
    }

    close(listen_sock);
    printf("[TCP 서버] 서버 종료\n");
    
    return 0;
}


void _bind(SOCKET sock, int port) {
    int retval;
    struct sockaddr_in serveraddr;

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    
    retval = bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR) err_quit("bind()");
}

void _listen(SOCKET sock, int maxconn) {
    int retval;

    retval = listen(sock, maxconn);
    if (retval == SOCKET_ERROR) {
        err_quit("listen()");
    }
}

SOCKET _accept(SOCKET sock, struct sockaddr_in* client_addr) {
    SOCKET client_sock;
    int addrlen = sizeof(client_addr);

    client_sock = accept(sock, (struct sockaddr *) client_addr, &addrlen);
    if (client_sock == INVALID_SOCKET) {
        err_display("accept()");
    }
    
    
    return client_sock;
}

HashMap* _parse_body(int hash_size, char* buf) {
    HashMap* hashMap = createHashMap(hash_size);
    char* key;
    char* value;

    char* str = strtok(buf, TOKEN_PARSER);
    while (str != NULL) {
        key = str;
        str = strtok(NULL, LINE_PARSER);
        value = str;
        str = strtok(NULL, TOKEN_PARSER);
        put(hashMap, key, value);
    }

    return hashMap;
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