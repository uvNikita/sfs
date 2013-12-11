#define STATUS_OK 0
#define STATUS_ERR 1
#define STATUS_NOT_MOUNT 2
#define STATUS_MAX_FILES_REACHED 3
#define STATUS_NO_SPACE_LEFT 4
#define STATUS_NOT_FOUND 5
#define STATUS_EXISTS_ERR 6
#define STATUS_NOT_FILE 7
#define STATUS_NOT_DIR 8
#define STATUS_SIZE_ERR 9
#define STATUS_NOT_EMPTY 10

int mount(char *path);
int umount();
int dump_stats();
int mkfs(char *path);
int create_file(char *path);
int list(char *path);
int filestat(int descr_id);
int mklink(char *from, char *to);
int rmlink(char *path);
int open_file(char *path);
int close_file(int fid);
int read_file(int fid, int offset, int size, char *data);
int write_file(int fid, int offset, int size, char *data);
int trancate(char *path, int new_size);
int make_dir(char *path);
int remove_dir(char *path);
int cd(char *path);
char *pwd();
int is_mount();
char *abs_path(char *path);
