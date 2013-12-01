#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include "sfs.h"

#define BLOCK_SIZE 512
#define DESCRIPTORS_PART 0.05
#define DIR_TYPE 1
#define FILE_TYPE 2
#define LINK_TYPE 3

#define MASK ((uint8_t *) (char *)FS + FS->mask_offset)
#define DESCR_TABLE ((descr_struct *) ((char *)FS + FS->descr_table_offset))

typedef struct {
    int type;
    int links_num;
    int blocks_num;
    char *blocks;
} descr_struct;

typedef struct {
    int block_size;
    int blocks_num;
    int size;
    // int mask_size;
    int mask_offset;
    int max_files;
    int descr_table_offset;
} fs_struct;

fs_struct *FS = NULL;

/* Forward declarations. */
int map_fs(char *path);
int umap_fs();
int check_mount();


int mount(char *path)
{
    return map_fs(path);
}

int umount()
{
    int err = check_mount();
    if (err)
        return err;
    return umap_fs();
}

int check_mount()
{
    if (FS == NULL)
    {
        fprintf(stderr, "error: file system not mounted\n");
        return STATUS_NOT_MOUNT;
    } else {
        return STATUS_OK;
    }
}

int umap_fs()
{
    msync(FS, FS->size, MS_SYNC);

    if (munmap(FS, FS->size) == -1)
        return STATUS_ERR;
    FS = NULL;
    return STATUS_OK;
}

int map_fs(char *path)
{
    // check file exists
    if (access(path, F_OK) == -1)
        return STATUS_ERR;

    // try to open the file
    int fd = open(path, O_RDWR | O_CREAT, (mode_t)0600);
    if (fd == -1)
    {
        close(fd);
        return STATUS_ERR;
    }

    int fs_size = lseek(fd, 0L, SEEK_END);

    FS = mmap(0, fs_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int exit_status = STATUS_OK;
    if (FS == MAP_FAILED)
    {
        FS = NULL;
        exit_status = STATUS_ERR;
    }
    close(fd);
    return exit_status;
}

int dump_stats()
{
    int err = check_mount();
    if (err)
        return err;

    printf("FS size: %d\n", FS->size);
    printf("block size: %d\n", FS->block_size);
    printf("blocks num: %d\n", FS->blocks_num);
    printf("max files: %d\n", FS->max_files);
    printf("mask offset: %d\n", FS->mask_offset);
    printf("descriptor table offset: %d\n", FS->descr_table_offset);
    return STATUS_OK;
}

void mask_block(int num)
{
    int i = num / 8;
    MASK[i] |= 1 << (num % 8);
}

void umask_block(int num)
{
    int i = num / 8;
    MASK[i] &= ~(1 << (num % 8));
}

bool check_block(int num)
{
    int i = num / 8;
    return MASK[i] & (1 << (num % 8));
}

int mkfs(char *path)
{
    int err = map_fs(path);
    if (err)
        return err;

    int fd = open(path, O_RDWR | O_CREAT, (mode_t)0600);

    // get file size
    FS->size = lseek(fd, 0L, SEEK_END);
    close(fd);

    FS->block_size = BLOCK_SIZE;
    FS->blocks_num = FS->size / FS->block_size;

    int mask_size = ceil((float) FS->blocks_num / 8.0);
    int mask_blocks_num = ceil((float) mask_size / FS->block_size);

    FS->mask_offset = FS->block_size;
    FS->max_files = ceil(FS->size / sizeof(descr_struct) * DESCRIPTORS_PART);
    FS->descr_table_offset = FS->mask_offset + mask_blocks_num * FS->block_size;
    int descr_table_blocks_num = ceil((float) FS->max_files * sizeof(descr_struct) / FS->block_size);

    // mark all block as free
    for (int i = 0; i < FS->blocks_num; ++i)
    {
        umask_block(i);
    }


    // first block for info
    mask_block(0);

    for (int i = 1; i < mask_blocks_num + 1; ++i)
        mask_block(i);

    // mark fake blocks as busy
    int blocks_in_mask = mask_blocks_num * FS->block_size * 8;
    for (int i = FS->blocks_num; i < blocks_in_mask; ++i)
    {
        mask_block(i);
    }

    // descriptors table blocks
    int descr_table_first_block = mask_blocks_num + 1;
    int descr_table_last_block = descr_table_first_block + descr_table_blocks_num;
    for (int i = descr_table_first_block; i < descr_table_last_block; ++i)
    {
        mask_block(i);
    }

    // all files are free
    for (int i = 0; i < FS->max_files; ++i)
    {
        descr_struct *descr = DESCR_TABLE + i;
        descr->type = 0;
        descr->links_num = 0;
        descr->blocks_num = 0;
        descr->blocks = NULL;
    }

    dump_stats();
    return umap_fs();
}
