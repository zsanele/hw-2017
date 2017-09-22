#define _POSIX_SOURCE

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

#define INPUT_STRING_SIZE 80
#define CNORMAL "\x1B[0m"
#define CYELLOW "\x1B[33m"

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int cmd_help(tok_t arg[]);
int cmd_quit(tok_t arg[]);
int cmd_pwd(tok_t arg[]);
int cmd_cd(tok_t arg[]);
char* find_file_from_path(char* filename, tok_t path_tokens[]);
void path_resolve(tok_t arg[], tok_t path_tokens[]);
int io_redirect(tok_t arg[]);

/**
 *  Built-In Command Lookup Table Structures
 */
typedef int cmd_fun_t (tok_t args[]); // cmd functions take token array and return int
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;
/** TODO: add more commands (pwd etc.) to this lookup table! */
fun_desc_t cmd_table[] = {
    {cmd_help, "help", "show this help menu"},
    {cmd_quit, "quit", "quit the command shell"},
    {cmd_pwd, "pwd", "prints the current working directory to standard output"},
    {cmd_cd, "cd","it changes the current directory"}
};

/**
 *  Determine whether cmd is a built-in shell command
 *
 */
int lookup(char cmd[]) {
    unsigned int i;
    for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
    }
    return -1;
}

int cmd_pwd(tok_t arg[]) {
  char* cwd = (char*) malloc(PATH_MAX + 1);
  getcwd(cwd, PATH_MAX);
  if (cwd != NULL) {
    printf("%s\n", cwd);
    free(cwd);
    return 0;
  }
  return 1;
}

/**
 *  Print the help table (cmd_table) for built-in shell commands
 *
 */
int cmd_help(tok_t arg[]) {
    unsigned int i;
    for (i = 0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
        printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
    }
    return 1;
}

/**
 *  Quit the terminal
 *
 */
int cmd_quit(tok_t arg[]) {
    printf("Bye\n");
    exit(0);
    return 1;
}

char* find_file_from_path(char* filename, tok_t path_tokens[])
{
   char* ret = (char*) malloc(PATH_MAX + MAXLINE + 2);
   struct dirent* ent;

   for (int i = 1; i < MAXTOKS && path_tokens[i]; ++i)
   {
        DIR* dir;
        if ((dir = opendir(path_tokens[i])) == NULL)
          continue;
        while ((ent = readdir(dir)) != NULL) {
          if (strcmp(ent->d_name, filename) == 0)
          {
            strncpy(ret, path_tokens[i], PATH_MAX);
            strncat(ret, "/", 1);
            strncat(ret, filename, MAXLINE);
            return ret;
          }
        }
        closedir(dir);
  }
  free(ret);
  return NULL;
}

void path_resolve(tok_t arg[], tok_t path_tokens[])
{
  char* eval = find_file_from_path(arg[0], path_tokens);
  if (eval != NULL)
  {
     arg[0] = eval;
  }
}

int io_redirect(tok_t arg[]) {
  int i, fd;
  int in_redir = is_direct_tok(arg, "<");
  int out_redir = is_direct_tok(arg, ">");
  if (in_redir != 0) {
    if (arg[in_redir + 1] == NULL ||
        strcmp(arg[in_redir + 1], ">") == 0 ||
        strcmp(arg[in_redir + 1], "<") == 0)
    {
      fprintf(stderr, "%s : Syntax error.\n", arg[0]);
      return -1;
    }
    if ((fd = open(arg[in_redir + 1], O_RDONLY, 0)) < 0) {
      fprintf(stderr, "%s : No such file or directory.\n", arg[in_redir + 1]);
      return -1;
    }
    dup2(fd, STDIN_FILENO);
    for (i = in_redir; i < MAXTOKS - 2 && arg[i + 2]; ++i) {
      free(arg[i]);
      arg[i] = arg[i + 2];
    }
    arg[i] = NULL;
  }
  if (out_redir != 0) {
    if (arg[out_redir + 1] == NULL ||
        strcmp(arg[out_redir + 1], ">") == 0 ||
        strcmp(arg[out_redir + 1], "<") == 0)
    {
      fprintf(stderr, "%s : Syntax error.\n", arg[0]);
      return -1;
    }
    if ((fd = open(arg[out_redir + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
      fprintf(stderr, "%s : No such file or directory.\n", arg[out_redir + 1]);
      return -1;
    }
    dup2(fd, STDOUT_FILENO);
    for (i = out_redir; i < MAXTOKS - 2 && arg[i + 2]; ++i) {
      free(arg[i]);
      arg[i] = arg[i + 2];
    }
    arg[i] = NULL;
  }
  return 0;
}

/**
 *  Initialise the shell
 *
 */
void init_shell() {
    // Check if we are running interactively
    shell_terminal = STDIN_FILENO;

    // Note that we cannot take control of the terminal if the shell is not interactive
    shell_is_interactive = isatty(shell_terminal);

    if( shell_is_interactive ) {

        // force into foreground
        while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
            kill( - shell_pgid, SIGTTIN);

        shell_pgid = getpid();
        // Put shell in its own process group
        if(setpgid(shell_pgid, shell_pgid) < 0){
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        // Take control of the terminal
        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);
    }

    /** TODO */
    // ignore signals
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

/**
 * Add a process to our process list
 *
 */
void add_process(/*process* p*/)
{
    /** TODO **/
    pid_t pid = fork();

}

/**
 * Creates a process given the tokens parsed from stdin
 *
 */
process* create_process(tok_t *input_str_tokens, int token_count, tok_t token_markers[], int bg)
{
    /** TODO */
    if (input_str_tokens[0] != NULL) {
        pid_t pid = fork();
        int status;
        int pgid = getpgrp();

        if (pid >= 0) {
            if (pid == 0) {
                process p;

                p.argv = input_str_tokens;
                p.argc = token_count;
                p.pid =  getpid();

		if (bg == 0) {
                    if (pgid == 0) pgid = p.pid;
                    setpgid(p.pid, pgid);
	            tcsetpgrp(STDIN_FILENO, pgid);
                    p.background = 'n';
		}
		else {
		    fprintf(stdout, "\n[1] %d\n", p.pid);
	            tcsetpgrp(shell_terminal, shell_pgid);
                    p.background = 'y';
		    fputc('\n', stdin);
		}

                signal (SIGINT, SIG_DFL);
                signal (SIGQUIT, SIG_DFL);
                signal (SIGTSTP, SIG_DFL);
                signal (SIGTTIN, SIG_DFL);
                signal (SIGTTOU, SIG_DFL);
                signal (SIGCHLD, SIG_DFL);

                p.completed  = 'n';
                p.stopped    = 'n';

                p.next = NULL;
                p.prev = NULL;

                char *prog = path_resolve(input_str_tokens[0]);

                input_str_tokens[token_count] = '\0';

                for (int i = 0; token_markers[i] && (i < MAXTOKS-1); i+=2) {
                    int in_out = token_markers[i][0] == '<' ? 0 : 1;
                    int fd;

                    if (in_out == 0) {
                        fd = open(token_markers[i+1], O_RDONLY);
                        p.stdin = fd;
                    }
                    else {
                        fd = open(token_markers[i+1], O_WRONLY | O_TRUNC |
                            O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                        p.stdout = fd;
                    }

                    dup2(fd, in_out);
                    close(fd);
                }
                int ret;
                ret = execv(prog, input_str_tokens);
                free(prog);

                p.completed = 'y';
                p.stopped = 'y';

		if (ret == -1) perror("execve");

		if (bg) fprintf(stdout, "[1] +done  %d.\n", p.pid);

                exit(ret);
            }
            else {
                if (!bg) wait(&status);
		else {
		    fputc('\n', stdin);
		}
	    }
        }
    }

    return NULL;
}



/**
 * Main shell loop
 *
 */
int shell (int argc, char *argv[]) {
    int lineNum = 0;
    pid_t pid = getpid();	// get current process's PID
    pid_t ppid = getppid();	// get parent's PID
    pid_t cpid;             // use this to store a child PID

    char *s = malloc(INPUT_STRING_SIZE+1); // user input string
    tok_t *t;			                   // tokens parsed from input
    // if you look in parse.h, you'll see that tokens are just C-style strings (char*)

    // perform some initialisation
    init_shell();

    fprintf(stdout, "%s running as PID %d under %d\n",argv[0],pid,ppid);
    /** TODO: replace "TODO" with the current working directory */
    fprintf(stdout, CYELLOW "\n%d %s# " CNORMAL, lineNum);
    cmd_pwd();

    // Read input from user, tokenize it, and perform commands
    while ( ( s = freadln(stdin) ) ) {

        t = getToks(s);            // break the line into tokens
        int fundex = lookup(t[0]); // is first token a built-in command?
        if( fundex >= 0 ) {
            cmd_table[fundex].fun(&t[1]); // execute built-in command
        } else {
            /** TODO: replace this statement to call a function that runs executables */
            //fprintf(stdout, "This shell only supports built-in functions. Replace this to run programs as commands.\n");
            path_resolve();
        }

        lineNum++;
        /** TODO: replace "TODO" with the current working directory */
        fprintf(stdout, CYELLOW "\n%d %s# " CNORMAL, lineNum);
        cmd_pwd();
    }
    return 0;
}
