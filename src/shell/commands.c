#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <readline/readline.h>

#include "shell/commands.h"
#include "sfs.h"


int com_mount(char *arg);
int com_umount(char *arg);
int com_mkfs(char *arg);
int com_stat(char *arg);
int com_list(char *arg);
int com_pwd(char *arg);
int com_create(char *arg);
int com_help(char *arg);
int com_open(char *arg);
int com_quit(char *arg);
int com_close(char *arg);
int com_tranc(char *arg);
int com_link(char *arg);
int com_unlink(char *arg);
int com_read(char *arg);
int com_write(char *arg);
int com_dump_stats(char *arg);

COMMAND commands[] = {
    { "mkfs", com_mkfs, "Create file system in file" },
    { "mount", com_mount, "Mount file system" },
    { "umount", com_umount, "Umount file system" },
    { "stat", com_stat, "Get info about descriptor spec. by ID" },
    { "help", com_help, "Display this text" },
    { "?", com_help, "Synonym for `help'" },
    { "list", com_list, "List files in DIR" },
    { "ls", com_list, "Synonym for `list'" },
    { "create", com_create, "Create file with FILENAME" },
    { "open", com_open, "Open FILE, get FD" },
    { "close", com_close, "Close file FD" },
    { "read", com_read, "Read from file: FD, OFFSET, SIZE" },
    { "write", com_write, "Write to file: FD, OFFSET, SIZE" },
    { "link", com_link, "Create link from FILE1 ro FILE2" },
    { "unlink", com_unlink, "Destroy link LINK" },
    { "pwd", com_pwd, "Print the current working directory" },
    { "tranc", com_tranc, "Change FILE size to SIZE" },
    { "dump", com_dump_stats, "Print file system stats" },
    { "quit", com_quit, "Quit shell" },
    { (char *)NULL, (Function *)NULL, (char *)NULL }
};



/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
COMMAND *find_command(char *name)
{
    for (int i = 0; commands[i].name; i++)
    {
        if (strcmp(name, commands[i].name) == 0)
        {
            return (&commands[i]);
        }
    }

  return ((COMMAND *)NULL);
}

char *dupstr(char *s)
{
    char *r = malloc(strlen(s));
    strcpy(r, s);
    return r;
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *command_generator(const char *text, int state)
{
    static int list_index, len;
    char *name;

    /* If this is a new word to complete, initialize now.  This includes
       saving the length of TEXT for efficiency, and initializing the index
       variable to 0. */
    if (!state)
    {
        list_index = 0;
        len = strlen(text);
    }

    /* Return the next name which partially matches from the command list. */
    while ((name = commands[list_index].name))
    {
        list_index++;

        if (strncmp(name, text, len) == 0)
            return dupstr(name);
    }

    /* If no names matched, then return NULL. */
    return ((char *)NULL);
}


/* Forward declarations. */
int valid_argument(char *caller, char *arg);
int valid_path(char *path);

int com_mount(char *arg)
{
    if (!valid_argument("mount", arg))
        return 1;
    if (mount(arg))
    {
        fprintf(stderr, "mount error\n");
        return STATUS_ERR;
    } else {
        printf("mount success\n");
        return STATUS_OK;
    }
}

int com_umount(char *ignore)
{
    int err = umount();
    if (err)
    {
        fprintf(stderr, "umount failed\n");
        return STATUS_ERR;
    } else {
        printf("umount success\n");
        return STATUS_OK;
    }
}

int com_mkfs(char *path)
{
    int err = mkfs(path);
    if (err)
    {
        fprintf(stderr, "Failed\n");
        return STATUS_ERR;
    }
    return STATUS_OK;
}

int com_stat(char *descr_id)
{
    if (!valid_argument("stat", descr_id))
        return 1;
    int err = filestat(atoi(descr_id));
    if (err == STATUS_NOT_FOUND)
    {
        fprintf(stderr, "No such descriptor: %s\n", descr_id);
        return STATUS_ERR;
    } else if (err == STATUS_OK) {
        return STATUS_OK;
    } else {
        fprintf(stderr, "Error occured\n");
        return STATUS_ERR;
    }
}

int com_list(char *path)
{
    if (path == NULL || (strcmp(path, "") == 0))
        path = "/";
    if (!valid_path(path))
        return STATUS_ERR;
    return list(path);
}

int com_create(char *path)
{
    if (!valid_argument("create", path))
        return STATUS_ERR;
    if (!valid_path(path))
        return STATUS_ERR;
    int err = create_file(path);
    if (err == STATUS_NO_SPACE_LEFT)
    {
        fprintf(stderr, "No space left on device\n");
        return STATUS_ERR;
    } else if (err == STATUS_EXISTS_ERR) {
        fprintf(stderr, "File already exists\n");
        return STATUS_ERR;
    } else {
        return STATUS_OK;
    }
}

int com_open(char *path)
{
    if (!valid_argument("open", path))
        return STATUS_ERR;
    int fid = open_file(path);
    if (fid == -1)
    {
        fprintf(stderr, "Can't open '%s'\n", path);
        return STATUS_ERR;
    }
    printf("fid: %d\n", fid);
    return STATUS_OK;
}

int com_close(char *fid_arg)
{
    if (!valid_argument("close", fid_arg))
        return STATUS_ERR;
    int fid = atoi(fid_arg);
    int err = close_file(fid);
    if (err == STATUS_NOT_FOUND)
    {
        fprintf(stderr, "No such fid: %d\n", fid);
        return STATUS_ERR;
    } else if(err) {
        fprintf(stderr, "Something goes wrong :(\n");
        return STATUS_ERR;
    }
    return STATUS_OK;
}

int com_read(char *arg)
{
    if (!valid_argument("read", arg))
        return 1;
    printf("read command\n");
    return STATUS_OK;
}

int com_write(char *arg)
{
    if (!valid_argument("write", arg))
        return 1;
    printf("write command\n");
    return STATUS_OK;
}

int com_link(char *arg)
{
    if (!valid_argument("link", arg))
        return STATUS_ERR;
    char *from = arg;
    char *to;
    for (int i = 0; i < strlen(arg); ++i)
    {
        if (arg[i] == ' ')
        {
            arg[i] = '\0';
            to = arg + i + 1;
        }
    }
    if (!valid_path(from) || !valid_path(to))
        return STATUS_ERR;
    int err = mklink(from, to);
    if (err == STATUS_NOT_FOUND)
    {
        fprintf(stderr, "No such file: %s\n", from);
        return STATUS_ERR;
    } else if(err) {
        return STATUS_ERR;
    }
    return STATUS_OK;
}

int com_unlink(char *path)
{
    if (!valid_argument("unlink", path))
        return STATUS_ERR;
    if (!valid_path(path))
        return STATUS_ERR;
    int err = rmlink(path);
    if (err == STATUS_NOT_FOUND)
    {
        fprintf(stderr, "No such file: %s\n", path);
        return STATUS_ERR;
    } else if(err) {
        return STATUS_ERR;
    }
    return STATUS_OK;
}

int com_tranc(char *arg)
{
    if (!valid_argument("tranc", arg))
        return 1;
    printf("tranc command\n");
    return STATUS_OK;
}

/* Print out help for ARG, or for all of the commands if ARG is not present. */
int com_help(char *arg)
{
    register int i;
    int printed = 0;

    for (i = 0; commands[i].name; i++)
    {
        if (!*arg || (strcmp(arg, commands[i].name) == 0))
        {
            printf("%s\t\t\t%s.\n", commands[i].name, commands[i].doc);
            printed++;
        }
    }

    if (!printed)
    {
        printf("No commands match `%s'.  Possibilties are:\n", arg);

        for (i = 0; commands[i].name; i++)
        {
            /* Print in six columns. */
            if (printed == 6)
            {
                printed = 0;
                printf("\n");
            }

            printf("%s\t", commands[i].name);
            printed++;
        }

        if (printed)
            printf("\n");
    }
    return STATUS_OK;
}

/* Print out the current working directory. */
int com_pwd(char *ignore)
{
    printf("/");
    return STATUS_OK;
}

/* The user wishes to quit using this program. Just set DONE non-zero. */
int com_quit(char *arg)
{
    return EXIT_CODE;
}

int com_dump_stats(char *ignore)
{
    return dump_stats();
}

/* Return non-zero if ARG is a valid argument for CALLER, else print
   an error message and return zero. */
int valid_argument(char *caller, char *arg)
{
    if (!arg || !*arg)
    {
        fprintf(stderr, "%s: Argument required.\n", caller);
        return 0;
    }

    return 1;
}

int valid_path(char *path)
{
    if (!path || path[0] != '/')
    {
        fprintf(stderr, "Only absolute path is supported\n");
        return 0;
    }

    return 1;
}
