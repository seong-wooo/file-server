#include "Common.h"
#include "hash.h"
#include "log.h"
#include <dirent.h>
#include <time.h>
#include <pthread.h>

#define BUFSIZE 10000
#define MAX_THREADS 8
#define MAX_QUEUE_SIZE 8
#define HASH_SIZE 10

void *process_client(void *arg);
HashMap *parse_body(int hash_size, char *buf);
void write_flie(HashMap *body, char *response);
void read_file(HashMap *body, char *response);
void delete_file(HashMap *body, char *response);
void read_list(HashMap *body, char *response);

typedef struct {
    SOCKET client_sock;
    char *data;
} Job;

typedef struct {
    Job queue[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t empty_cond;
    pthread_cond_t full_cond;
} Queue;

int cthred_pipefd;

void init_wthr_pool(Queue *queue, int pipefd) {
    cthred_pipefd = pipefd;    
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->empty_cond, NULL);
    pthread_cond_init(&queue->full_cond, NULL);

    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&threads[i], NULL, process_client, queue);
    }
}

bool is_queue_empty(Queue *queue) {
    return (queue->size == 0);
}

bool is_queue_full(Queue *queue) {
    return (queue->size == MAX_QUEUE_SIZE);
}

void enqueue(Queue *queue, Job job) {
    pthread_mutex_lock(&queue->lock);
    while (is_queue_full(queue)) {
        pthread_cond_wait(&queue->full_cond, &queue->lock);
    }
    
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    queue->queue[queue->rear] = job;
    queue->size++;

    pthread_cond_signal(&queue->empty_cond);
    pthread_mutex_unlock(&queue->lock);
}

Job* dequeue(Queue *queue) {
    pthread_mutex_lock(&queue->lock);
    while (is_queue_empty(queue)) {
        pthread_cond_wait(&queue->empty_cond, &queue->lock);
    }
    
    Job *job = &queue->queue[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->size--;

    pthread_cond_signal(&queue->full_cond);
    pthread_mutex_unlock(&queue->lock);

    return job;
}

void *process_client(void *arg)
{   
    Queue *queue = (Queue *) arg;
    while(1) {
        Job *job = dequeue(queue);
        char response[BUFSIZE];

        HashMap *body = parse_body(HASH_SIZE, job->data);

        char *option = get(body, OPTION);
        switch (*option)
        {
        case WRITE:
            write_flie(body, response);
            break;
        case READ:
            read_file(body, response);
            break;
        case DELETE:
            delete_file(body, response);
            break;

        case LIST:
            read_list(body, response);
            break;

        default:
            printf("잘못된 옵션입니다.\n");
            break;
        }
        freeHashMap(body);
        write(cthred_pipefd, response, BUFSIZE);
    }
};

HashMap *parse_body(int hash_size, char *buf)
{
    HashMap *body = createHashMap(hash_size);
    char *key;
    char *value;

    char *str = strtok(buf, TOKEN_PARSER);
    while (str != NULL)
    {
        key = str;
        str = strtok(NULL, LINE_PARSER);
        value = str;
        str = strtok(NULL, TOKEN_PARSER);
        put(body, key, value);
    }

    return body;
}

void write_flie(HashMap *body, char *response)
{
    char *filename = get(body, FILENAME);
    int offset = atoi(get(body, OFFSET));
    char *data = get(body, DATA);

    FILE *fp = fopen(filename, "rb+");
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

    // 내가 원하는 만큼 저장했나? 확인
    // 예외 처리
    // 시스템 콜을 사용하면서 신경썻떤것 메모하기
    fwrite(data, sizeof(char), strlen(data), fp);

    char logMessage[BUFSIZE];
    snprintf(logMessage, BUFSIZE, "option=%c, filename=%s, offset=%d, data=%s", WRITE, filename, offset, data);
    write_log(LOG_INFO, logMessage);

    fclose(fp);
    strcpy(response, "Data written successfully");
}

void read_file(HashMap *body, char *response)
{
    char *filename = get(body, FILENAME);
    int offset = atoi(get(body, OFFSET));
    int length = atoi(get(body, LENGTH));

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        strcpy(response, "Error opening file");
    }
    else
    {
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

void delete_file(HashMap *body, char *response)
{
    char *filename = get(body, FILENAME);
    if (unlink(filename) == 0)
    {
        char logMessage[BUFSIZE];
        snprintf(logMessage, BUFSIZE, "option=%c, filename=%s", DELETE, filename);
        write_log(LOG_INFO, logMessage);
        strcpy(response, "File deleted successfully");
    }
    else
    {
        strcpy(response, "Error deleting file");
    }
}

void read_list(HashMap *body, char *response)
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
    strcat(response, "\0");

    closedir(dir);
}