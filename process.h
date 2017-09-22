#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <signal.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct process {
    char** argv;
    int argc;
    pid_t pid;
    bool completed;
    bool stopped;
    bool background;
    int status;
    struct termios tmodes;
    int stdin, stdout, stderr;
    struct process* next;
    struct process* prev;
} process;

process* first_process; // pointer to the first process that is launched

void launch_process(process *p, pid_t pgid, int infile, int outfile, int errfile, int foreground);
void put_process_in_background (pid_t pid, bool cont);
void put_process_in_foreground (pid_t pid, bool cont, struct termios *tmodes);

#endif
