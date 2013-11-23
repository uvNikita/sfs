#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <readline/readline.h>
#include <readline/history.h>

/* All in-program commands */
int com_mount();
int com_umount();
int com_stat();
int com_list();
int com_pwd();
int com_create();
int com_help();
int com_open();
int com_quit();
int com_close();
int com_tranc();
int com_link();
int com_unlink();
int com_read();
int com_write();

typedef struct {
    char *name;           /* User printable name of the function. */
    Function *func;       /* Function to call to do the job. */
    char *doc;            /* Documentation for this function.  */
} COMMAND;

COMMAND commands[] = {
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
    { "quit", com_quit, "Quit using Fileman" },
    { (char *)NULL, (Function *)NULL, (char *)NULL }
};

/* Forward declarations. */
char *stripwhite();
COMMAND *find_command();
void initialize_readline();
int execute_line();

/* When non-zero, this global means the user is done using this program. */
int done;

char *dupstr(char *s)
{
    char *r = malloc(strlen(s));
    strcpy(r, s);
    return r;
}

int main(int argc, char **argv)
{
    initialize_readline();   /* Bind completer. */

    /* Loop reading and executing lines until the user quits. */
    while(done == 0)
    {
        char *line = readline("# ");

        if (!line)
            break;

        /* Remove leading and trailing whitespace from the line.
           Then, if there is anything left, add it to the history list
           and execute it. */
        char *striped = stripwhite(line);

        if (*striped)
        {
            add_history(striped);
            execute_line(striped);
        }

        free(line);
    }
    exit(0);
}

/* Execute a command line. */
int execute_line(char *line)
{
    register int i;
    COMMAND *command;
    char *com_word;
    char *com_args;

    /* Isolate the command word from args. */
    com_word = line;

    i = 0;
    while (line[i] && !whitespace(line[i]))
        i++;

    if (line[i])
        line[i++] = '\0';

    command = find_command(com_word);

    if (!command)
    {
        fprintf(stderr, "%s: No such command.\n", com_word);
        return (-1);
    }

    /* Get argument to command, if any. */
    while (whitespace(line[i]))
        i++;

    com_args = line + i;

    /* Call the function. */
    return (*(command->func)) (com_args);
}

/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
COMMAND *find_command(char *name)
{
  for (int i = 0; commands[i].name; i++)
    if (strcmp(name, commands[i].name) == 0)
      return (&commands[i]);

  return ((COMMAND *)NULL);
}

/* Strip whitespace from the start and end of STRING.  Return a pointer
   into STRING. */
char *stripwhite(char *string)
{
    char *s, *t;

    for (s = string; whitespace(*s); s++)
    ;
    
    if (*s == 0)
        return s;

    t = s + strlen(s) - 1;
    while (t > s && whitespace(*t))
        t--;
    *++t = '\0';

    return s;
}

/* **************************************************************** */
/*                                                                  */
/*                  Interface to Readline Completion                */
/*                                                                  */
/* **************************************************************** */

char *command_generator ();
char **fileman_completion ();

/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */
void initialize_readline()
{
    /* Allow conditional parsing of the ~/.inputrc file. */
    rl_readline_name = "sfs shell";

    /* Tell the completer that we want a crack first. */
    rl_attempted_completion_function = (CPPFunction *)fileman_completion;
}

/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */
char **fileman_completion(char *text, int start, int end)
{
    char **matches;

    matches = (char **)NULL;

    /* If this word is at the start of the line, then it is a command
       to complete.  Otherwise it is the name of a file in the current
       directory. */
    if (start == 0)
        matches = rl_completion_matches(text, command_generator);

    return matches;
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *command_generator(char *text, int state)
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

/* **************************************************************** */
/*                                                                  */
/*                       Shell Commands                           */
/*                                                                  */
/* **************************************************************** */

int valid_argument();

int com_mount(char *arg)
{
    if (!valid_argument("mount", arg))
        return 1;
    printf("mount command\n");
    return 0;
}

int com_umount(char *arg)
{
    if (!valid_argument("unmount", arg))
        return 1;
    printf("unmount command\n");
    return 0;
}

int com_stat(char *arg)
{
    if (!valid_argument("stat", arg))
        return 1;
    printf("stat command\n");
    return 0;
}

int com_list(char *arg)
{
    if (!arg)
        arg = "";
    printf("list command\n");
    return 0;
}

int com_create(char *arg)
{
    if (!valid_argument("create", arg))
        return 1;
    printf("create command\n");
    return 0;
}

int com_open(char *arg)
{
    if (!valid_argument("open", arg))
        return 1;
    printf("open command\n");
    return 0;
}

int com_close(char *arg)
{
    if (!valid_argument("close", arg))
        return 1;
    printf("close command\n");
    return 0;
}

int com_read(char *arg)
{
    if (!valid_argument("read", arg))
        return 1;
    printf("read command\n");
    return 0;
}

int com_write(char *arg)
{
    if (!valid_argument("write", arg))
        return 1;
    printf("write command\n");
    return 0;
}

int com_link(char *arg)
{
    if (!valid_argument("link", arg))
        return 1;
    printf("link command\n");
    return 0;
}

int com_unlink(char *arg)
{
    if (!valid_argument("unlink", arg))
        return 1;
    printf("unlink command\n");
    return 0;
}

int com_tranc(char *arg)
{
    if (!valid_argument("tranc", arg))
        return 1;
    printf("tranc command\n");
    return 0;
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
    return 0;
}

/* Print out the current working directory. */
int com_pwd(char *ignore)
{
    printf("/");
    return 0;
}

/* The user wishes to quit using this program. Just set DONE non-zero. */
int com_quit(char *arg)
{
    done = 1;
    return 0;
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
