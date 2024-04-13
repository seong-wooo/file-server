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

void *work(void *arg);
HashMap *parse_body(int hash_size, char *buf);
void write_flie(HashMap *body, char *response);
void read_file(HashMap *body, char *response);
void delete_file(HashMap *body, char *response);
void read_list(HashMap *body, char *response);

int cthred_pipefd;

typedef struct {
    SOCKET client_sock;
    char *data;
} Job;

typedef struct Node
{
    Job *job;
    struct Node *next;
} Node;

typedef struct Queue
{
    Node *front;
    Node *rear;
    pthread_cond_t empty_cond;
    pthread_mutex_t mutex;
} Queue;

void init_queue(Queue *queue)
{
    queue->front = NULL;
    queue->rear = NULL;
    pthread_cond_init(&queue->empty_cond, NULL);
    pthread_mutex_init(&queue->mutex, NULL);
}

void init_wthr_pool(Queue *queue, int pipefd) {
    init_queue(queue);
    
    cthred_pipefd = pipefd;
    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&threads[i], NULL, work, queue);
    }
}

void enqueue(Queue *queue, Job* job)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        printf("메모리 할당에 실패했습니다.\n");
        return;
    }
    newNode->job = job;
    newNode->next = NULL;

    pthread_mutex_lock(&queue->mutex);

    if (queue->rear == NULL)
    {
        queue->front = newNode;
        queue->rear = newNode;
    }
    else
    {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }
    
    pthread_cond_signal(&queue->empty_cond);
    pthread_mutex_unlock(&queue->mutex);
}


Job* dequeue(Queue *queue)
{
    Job *job = NULL;
    pthread_mutex_lock(&queue->mutex);
    while (queue->front == NULL)
    {
        pthread_cond_wait(&queue->empty_cond, &queue->mutex);
    }
    
    Node *temp = queue->front;
    queue->front = queue->front->next;

    if (queue->front == NULL)
    {
        queue->rear = NULL;
    }

    job = temp->job;
    free(temp);
    

    pthread_mutex_unlock(&queue->mutex);

    return job;
}

void freeQueue(Queue *queue) {
    Node *current = queue->front;
    while (current != NULL) {
        Node *temp = current;
        current = current->next;
        free(temp);
    }
    pthread_mutex_destroy(&(queue->mutex));
}

void *work(void *arg)
{   
    Queue *queue = (Queue *) arg;
    while(1) {
        Job *job = dequeue(queue);
        if (job == NULL) {
            continue;
        }
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