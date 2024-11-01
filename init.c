#include "header.h"

/*
Retrieves the current working directory and stores it in `base_dir`,
then copies it to `cwd` for further use.
*/
void get_home_dir(void)
{
  getcwd(base_dir, MAX_BUF_LEN - 1);
  strcpy(cwd, base_dir);
}

/*
Updates `cwd` to a relative path by replacing the common prefix with `base_dir` with '~'
*/
void update_cwd_relative(char *cwd)
{
  int i, j;
  for (i = 0; cwd[i] == base_dir[i] && cwd[i] != '\0' && base_dir[i] != '\0'; i++)
    ;

  // If entire base_dir matches the start of cwd
  if (base_dir[i] == '\0')
  {
    cwd[0] = '~'; // Replace the start of cwd with '~'

    for (j = 1; cwd[i] != '\0'; j++) // Copy the remaining part of cwd after base_dir to the position after '~'
    {
      cwd[j] = cwd[i++];
    }
    cwd[j] = '\0';
  }
}

/*
Handles SIGINT by ignoring and resetting the handler.
Processes SIGCHLD to manage child process termination and update their status in the job table.
*/
void handle_signal(int signum)
{
  if (signum == SIGINT)
  {
    signal(SIGINT, SIG_IGN);       // ignore Ctrl+C
    signal(SIGINT, handle_signal); // reset signal handler
  }
  else if (signum == SIGCHLD)
  { /* For handling signal from child processes */
    int i, status, die_pid;
    while ((die_pid = waitpid(-1, &status, WNOHANG)) > 0)
    { /* Get id of the process which has terminated  */
      for (i = 0; i < job_num; i++)
      {
        if (table[i].active == 0)
          continue;
        else if (table[i].pid == die_pid)
          break;
      }
      if (i != job_num)
      {
        if (WIFEXITED(status)) /* returns true if the child terminated normally */
          fprintf(stdout, "\n%s with pid %d exited normally\n", table[i].name, table[i].pid);
        else if (WIFSIGNALED(status)) /* returns true if the child process was terminated by a signal */
          fprintf(stdout, "\n%s with pid %d has exited with signal\n", table[i].name, table[i].pid);
        table[i].active = 0;
      }
    }
  }
}

/*
Set up file descriptors, allocate memory
Ignore specific signals
Set process group ID
Update the current directory relative to the home directory.
*/

void setup(void)
{

  shell = STDERR_FILENO; // FD for stderr

  job_num = 0; //  Init job number counter to keep track of background or concurrent jobs

  if (isatty(shell)) // Checks if the file descriptor refers to a terminal.
  {
    // Ensure process group matches terminal's
    // if not send SIGTTIN to stop the process until it can be attached to the terminal.
    while (tcgetpgrp(shell) != (shell_pgid = getpgrp()))
      kill(shell_pgid, SIGTTIN);
  }

  my_pid = my_pgid = getpid(); /* process group ID to match pid */
  setpgid(my_pid, my_pgid);
  tcsetpgrp(shell, my_pgid); /* Assign control of stderr to the process group */

  signal(SIGQUIT, SIG_IGN); /* To ignore Ctrl+\ */
  signal(SIGTSTP, SIG_IGN); // ignore Ctrl+Z
  signal(SIGINT, SIG_IGN);  // ignore Ctrl+C
  signal(SIGTTIN, SIG_IGN); // Ignore attempts to read from the terminal in the background.
  signal(SIGTTOU, SIG_IGN); // Ignore attempts to write to the terminal in the background.

  get_home_dir();
  update_cwd_relative(cwd);
}
