#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 해시 맵의 한 항목을 나타내는 구조체
typedef struct HashNode {
    char* key;
    char* value;
    struct HashNode* next; // 충돌 발생 시 체이닝을 위한 다음 노드를 가리킴
} HashNode;

// 해시 맵 자료구조
typedef struct HashMap {
    int size;
    HashNode** buckets; // 해시 버킷 배열
} HashMap;

// 해시 맵 생성
HashMap* createHashMap(int size) {
    HashMap* hashMap = (HashMap*)malloc(sizeof(HashMap));
    if (hashMap == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    hashMap->size = size;
    hashMap->buckets = (HashNode**)malloc(sizeof(HashNode*) * size);
    if (hashMap->buckets == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    // 버킷을 초기화
    for (int i = 0; i < size; ++i) {
        hashMap->buckets[i] = NULL;
    }

    return hashMap;
}

// 해시 함수: 문자열을 해시 맵의 크기에 맞게 해싱하여 버킷 인덱스를 반환
int hash(HashMap* hashMap, char* key) {
    unsigned long hashValue = 0;
    int len = strlen(key);
    for (int i = 0; i < len; ++i) {
        hashValue = (hashValue << 5) + key[i];
    }
    return hashValue % hashMap->size;
}

// 해시 맵에 새로운 키-값 쌍을 추가
void put(HashMap* hashMap, char* key, char* value) {
    int index = hash(hashMap, key);

    // 새로운 노드 생성
    HashNode* newNode = (HashNode*)malloc(sizeof(HashNode));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    newNode->key = strdup(key);
    newNode->value = strdup(value);
    newNode->next = NULL;

    // 버킷에 노드 추가
    if (hashMap->buckets[index] == NULL) {
        hashMap->buckets[index] = newNode;
    } else {
        // 체이닝: 충돌이 발생한 경우 연결 리스트에 노드 추가
        HashNode* temp = hashMap->buckets[index];
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = newNode;
    }
}

// 주어진 키에 해당하는 값을 가져옴
char* get(HashMap* hashMap, char* key) {
    int index = hash(hashMap, key);

    // 연결 리스트를 탐색하여 해당 키를 찾음
    HashNode* temp = hashMap->buckets[index];
    while (temp != NULL) {
        if (strcmp(temp->key, key) == 0) {
            return temp->value;
        }
        temp = temp->next;
    }

    return NULL; // 해당 키를 찾지 못한 경우
}

// 해시 맵의 메모리를 해제
void freeHashMap(HashMap* hashMap) {
    for (int i = 0; i < hashMap->size; ++i) {
        HashNode* node = hashMap->buckets[i];
        while (node != NULL) {
            HashNode* temp = node;
            node = node->next;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
    }
    free(hashMap->buckets);
    free(hashMap);
}