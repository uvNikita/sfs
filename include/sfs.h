#define STATUS_OK 0
#define STATUS_ERR 1
#define STATUS_NOT_MOUNT 2
#define STATUS_MAX_FILES_REACHED 3
#define STATUS_NO_SPACE_LEFT 4
#define STATUS_NOT_FOUND 5
#define STATUS_EXISTS_ERR 6

int mount(char *path);
int umount();
int dump_stats();
int mkfs(char *path);
int create_file(char *path);
int list(char *path);
int filestat(int descr_id);
