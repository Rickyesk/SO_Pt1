#include <stdio.h>
#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define THREAD_COUNT 2
#define SIZE 1024

char *path = "/f1";
int fd;

void *write_func(void *letras){
    char input[SIZE];
    char letra = *((char*)letras); 
    memset(input, letra, SIZE);
    tfs_write(fd, input, SIZE);
    return NULL;
}

int main() {
    pthread_t tid[THREAD_COUNT];
    assert(tfs_init() != -1);
    fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    char inputA[SIZE];
    memset(inputA, 'A', SIZE);
    char inputB[SIZE];
    memset(inputB, 'B', SIZE); 
    char output [SIZE];
    int letras[THREAD_COUNT];

    for(int i = 0; i < THREAD_COUNT; i++){
        letras[i] = 'A' + i;
    }
    for (int i = 0; i < THREAD_COUNT; i++){
        assert(pthread_create(&tid[i], NULL, write_func, (void*)&letras[i])!=-1);
    }

    for (int i = 0; i < THREAD_COUNT; i++){
        pthread_join (tid[i], NULL);
    }

    assert(tfs_close(fd) != -1);

    fd = tfs_open(path, 0);
    assert(fd != -1 );
    tfs_copy_to_external_fs(path, "out_wr_test");
    for(int i = 0; i < THREAD_COUNT; i++){
        assert(tfs_read(fd, output, SIZE) == SIZE);
        if(output[0] == 'A'){
            assert(memcmp(inputA, output, SIZE) == 0);
        }
        else if(output[0] == 'B'){
            assert(memcmp(inputB, output, SIZE) == 0);
        }
    }
    assert(tfs_close(fd) != -1);
    printf("Successful test.\n");
    return 0;
}