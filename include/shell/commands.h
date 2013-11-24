typedef struct {
    char *name;           /* User printable name of the function. */
    Function *func;       /* Function to call to do the job. */
    char *doc;            /* Documentation for this function.  */
} COMMAND;

COMMAND *find_command(char *name);
char *command_generator(const char *text, int state);

#define EXIT_CODE 3

