#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int tfs_init() {
    state_init();
    /* create root inode */ 
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    /* Verifies the initialization of the mutexs */
    if(pthread_mutex_init(&lock_open_file_table, NULL) != 0 || pthread_mutex_init(&lock_inodes, NULL) != 0 || pthread_mutex_init(&lock_blocks, NULL) != 0){
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    /* Destroy the mutexs */
    pthread_mutex_destroy(&lock_open_file_table);
    pthread_mutex_destroy(&lock_blocks);
    pthread_mutex_destroy(&lock_inodes);
    state_destroy();
    return 0;
}

int* block_fetcher(inode_t *inode, size_t iBlock) {
    int* block;
    /* Gets direct block */
    if (iBlock < 10) {
        block = data_block_get(inode->i_data_block[iBlock]);
    }
    /* Gets indirect block */
    else {
        size_t new_iblock = iBlock - 10; // index of the block of reference 
        int *point_block = data_block_get(inode->i_data_block[10]);
        block = data_block_get(point_block[new_iblock]);
    }

    return block;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }
    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }
    inum = tfs_lookup(name);
    if (inum >= 0) {
        pthread_mutex_lock(&lock_inodes);
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            pthread_mutex_unlock(&lock_inodes);
            return -1;
        }
        pthread_rwlock_wrlock(&inode->rwLock);
        pthread_mutex_unlock(&lock_inodes);
        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                while (inode->i_size > 0) {
                    if (data_block_free(*block_fetcher(inode, inode->i_size / BLOCK_SIZE)) == -1) {
                        pthread_rwlock_unlock(&inode->rwLock);
                        return -1;
                    }
                    if (inode->i_size > BLOCK_SIZE) {
                        inode->i_size -= BLOCK_SIZE;
                    }
                    else {
                        inode->i_size = 0;
                    }
                }
            }
            pthread_rwlock_unlock(&inode->rwLock);
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
            pthread_rwlock_unlock(&inode->rwLock);
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }

        /*Inicialize read lock on read/write lock*/
        if (pthread_rwlock_init(&inode_get(inum)->rwLock, NULL) != 0) {
            return -1;
        }

        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}

int tfs_close(int fhandle) { 
    return remove_from_open_file_table(fhandle);
}

int block_allocator(inode_t *inode, size_t iBlock) {
    int *block;
    size_t new_iblock = iBlock - 10;
    /* If empty file or in a new Block, allocate new block */
    if(iBlock < 10){
        inode->i_data_block[iBlock] = data_block_alloc();
    }
    /*Allocate indirect block and first new indirect block*/
    else {
        if(iBlock == 10) {
            inode->i_data_block[iBlock] = data_block_alloc();
        }
        block = (int*)data_block_get(inode->i_data_block[10]);
        block[new_iblock] = data_block_alloc();
    }

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    pthread_mutex_lock(&lock_open_file_table);
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    pthread_mutex_lock(&file->file_table_lock);
    pthread_mutex_unlock(&lock_open_file_table);
    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    size_t aux_to_write = to_write;
    pthread_rwlock_wrlock(&inode->rwLock);
    while (aux_to_write > 0) {
        size_t iBlock = file->of_offset / BLOCK_SIZE;
        int *block = NULL;

        /* If the offset is pointing to the end of the block */
        if (file->of_offset % BLOCK_SIZE == 0) {
            pthread_mutex_lock(&lock_inodes);
            block_allocator(inode, iBlock);
            pthread_mutex_unlock(&lock_inodes);
        }
        block = block_fetcher(inode, iBlock);
        if (block == NULL) {
            return -1;
        }

        /* If its to write in more than 1 block */
        if (aux_to_write + (file->of_offset % BLOCK_SIZE) > BLOCK_SIZE) {
            size_t written = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
            memcpy((void*)block + (file->of_offset % BLOCK_SIZE), buffer, written);
            file->of_offset += written;
            if (file->of_offset > inode->i_size) {
                inode->i_size = file->of_offset;
            }
            aux_to_write -= written;
        }
        /* If its to write in 1 block */
        else {
            memcpy((void*)block + (file->of_offset % BLOCK_SIZE), buffer, aux_to_write);
            file->of_offset += aux_to_write;
            if (file->of_offset > inode->i_size) {
                inode->i_size = file->of_offset;
            }
            aux_to_write = 0;
        }
    }
    pthread_rwlock_unlock(&inode->rwLock);
    pthread_mutex_unlock(&file->file_table_lock);
    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t to_read) {
    pthread_mutex_lock(&lock_open_file_table);
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    pthread_mutex_lock(&file->file_table_lock);
    pthread_mutex_unlock(&lock_open_file_table);
    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    pthread_rwlock_rdlock(&inode->rwLock);
    /* Determine how many bytes to read */
    size_t aux_to_read = inode->i_size - file->of_offset;
    if (to_read < aux_to_read) {
        aux_to_read = to_read;
    }
    size_t total_read = aux_to_read;
    while (aux_to_read > 0) {
        size_t iBlock = file->of_offset / BLOCK_SIZE;
        int *block = NULL;
        block = block_fetcher(inode, iBlock);
        if (block == NULL) {
            return -1;
        }
        if ((file->of_offset % BLOCK_SIZE) + aux_to_read > BLOCK_SIZE) {
            size_t read = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
            memcpy(buffer, (void*)block + (file->of_offset % BLOCK_SIZE), read);
            file->of_offset += read;
            aux_to_read -= read;
        }
        else {
            /* Perform the actual read */
            memcpy(buffer, (void*)block + (file->of_offset % BLOCK_SIZE), aux_to_read);
            /* The offset associated with the file handle is
            * incremented accordingly */
            file->of_offset += aux_to_read;
            aux_to_read = 0;
        }

    }
    pthread_rwlock_unlock(&inode->rwLock);
    pthread_mutex_unlock(&file->file_table_lock);
    return (ssize_t)total_read;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    int fhandle = tfs_open(source_path, 0);
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    char buffer[BLOCK_SIZE + 1];
    memset(buffer, 0, sizeof(buffer));

    /* In case the file doesn't exist it will create one */
    FILE* df = fopen(dest_path, "w");
    if (df == NULL) {
        return -1;
    }

    size_t aux_to_read = inode->i_size;
    while (aux_to_read > 0) {
        if (aux_to_read > BLOCK_SIZE) {
            if (tfs_read(fhandle, buffer, BLOCK_SIZE) != BLOCK_SIZE) {
                return -1;
            }
            fwrite(buffer, 1, BLOCK_SIZE, df);
            aux_to_read -= BLOCK_SIZE;
        }
        else {
            if (tfs_read(fhandle, buffer, aux_to_read) != aux_to_read) {
                return -1;
            }
            fwrite(buffer, 1, aux_to_read, df);
            aux_to_read = 0;
        }

    }
    tfs_close(fhandle);
    fclose(df);
    return 0;
}
