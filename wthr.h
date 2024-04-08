#include "Common.h"
#include "hash.h"
#include "log.h"
#include <dirent.h>
#include <time.h>

#define LOGNAME "FILE_SERVER"
#define BUFSIZE 10000

HashMap *_parse_body(int hash_size, char *buf);

typedef struct Job {
    SOCKET client_sock;
    char *data;
    int pipefd;
} Job;

void *process_client(void *arg)
{   
    int retval;
    int hash_size = 10;
    FILE *fp;
    Job *job = (Job *) arg;
    char response[BUFSIZE];

    HashMap *hashMap = _parse_body(hash_size, job->data);

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
        strcpy(response, "Data written successfully");
    }

    else if (*option == READ)
    {
        char *filename = get(hashMap, FILENAME);
        int offset = atoi(get(hashMap, OFFSET));
        int length = atoi(get(hashMap, LENGTH));

        fp = fopen(filename, "r");
        if (fp == NULL) {
            strcpy(response, "Error opening file");
        }
        else {
            fseek(fp, 0, SEEK_END);
            long fileSize = ftell(fp);
            if (offset > fileSize)
            {
                strcpy(response, "Offset is greater than file size");
            }
            else
            {
                fseek(fp, offset, SEEK_SET);
                length = (offset + length > fileSize) ? fileSize - offset : length;
                fread(response, sizeof(char), length, fp);
                response[length] = '\0';
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
            strcpy(response, "File deleted successfully");
        } 
        else {
            strcpy(response, "Error deleting file");
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
            strcat(response, entry->d_name);
            strcat(response, "\n");
        }

        closedir(dir);
    }
    freeHashMap(hashMap);

    write(job->pipefd, response, strlen(response));
    return NULL;
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