#ifndef USPSH_H
#define USPSH_H

/* Bibliotecas */

#include <stdio.h>          // printf, snprintf
#include <stdlib.h>         // malloc, free, exit, strtol
#include <string.h>         // strlen, strcmp, strtok
#include <unistd.h>         // gethostname, getcwd, fork, execve, chdir, getuid
#include <pwd.h>            // struct passwd, getpwuid
#include <sys/types.h>      // mode_t, pid_t
#include <sys/stat.h>       // chmod
#include <sys/wait.h>       // waitpid
#include <readline/readline.h>  // readline
#include <readline/history.h>   // add_history

char *show_prompt();
char *read_command(char **command, char **args, char *prompt);

#endif