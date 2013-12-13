#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "sfs.h"

#define BLOCK_SIZE 512
#define DESCRIPTORS_PART 0.05
#define DIR_TYPE 1
#define FILE_TYPE 2
#define LINK_TYPE 3
#define FILENAME_SIZE 20
#define FIDS_NUM 512
#define MAX_PATH_SIZE 512

#define MASK ((uint8_t *) (char *)FS + FS->mask_offset)
#define DESCR_TABLE ((descr_struct *) ((char *)FS + FS->descr_table_offset))
#define BLOCKS(ID) ((void *)((char *)FS + ID * FS->block_size))

#define SPACE_LEFT(descr) (ceil((float) descr->size / FS->block_size) * FS->block_size - descr->size)
#define BLOCKS_NUM(descr) ((int) ceil((float) descr->size / FS->block_size))
#define FILES_IN_BLOCK (FS->block_size / sizeof(file_struct))
#define FILES_NUM(descr) ((int) (descr->size / sizeof(file_struct)))

typedef struct {
    int id;
    int type;
    int links_num;
    int size;
    int blocks_id;
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

typedef struct {
    char filename[FILENAME_SIZE];
    int descr_id;
} file_struct;

fs_struct *FS = NULL;
int FIDS[FIDS_NUM] = {[0 ... FIDS_NUM - 1] = -1};
char WORK_DIR[MAX_PATH_SIZE];

/* Forward declarations. */
int map_fs(char *path);
int umap_fs();
int check_mount();
descr_struct *lookup_link(char *path);
descr_struct *lookup_full(char *path);
char *read_symlink(descr_struct *link);


int mount(char *path)
{
    int err = map_fs(path);
    if (err)
        return err;
    strcpy(WORK_DIR, "/");
    return STATUS_OK;
}

int umount()
{
    int err = check_mount();
    strcpy(WORK_DIR, "");
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

descr_struct *find_descr()
{
    for (int i = 0; i < FS->max_files; ++i)
    {
        descr_struct *descr = DESCR_TABLE + i;
        if (descr->type == 0)
            return descr;
    }
    return NULL;
}

int find_block()
{
    for (int i = 0; i < FS->blocks_num; ++i)
    {
        if(!check_block(i))
            return i;
    }
    return -1;
}

char *get_filename(char *path)
{

    int last_delim = -1;

    for (int i = 0; i < strlen(path); ++i)
    {
        if (path[i] == '/')
            last_delim = i;
    }
    char *filename = path + last_delim + 1;
    return filename;
}

char *get_dir_path(char *path)
{
    int last_delim = -1;

    for (int i = 0; i < strlen(path); ++i)
    {
        if (path[i] == '/')
            last_delim = i;
    }
    // for root case
    if (last_delim == 0)
        last_delim = 1;
    char *dir_path = malloc(last_delim + 1);
    strncpy(dir_path, path, last_delim);
    dir_path[last_delim] = '\0';
    return dir_path;
}

descr_struct *lookup(char *path, bool follow_symlinks)
{
    if (strcmp(path, "/") == 0)
    {
        return DESCR_TABLE + 0;
    }
    char *filename = get_filename(path);
    char *dir_path = get_dir_path(path);
    descr_struct *dir = lookup_full(dir_path);
    free(dir_path);
    if (dir == NULL)
        return NULL;
    int *blocks = BLOCKS(dir->blocks_id);
    file_struct *files = BLOCKS(blocks[0]);
    int bf_id = 0;
    int block_id = 0;
    for (int f_id = 0; f_id < FILES_NUM(dir); ++f_id)
    {
        file_struct *file = files + bf_id;
        if(strcmp(file->filename, filename) == 0)
        {
            descr_struct *file_descr = DESCR_TABLE + file->descr_id;
            if (follow_symlinks && file_descr->type == LINK_TYPE)
            {
                char *link_path = read_symlink(file_descr);
                return lookup_full(link_path);
            } else {
                return file_descr;
            }
        }
        bf_id++;
        if (bf_id == FILES_IN_BLOCK)
        {
            bf_id = 0;
            block_id++;
            files = BLOCKS(blocks[block_id]);
        }
    }
    return NULL;

}

descr_struct *lookup_link(char *path)
{
    return lookup(path, false);
}

descr_struct *lookup_full(char *path)
{
    return lookup(path, true);
}

int add_to_dir(descr_struct *dir, descr_struct *file, char *filename)
{
    int left = SPACE_LEFT(dir);
    file_struct *new_file;
    int *blocks = BLOCKS(dir->blocks_id);
    if (left >= sizeof(file_struct))
    {
        int block_id = blocks[BLOCKS_NUM(dir) - 1];
        new_file = (file_struct *)((char *) BLOCKS(block_id) + FS->block_size - left);
    } else {
        int new_block_id = find_block();
        if (new_block_id == -1)
            return STATUS_NO_SPACE_LEFT;
        mask_block(new_block_id);
        blocks[BLOCKS_NUM(dir)] = new_block_id;
        dir->size += left;
        new_file = BLOCKS(new_block_id);
    }
    dir->size += sizeof(file_struct);
    strcpy(new_file->filename, filename);
    new_file->descr_id = file->id;
    return STATUS_OK;
}

int rm_from_dir(descr_struct *dir, char *filename)
{
    int *blocks = BLOCKS(dir->blocks_id);
    file_struct *files = BLOCKS(blocks[0]);
    int bf_id = 0;
    int block_id = 0;
    file_struct *del_file;
    for (int f_id = 0; f_id < FILES_NUM(dir); ++f_id)
    {
        if (strcmp(files[bf_id].filename, filename) == 0)
        {
            del_file = files + bf_id;
            break;
        }
        bf_id++;
        if (bf_id == FILES_IN_BLOCK)
        {
            bf_id = 0;
            block_id++;
            files = BLOCKS(blocks[block_id]);
        }
    }
    if (del_file == NULL)
        return STATUS_NOT_FOUND;
    int last_block_id = blocks[BLOCKS_NUM(dir) - 1];
    int last_file_id = FILES_NUM(dir) % FILES_IN_BLOCK - 1;
    file_struct *last_file = (file_struct *) BLOCKS(last_block_id) + last_file_id;

    // copy last file on the place of deleted file
    strncpy(del_file->filename, last_file->filename, FILENAME_SIZE);
    del_file->descr_id = last_file->descr_id;

    strncpy(last_file->filename, "", FILENAME_SIZE);
    last_file->descr_id = 0;

    int old_blocks_num = BLOCKS_NUM(dir);
    dir->size -= sizeof(file_struct);
    if (old_blocks_num - BLOCKS_NUM(dir) != 0)
    {
        blocks[last_block_id] = 0;
        umask_block(last_block_id);
    }

    // If it was last file in dir
    if (dir->size == 0)
    {
        strncpy(del_file->filename, "", FILENAME_SIZE);
        del_file->descr_id = 0;
        umask_block(blocks[0]);
        blocks[0] = 0;
        return STATUS_OK;
    }
    return STATUS_OK;
}

int rm_descr(descr_struct *descr)
{
    int *blocks = BLOCKS(descr->blocks_id);
    for (int i = 0; i < BLOCKS_NUM(descr); ++i)
    {
        umask_block(blocks[i]);
    }
    umask_block(descr->blocks_id);
    descr->type = 0;
    descr->size = 0;
    descr->blocks_id = 0;
    descr->links_num = 0;
    return STATUS_OK;
}

int check_fid(int fid)
{
    if (fid >= FIDS_NUM || fid < 0)
        return STATUS_NOT_FOUND;
    if (FIDS[fid] == -1)
        return STATUS_NOT_FOUND;
    return STATUS_OK;
}

int create_fid(descr_struct *file)
{
    for (int fid = 0; fid < FIDS_NUM; ++fid)
    {
        if (FIDS[fid] == -1)
        {
            FIDS[fid] = file->id;
            return fid;
        }
    }
    return -1;
}

int rm_fid(int fid)
{
    int err = check_fid(fid);
    if (err)
        return err;
    FIDS[fid] = -1;
    return STATUS_OK;
}

int list(char *path_arg)
{
    int err = check_mount();
    if (err)
        return err;

    char *path = abs_path(path_arg);
    descr_struct *dir = lookup_full(path);
    if (dir == NULL)
    {
        free(path);
        return STATUS_NOT_FOUND;
    }
    if (dir->type != DIR_TYPE)
    {
        printf("%s\n", path);
        free(path);
        return STATUS_OK;
    }
    free(path);
    int *blocks = BLOCKS(dir->blocks_id);
    file_struct *files = BLOCKS(blocks[0]);
    int bf_id = 0;
    int block_id = 0;
    for (int f_id = 0; f_id < FILES_NUM(dir); ++f_id)
    {
        file_struct *file = files + bf_id;
        descr_struct *file_descr = DESCR_TABLE + file->descr_id;
        if (file_descr->type == FILE_TYPE)
            printf( "%s \t\t id:%d\n", file->filename, file->descr_id);
        if (file_descr->type == DIR_TYPE)
            printf( "%s/ \t\t id:%d\n", file->filename, file->descr_id);
        if (file_descr->type == LINK_TYPE)
        {
            char *link_path = read_symlink(file_descr);
            printf("%s@ -> %s \t id:%d\n", file->filename, link_path, file->descr_id);
            free(link_path);
        }
        bf_id++;
        if (bf_id == FILES_IN_BLOCK)
        {
            bf_id = 0;
            block_id++;
            files = BLOCKS(blocks[block_id]);
        }
    }
    return STATUS_OK;
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

    // create root dir
    descr_struct *root = DESCR_TABLE + 0;
    root->id = 0;
    root->type = DIR_TYPE;
    root->links_num = 1;
    root->size = 0;
    int block_num = find_block();
    if (block_num == -1)
    {
        umap_fs();
        return STATUS_NO_SPACE_LEFT;
    }
    mask_block(block_num);
    root->blocks_id = block_num;
    add_to_dir(root, root, ".");
    add_to_dir(root, root, "..");

    // all files are free
    for (int i = 1; i < FS->max_files; ++i)
    {
        descr_struct *descr = DESCR_TABLE + i;
        descr->id = i;
        descr->type = 0;
        descr->links_num = 0;
        descr->size = 0;
        descr->blocks_id = 0;
    }

    dump_stats();
    return umap_fs();
}

int create(char *path_arg, int type)
{
    int err = check_mount();
    if (err)
        return err;
    char *path = abs_path(path_arg);
    if (lookup_full(path) != NULL)
    {
        free(path);
        return STATUS_EXISTS_ERR;
    }
    descr_struct *cr = find_descr();
    if (cr == NULL)
    {
        free(path);
        return STATUS_MAX_FILES_REACHED;
    }

    int block_num = find_block();
    mask_block(block_num);

    if (block_num == -1)
    {
        free(path);
        return STATUS_NO_SPACE_LEFT;
    }

    cr->type = type;
    cr->links_num = 1;
    cr->size = 0;
    cr->blocks_id = block_num;

    char *filename = get_filename(path);
    char *dir_path = get_dir_path(path);
    descr_struct *dir = lookup_full(dir_path);
    free(dir_path);
    err = add_to_dir(dir, cr, filename);
    free(path);

    if (err)
        return err;

    if (type == DIR_TYPE)
    {
        err = add_to_dir(cr, cr, ".");
        if (err)
            return err;

        return add_to_dir(cr, dir, "..");
    } else {
        return STATUS_OK;
    }
}

int create_file(char *path_arg)
{
    return create(path_arg, FILE_TYPE);
}


int filestat(int descr_id)
{
    int err = check_mount();
    if (err)
        return err;
    descr_struct *descr = NULL;
    for (int i = 0; i < FS->max_files; ++i)
    {
        descr_struct *descr_ = DESCR_TABLE + i;
        if (descr_->id == descr_id)
        {
            descr = descr_;
            break;
        }
    }
    if (descr == NULL || descr->type == 0)
    {
        return STATUS_NOT_FOUND;
    }

    char *type;
    if (descr->type == FILE_TYPE)
    {
        type = "file";
    } else if (descr->type == DIR_TYPE) {
        type = "dir";
    } else if (descr->type == LINK_TYPE){
        type = "link";
    }
    printf("Descriptor #%d\n", descr->id);
    printf("type: %s\n", type);
    printf("size: %d\n", descr->size);
    printf("links num: %d\n", descr->links_num);
    printf("blocks num: %d\n", BLOCKS_NUM(descr));
    if (descr->type == DIR_TYPE)
    {
        printf("files num: %d\n", FILES_NUM(descr));
    }
    return STATUS_OK;
}

int mklink(char *from_arg, char *to_arg)
{
    char *from = abs_path(from_arg);
    descr_struct *from_file = lookup_full(from);
    free(from);
    if (from_file == NULL)
        return STATUS_NOT_FOUND;
    char *to = abs_path(to_arg);
    char *dir_path = get_dir_path(to);
    char *filename = get_filename(to);
    descr_struct *to_dir = lookup_full(dir_path);
    free(dir_path);
    if (to_dir == NULL)
    {
        free(to);
        return STATUS_NOT_FOUND;
    }
    int err = add_to_dir(to_dir, from_file, filename);
    free(to);
    if (err)
    {
        return err;
    } else {
        from_file->links_num++;
        return STATUS_OK;
    }
}

int rmlink(char *path_arg)
{
    char *path = abs_path(path_arg);
    descr_struct *file = lookup_link(path);
    if (file == NULL)
    {
        free(path);
        return STATUS_NOT_FOUND;
    }
    // if (file->type != FILE_TYPE && file->type != LINK_TYPE)
    // {
    //     free(path);
    //     return STATUS_NOT_FILE;
    // }
    char *filename = get_filename(path);
    char *dir_path = get_dir_path(path);
    descr_struct *dir = lookup_full(dir_path);
    free(dir_path);
    if (dir == NULL)
        return STATUS_NOT_FOUND;
    int err = rm_from_dir(dir, filename);
    free(path);
    if (err)
        return err;
    file->links_num--;
    if (file->links_num == 0)
        return rm_descr(file);
    return STATUS_OK;
}

int open_file(char *path_arg)
{
    char *path = abs_path(path_arg);
    descr_struct *file = lookup_full(path);
    free(path);
    if (file == NULL)
        return -1;
    if (file->type != FILE_TYPE && file->type != LINK_TYPE)
        return -1;

    return create_fid(file);
}

int close_file(int fid)
{
    return rm_fid(fid);
}

int read_file(int fid, int offset, int size, char *data)
{
    int err = check_fid(fid);
    if (err)
        return err;
    descr_struct *file = DESCR_TABLE + FIDS[fid];
    if (file->type != FILE_TYPE && file->type != LINK_TYPE)
        return STATUS_NOT_FILE;
    if (offset + size > file->size)
        return STATUS_SIZE_ERR;
    int *blocks = BLOCKS(file->blocks_id);
    int block_id = offset / FS->block_size;
    int b_id = offset % FS->block_size;
    char *block = BLOCKS(blocks[block_id]);
    for (int i = 0; i < size; ++i)
    {
        data[i] = block[b_id];
        b_id++;
        if (b_id >= FS->block_size)
        {
            b_id = 0;
            block_id++;
            block = BLOCKS(blocks[block_id]);
        }
    }
    return STATUS_OK;
}

int write_file(int fid, int offset, int size, char *data)
{
    int err = check_fid(fid);
    if (err)
        return err;
    descr_struct *file = DESCR_TABLE + FIDS[fid];
    if (file->type != FILE_TYPE && file->type != LINK_TYPE)
        return STATUS_NOT_FILE;
    if (offset > file->size)
        return STATUS_SIZE_ERR;

    int *blocks = BLOCKS(file->blocks_id);
    int old_blocks_num = BLOCKS_NUM(file);
    file->size += size - (file->size - offset);
    // add new blocks
    for (int i = old_blocks_num; i < BLOCKS_NUM(file); ++i)
    {
        int block_id = find_block();
        if (block_id == -1)
        {
            // TODO: release blocks
            return STATUS_NO_SPACE_LEFT;
        }
        mask_block(block_id);
        blocks[i] = block_id;
    }

    int block_id = offset / FS->block_size;
    int b_id = offset % FS->block_size;
    char *block = BLOCKS(blocks[block_id]);
    for (int i = 0; i < size; ++i)
    {
        block[b_id] = data[i];
        b_id++;
        if (b_id >= FS->block_size)
        {
            b_id = 0;
            block_id++;
            block = BLOCKS(blocks[block_id]);
        }
    }
    return STATUS_OK;
}

int trancate(char *path_arg, int new_size)
{
    char *path = abs_path(path_arg);
    descr_struct *file = lookup_full(path);
    free(path);
    if (file == NULL)
        return STATUS_NOT_FOUND;
    if (file->type != FILE_TYPE && file->type != LINK_TYPE)
        return STATUS_NOT_FILE;
    int *blocks = BLOCKS(file->blocks_id);
    int old_blocks_num = BLOCKS_NUM(file);
    if (file->size > new_size)
    {
        file->size = new_size;
        for (int i = old_blocks_num - 1; i >= BLOCKS_NUM(file); --i)
        {
            umask_block(blocks[i]);
            blocks[i] = 0;
        }
    } else {
        int fid = open_file(path_arg);
        int add_bytes = new_size - file->size;
        char *data = malloc(add_bytes);
        memset(data, 0, add_bytes);
        int err = write_file(fid, file->size, new_size - file->size, data);
        close_file(fid);
        free(data);
        return err;
    }
    return STATUS_OK;
}

int make_dir(char *path_arg)
{
    return create(path_arg, DIR_TYPE);
}

char *pwd()
{
    int err = check_mount();
    if (err)
        return NULL;
    return WORK_DIR;
}

int cd(char *path_arg)
{
    int err = check_mount();
    if (err)
        return err;
    char *path = abs_path(path_arg);
    descr_struct *dir = lookup_full(path);
    if (dir == NULL)
    {
        free(path);
        return STATUS_NOT_FOUND;
    }
    if (dir->type != DIR_TYPE)
    {
        free(path);
        return STATUS_NOT_DIR;
    }

    strcpy(WORK_DIR, path);
    free(path);
    return STATUS_OK;
}

int is_mount()
{
    return FS != NULL;
}

int remove_dir(char *path_arg)
{
    char *path = abs_path(path_arg);
    descr_struct *dir = lookup_full(path);
    if (dir == NULL)
    {
        free(path);
        return STATUS_NOT_FOUND;
    }
    if (dir->type != DIR_TYPE)
    {
        free(path);
        return STATUS_NOT_DIR;
    }
    char *name = get_filename(path);
    char *dir_path = get_dir_path(path);
    descr_struct *parent_dir = lookup_full(dir_path);
    free(dir_path);
    if (parent_dir == NULL)
    {
        free(path);
        return STATUS_NOT_FOUND;
    }
    if (dir->size > 2 * sizeof(file_struct))
    {
        free(path);
        return STATUS_NOT_EMPTY;
    }
    int err = rm_from_dir(parent_dir, name);
    free(path);
    if (err)
        return err;
    dir->links_num--;
    if (dir->links_num == 0)
        return rm_descr(dir);
    return STATUS_OK;
}

char *pack_path(char *path)
{
    if (strcmp(path, "/") == 0)
    {
        char *packed = malloc(2);
        strcpy(packed, "/");
        return packed;
    }
    char *name = get_filename(path);
    char *dir_path = get_dir_path(path);
    char *packed_dir = pack_path(dir_path);
    free(dir_path);
    if (strcmp(name, ".") == 0)
    {
        return packed_dir;
    }
    if (strcmp(name, "..") == 0)
    {
        char *dir_path = get_dir_path(packed_dir);
        free(packed_dir);
        return dir_path;
    }
    char *packed = malloc(strlen(path));
    strcpy(packed, "");
    strcat(packed, packed_dir);
    if (strcmp(packed_dir, "/") != 0)
        strcat(packed, "/");
    strcat(packed, name);
    free(packed_dir);
    return packed;
}

char *abs_path(char *path_arg)
{
    char *path = malloc(MAX_PATH_SIZE);
    strcpy(path, path_arg);
    // strip trailing slash
    int path_len = strlen(path);
    while(path_len > 1 && path[path_len - 1] == '/')
    {
        path[path_len - 1] = '\0';
        path_len--;
    }
    char *a_path = malloc(MAX_PATH_SIZE);
    if (path[0] == '/')
    {
        strcpy(a_path, path);
    } else {
        strcpy(a_path, WORK_DIR);
        if (strcmp(WORK_DIR, "/") != 0)
            strcat(a_path, "/");
        strcat(a_path, path);
    }
    free(path);

    char *packed = pack_path(a_path);
    free(a_path);
    return packed;
}

int mksymlink(char *from_arg, char *to_arg)
{
    char *from = abs_path(from_arg);
    if (lookup_full(from) == NULL)
    {
        free(from);
        return STATUS_NOT_FOUND;
    }
    char *to = abs_path(to_arg);
    int err = create(to, LINK_TYPE);
    if (err)
    {
        free(to);
        return err;
    }
    descr_struct *link = lookup_link(to);
    int fid = create_fid(link);
    err = write_file(fid, 0, strlen(from), from);
    rm_fid(fid);
    free(to);
    free(from);
    return err;
}

int get_file_size(char *path_arg)
{
    char *path = abs_path(path_arg);
    descr_struct *file = lookup_full(path);
    free(path);
    if (file == NULL)
        return -1;
    return file->size;
}

char *read_symlink(descr_struct *link)
{
    int fid = create_fid(link);
    char *data = malloc(link->size + 1);
    read_file(fid, 0, link->size, data);
    data[link->size] = '\0';
    rm_fid(fid);
    return data;
}
