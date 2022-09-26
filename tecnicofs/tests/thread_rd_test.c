#include <stdio.h>
#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define COUNT 40
#define THREAD_COUNT 2
#define SIZE 1024

char *path = "/f1";
int df;

void *read_func(void* letras) {
    char input[SIZE];
    char output[SIZE];
    char letra = *((char*) letras);
    memset(input, letra, SIZE);
    tfs_read(df, output, SIZE);
    if (output[0] == 'A') {
        assert(memcmp(input, output,SIZE) == 0);
    }
    else if (output[0] == 'B') {
        assert(memcmp(input, output, SIZE) == 0);
    }
    tfs_close(df);
    return NULL;
}

int main() {
    pthread_t tid[THREAD_COUNT];
    char input[SIZE];
    int letras[THREAD_COUNT];
    letras[0] = 'A';
    letras[1] = 'B';
    assert(tfs_init() != -1);
    df = tfs_open(path, TFS_O_CREAT);
    assert(df != -1);
    memset(input, 'A', SIZE);
    assert(tfs_write(df, input, SIZE) == SIZE);
    memset(input, 'B', SIZE);
    assert(tfs_write(df, input, SIZE) == SIZE);

    assert(pthread_create(&tid[0], NULL, &read_func, (void*)&letras) != -1);
    assert(pthread_create(&tid[1], NULL, &read_func, (void*)&letras) != -1);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    df = tfs_open(path, 0);
    assert(df != -1 );

    assert(tfs_close(df) != -1);
    printf("Successful test.\n");
    return 0;
}