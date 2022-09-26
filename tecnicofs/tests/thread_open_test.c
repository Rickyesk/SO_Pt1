#include <assert.h>
#include <string.h>
#include <pthread.h>
#include "fs/operations.h"

#define THREAD_COUNT 6
#define SIZE 256

char *path = "/f1";
int fhandles[THREAD_COUNT];

void *open_20(){
    fhandles[0] = tfs_open(path, 0);
    return NULL;
}

void *open_21(){
    fhandles[1] = tfs_open(path, 0);
    return NULL;
}
void *open_22(){
    fhandles[2] = tfs_open(path, 0);
    return NULL;
}

void *open_23(){
    fhandles[3] = tfs_open(path, 0);
    return NULL;
}
void *open_24(){
    fhandles[4] = tfs_open(path, 0);
    return NULL;
}

void *open_25(){
    fhandles[5] = tfs_open(path, 0);
    return NULL;
}

int main() {
    pthread_t tid[THREAD_COUNT];

    assert(tfs_init() != -1);
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);

    for (int i = 0; i < MAX_OPEN_FILES - 2; i++) {
        tfs_open(path, 0);
    }

    assert(pthread_create(&tid[0], NULL, &open_20, NULL) != -1);
    assert(pthread_create(&tid[1], NULL, &open_21, NULL) != -1);
    assert(pthread_create(&tid[2], NULL, &open_22, NULL) != -1);
    assert(pthread_create(&tid[3], NULL, &open_23, NULL) != -1);
    assert(pthread_create(&tid[4], NULL, &open_24, NULL) != -1);
    assert(pthread_create(&tid[5], NULL, &open_25, NULL) != -1);

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(tid[i], NULL);
    }
    int count = 0;
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (fhandles[i] != -1) {
            count++;
        }
    }
    if (count == 1) {
        printf("Successful test.\n");
    }
    return 0;
}
