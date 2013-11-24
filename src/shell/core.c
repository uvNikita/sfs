#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <readline/readline.h>

#include "shell/commands.h"


/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */
char **completion(char *text, int start, int end)
{
    char **matches;

    matches = (char **)NULL;

    /* If this word is at the start of the line, then it is a command
       to complete.  Otherwise it is the name of a file in the current
       directory. */
    if (start == 0)
    {  
        matches = rl_completion_matches(text, command_generator);
    }

    return matches;
}


/* Tell the GNU Readline library how to complete.  Try to complete on command
   names if this is the first word in the line, or on filenames if not. */
void initialize_readline()
{
    rl_attempted_completion_function = (CPPFunction *)completion;
}

