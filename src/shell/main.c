#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "sfs.h"
#include "shell/commands.h"
#include "shell/core.h"


/* When non-zero, this global means the user is done using this program. */
int done;

char *stripwhite(char *string);
int execute_line(char *line);


int main(int argc, char **argv)
{
    initialize_readline();   /* Bind completer. */

    /* Loop reading and executing lines until the user quits. */
    while(done == 0)
    {
        char *line;
        if (is_mount())
        {
            char ps[50];
            sprintf(ps, "sfs %s: ", pwd());
            line = readline(ps);
        } else {
            line = readline("sfs: ");
        }

        if (!line)
            break;

        /* Remove leading and trailing whitespace from the line.
           Then, if there is anything left, add it to the history list
           and execute it. */
        char *striped = stripwhite(line);

        if (*striped)
        {
            add_history(striped);
            int res = execute_line(striped);
            if (res == EXIT_CODE)
            {
                done = 1;
            }
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
        return STATUS_ERR;
    }

    /* Get argument to command, if any. */
    while (whitespace(line[i]))
        i++;

    com_args = line + i;

    /* Call the function. */
    return (*(command->func)) (com_args);
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
