#include "Common.h"
#include "hash.h"
#include "log.h"
#include <dirent.h>
#include <time.h>
#include <pthread.h>

#define LOGNAME "FILE_SERVER"
#define BUFSIZE 10000
#define MAX_THREADS 8
#define MAX_QUEUE_SIZE 8

void *process_client(void *arg);
HashMap *_parse_body(int hash_size, char *buf);

typedef struct {
    SOCKET client_sock;
    char *data;
    int pipefd;
} Job;

typedef struct {
    Job jobs[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t empty_cond;
    pthread_cond_t full_cond;
} WTHR_QUEUE;

void init_wthr_pool(WTHR_QUEUE *jobs) {
    jobs->front = 0;
    jobs->rear = -1;
    jobs->size = 0;
    pthread_mutex_init(&jobs->lock, NULL);
    pthread_cond_init(&jobs->empty_cond, NULL);
    pthread_cond_init(&jobs->full_cond, NULL);

    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&threads[i], NULL, process_client, jobs);
    }
}

bool is_queue_empty(WTHR_QUEUE *jobs) {
    return (jobs->size == 0);
}

bool is_queue_full(WTHR_QUEUE *jobs) {
    return (jobs->size == MAX_QUEUE_SIZE);
}

void enqueue(WTHR_QUEUE *jobs, Job job) {
    pthread_mutex_lock(&jobs->lock);
    while (is_queue_full(jobs)) {
        pthread_cond_wait(&jobs->full_cond, &jobs->lock);
    }
    
    jobs->rear = (jobs->rear + 1) % MAX_QUEUE_SIZE;
    jobs->jobs[jobs->rear] = job;
    jobs->size++;

    pthread_cond_signal(&jobs->empty_cond);
    pthread_mutex_unlock(&jobs->lock);
}

Job* dequeue(WTHR_QUEUE *queue) {
    pthread_mutex_lock(&queue->lock);
    while (is_queue_empty(queue)) {
        pthread_cond_wait(&queue->empty_cond, &queue->lock);
    }
    
    Job *job = &queue->jobs[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->size--;

    pthread_cond_signal(&queue->full_cond);
    pthread_mutex_unlock(&queue->lock);

    return job;
}

void *process_client(void *arg)
{   
    WTHR_QUEUE *jobs = (WTHR_QUEUE *) arg;

    int retval;
    int hash_size = 10;
    FILE *fp;
    Job *job;

    while(1) {
        job = dequeue(jobs);
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
    }
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