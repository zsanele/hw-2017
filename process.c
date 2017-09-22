#include "process.h"
#include "shell.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <termios.h>
#include <fcntl.h>

/**
 *  Executes the process p.
 *  If the shell is in interactive mode and the process is a foreground process,
 *  then p should take control of the terminal.
 *
 */

void launch_process (process *p, pid_t pgid, int infile, int outfile, int errfile, int foreground)
{
    /** TODO **/

  pid_t pid;

  if (shell_is_interactive)
    {
      pid = getpid ();
      if (pgid == 0)
        pgid = pid;
        setpgid(pid, pgid);
      if (foreground)
        tcsetpgrp (shell_terminal, pgid);

      signal (SIGINT, SIG_DFL);
      signal (SIGQUIT, SIG_DFL);
      signal (SIGTSTP, SIG_DFL);
      signal (SIGTTIN, SIG_DFL);
      signal (SIGTTOU, SIG_DFL);
      signal (SIGCHLD, SIG_DFL);
    }

  if (infile != STDIN_FILENO)
    {
      dup2 (infile, STDIN_FILENO);
      close (infile);
    }
  if (outfile != STDOUT_FILENO)
    {
      dup2 (outfile, STDOUT_FILENO);
      close (outfile);
    }
  if (errfile != STDERR_FILENO)
    {
      dup2(errfile, STDERR_FILENO);
      close(errfile);
    }

  execvp (p->argv[0], p->argv);
  perror ("execvp");
  exit (1);
}

}

/**
 *  Put a process in the foreground. This function assumes that the shell
 *  is in interactive mode. If the cont argument is true, send the process
 *  group a SIGCONT signal to wake it up.
 *
 */
void put_process_in_foreground (pid_t pid, bool cont, struct termios *tmodes) {
    /** TODO **/
    int status;

  tcsetpgrp(shell_terminal, pid);
  if (tmodes)
    tcsetattr(shell_terminal, TCSADRAIN, tmodes);

  if (cont && kill(-pid, SIGCONT) < 0)
    perror("kill (SIGCONT)");
    waitpid(pid, &status, WUNTRACED);
    tcsetpgrp(shell_terminal, shell_pgid);

  if (tmodes)
    tcgetattr(shell_terminal, tmodes);
    tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
 ;
}

/**
 *  Put a process in the background. If the cont argument is true, send
 *  the process group a SIGCONT signal to wake it up.
 *
 */
void put_process_in_background (pid_t pid, bool cont) {
    /** TODO **/
   if (cont && kill(-pid, SIGCONT) < 0)
    perror("kill (SIGCONT)");
}

/**
 *  Prints the list of processes.
 *
 */
void print_process_list(void) {
    process* curr = first_process;
    while(curr) {
        if(!curr->completed) {
            printf("\n");
            printf("PID: %d\n",curr->pid);
            printf("Name: %s\n",curr->argv[0]);
            printf("Status: %d\n",curr->status);
            printf("Completed: %s\n",(curr->completed)?"true":"false");
            printf("Stopped: %s\n",(curr->stopped)?"true":"false");
            printf("Background: %s\n",(curr->background)?"true":"false");
            printf("Prev: %lx\n",(unsigned long)curr->prev);
            printf("Next: %lx\n",(unsigned long)curr->next);
        }
        curr=curr->next;
    }
}
