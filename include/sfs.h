#define STATUS_OK 0
#define STATUS_ERR 1
#define STATUS_NOT_MOUNT 1

int mount(char *path);
int umount();
int dump_stats();
int mkfs(char *path);
