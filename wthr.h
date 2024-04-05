#include "Common.h"
#include "hash.h"
#include "log.h"
#include <dirent.h>
#include <time.h>

#define BUFSIZE 10000
#define LOGNAME "FILE_SERVER"

HashMap *_parse_body(int hash_size, char *buf);

void *process_client(void *arg)
{
    int retval;
    char buf[BUFSIZE + 1];
    int hash_size = 10;
    FILE *fp;
    SOCKET client_sock = (SOCKET)(long long)arg;
    while (1)
    {
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

        buf[retval] = '\0';
        HashMap *hashMap = _parse_body(hash_size, buf);

        char *option = get(hashMap, OPTION);
        if (*option == WRITE)
        {
            char *filename = get(hashMap, FILENAME);
            int offset = atoi(get(hashMap, OFFSET));
            char *data = get(hashMap, DATA);

            fp = fopen(filename, "rb+");
            if (fp == NULL)
            {
                fp = fopen(filename, "wb");
                if (fp == NULL)
                {
                    perror("Error opening file");
                }
                offset = 0;
            }

            fseek(fp, 0, SEEK_END);
            long fileSize = ftell(fp);
            if (offset > fileSize)
            {
                offset = fileSize;
            }

            fseek(fp, offset, SEEK_SET);
            fwrite(data, sizeof(char), strlen(data), fp);

            char logMessage[BUFSIZE];
            snprintf(logMessage, BUFSIZE, "option=%c, filename=%s, offset=%d, data=%s", WRITE, filename, offset, data);
            write_log(LOG_INFO, logMessage);

            fclose(fp);
            strcpy(buf, "Data written successfully");
        }

        else if (*option == READ)
        {
            char *filename = get(hashMap, FILENAME);
            int offset = atoi(get(hashMap, OFFSET));
            int length = atoi(get(hashMap, LENGTH));

            fp = fopen(filename, "r");
            if (fp == NULL) {
                strcpy(buf, "Error opening file");
            }
            else {
                fseek(fp, 0, SEEK_END);
                long fileSize = ftell(fp);
                if (offset > fileSize)
                {
                    strcpy(buf, "Offset is greater than file size");
                }
                else
                {
                    fseek(fp, offset, SEEK_SET);
                    length = (offset + length > fileSize) ? fileSize - offset : length;
                    fread(buf, sizeof(char), length, fp);
                    buf[length] = '\0';
                }
                fclose(fp);
            }
        }

        else if (*option == DELETE)
        {
            char *filename = get(hashMap, FILENAME);
            if (unlink(filename) == 0) {
                char logMessage[BUFSIZE];
                snprintf(logMessage, BUFSIZE, "option=%c, filename=%s", DELETE, filename);
                write_log(LOG_INFO, logMessage);
                strcpy(buf, "File deleted successfully");
            } 
            else {
                strcpy(buf, "Error deleting file");
            }
        }
        else if (*option == LIST)
        {
            DIR *dir;
            struct dirent *entry;

            dir = opendir(".");
            if (dir == NULL)
            {
                perror("Error opening dir");
            }

            while ((entry = readdir(dir)) != NULL)
            {
                strcat(buf, entry->d_name);
                strcat(buf, "\n");
            }

            closedir(dir);
        }

        retval = send(client_sock, buf, BUFSIZE, 0);
        if (retval == SOCKET_ERROR)
        {
            err_display("send()");
            break;
        }

        freeHashMap(hashMap);
    }
    close(client_sock);
    return 0;
};

HashMap *_parse_body(int hash_size, char *buf)
{
    HashMap *hashMap = createHashMap(hash_size);
    char *key;
    char *value;

    char *str = strtok(buf, TOKEN_PARSER);
    while (str != NULL)
    {
        key = str;
        str = strtok(NULL, LINE_PARSER);
        value = str;
        str = strtok(NULL, TOKEN_PARSER);
        put(hashMap, key, value);
    }

    return hashMap;
}