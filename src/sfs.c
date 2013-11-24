#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sfs.h"

#define BLOCK_SIZE 512

char *FS = NULL;
int FS_SIZE = 0;

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
    if(err)
    {
        return err;
    }
    return umap_fs();
}

int check_mount()
{
    return FS != NULL ? STATUS_OK : STATUS_NOT_MOUNT;
}

int umap_fs()
{
    msync(FS, FS_SIZE, MS_SYNC);

    if (munmap(FS, FS_SIZE) == -1) {
        return STATUS_ERR;
    }
    FS = NULL;
    return STATUS_OK;
}

int map_fs(char *path)
{
    // try to open the file
    int fd = open(path, O_RDWR | O_CREAT, (mode_t)0600);
    if (fd == -1)
    {
        return STATUS_ERR;
    }

    // get file size
    FS_SIZE = lseek(fd, 0L, SEEK_END);

    FS = mmap(0, FS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int exit_status = STATUS_OK;
    if (FS == MAP_FAILED) {
        exit_status = STATUS_ERR;
    }
    close(fd);
    return exit_status;
}

