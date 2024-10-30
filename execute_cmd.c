/********************************************** Fork a child process to execute a command with exec *********************************/

#include "header.h"

/*
- Forks a child process to execute a command using execvp.
- In the child process, sets process group ID, handles I/O redirection, and restores default signal handlers.
- For foreground processes, the parent waits for completion and manages terminal control.
- For background processes, the parent continues execution and adds the process to the job list.
*/
int execute_command(char **cmd_tokens)
{
  pid_t pid;
  pid = fork();
  if (pid < 0)
  {
    perror("Child Proc. not created\n");
    return -1;
  }
  else if (pid == 0)
  {
    int fin, fout, ferr;
    setpgid(pid, pid); /* Assign pgid of process equal to its pid */

    if (input_redi)
    {
      fin = open_input_file();
      if (fin == -1)
        _exit(-1);
    }
    if (output_redi)
    {
      fout = open_output_file();
      if (fout == -1)
        _exit(-1);
    }

    if (is_background == 0)
      tcsetpgrp(shell, getpid()); /* Assign terminal to this process if it is not background */

    signal(SIGINT, SIG_DFL); /* Restore default signals in child process */
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    int ret;
    if ((ret = execvp(cmd_tokens[0], cmd_tokens)) < 0)
    {
      perror("Error executing command!\n");
      _exit(-1);
    }
    _exit(0);
  }
  if (is_background == 0)
  {
    tcsetpgrp(shell, pid); /* Make sure the parent also gives control to child */
    add_process(pid, cmd_tokens[0]);
    int status;
    fgpid = pid;
    waitpid(pid, &status, WUNTRACED); /* Wait for this process, return even if it has stopped without trace */

    if (!WIFSTOPPED(status))
      remove_process(pid); /* returns true if the child process was stopped by delivery of a signal */

    else
      fprintf(stderr, "\n%s with pid %d has stopped!\n", cmd_tokens[0], pid);

    tcsetpgrp(shell, my_pgid); /* Give control of terminal back to the executable */
  }
  else
  {

    printf("\[%d] %d\n", job_num, pid);
    add_process(pid, cmd_tokens[0]);
    return 0;
  }
}

/*
Adds a process to the job table with its pid, name, and marks it as active.
*/
void add_process(int pid, char *name)
{
  table[job_num].pid = pid;
  table[job_num].name = strdup(name);
  table[job_num].active = 1;
  job_num++;
}

/*
Deactivates a process in the job table by setting its active status to 0 using its pid.
*/
void remove_process(int pid)
{
  int i;
  for (i = 0; i < job_num; i++)
  {
    if (table[i].pid == pid)
    {
      table[i].active = 0;
      break;
    }
  }
}

/*
- Process tokens to identify and execute commands.
- Handle built-in commands like history, cd, pwd, prompt, and exit.
- Execute commands by prefix or in the background if specified.
- Free command tokens after execution.
*/
void handle_normal_command(int tokens, char **cmd_tokens)
{
  if (tokens > 0)
  {
    if (strcmp(cmd_tokens[0], "history\0") == 0)
      print_history();
    else if (cmd_tokens[0][0] == '!')
    {
      char *prefix = cmd_tokens[0] + 1;
      char *found_cmd = find_command_by_prefix(prefix);
      if (found_cmd)
      {
        int j;

        char **cmd_tokens = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN);
        for (j = 0; j < MAX_BUF_LEN; j++)
        {
          cmd_tokens[j] = NULL;
        }
        parse_command(strdup(found_cmd), cmd_tokens);
        execute_command(cmd_tokens);
      }
    }
    else if (strcmp(cmd_tokens[tokens - 1], "&\0") == 0)
    {
      cmd_tokens[tokens - 1] = NULL;
      is_background = 1;
      execute_command(cmd_tokens); // for running background process
    }
    else if (strcmp(cmd_tokens[0], "cd\0") == 0)
      cd(cmd_tokens, cwd, base_dir);
    else if (strcmp(cmd_tokens[0], "pwd\0") == 0)
      pwd(cmd_tokens);
    else if (strcmp(cmd_tokens[0], "prompt\0") == 0)
      change_prompt(cmd_tokens[1]);
    else if (strcmp(cmd_tokens[0], "exit\0") == 0)
      _exit(0);
    else
      execute_command(cmd_tokens);
  }
  free(cmd_tokens);
}

/*
- Parse the command for piping and set up necessary pipes.
- Fork processes for each command segment, setting process group IDs.
- Restore default signals in child processes.
- Handle input/output redirection and pipe connections.
- Execute each command segment with execvp.
- Close all pipe file descriptors after use.
- Wait for foreground processes to complete and manage terminal control.
*/
void handle_redirect_and_piping(char *cmd)
{
  int pid, pgid, fin, fout;

  pipe_num = 0;

  parse_for_piping(cmd);

  int *pipes = (int *)malloc(sizeof(int) * (2 * (pipe_num - 1)));

  int i;

  for (i = 0; i < 2 * pipe_num - 3; i += 2)
  {
    if (pipe(pipes + i) < 0)
    { /* Create required number of pipes, each a combination of input and output fds */
      perror("Pipe not opened!\n");
      return;
    }
  }
  int status, j;
  for (i = 0; i < pipe_num; i++)
  {
    char **cmd_tokens = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN); /* array of command tokens */
    int tokens = parse_for_redirect(strdup(pipe_cmds[i]), cmd_tokens);
    is_background = 0;
    pid = fork();
    if (i < pipe_num - 1)
      add_process(pid, cmd_tokens[0]);

    if (pid != 0)
    {
      if (i == 0)
        pgid = pid;
      setpgid(pid, pgid); /* Assign pgid of process equal to pgid of first pipe command pid */
    }
    if (pid < 0)
    {
      perror("Fork Error!\n");
    }
    else if (pid == 0)
    {
      signal(SIGINT, SIG_DFL); /* Restore default signals in child process */
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
      signal(SIGCHLD, SIG_DFL);

      if (output_redi)
        fout = open_output_file();
      else if (i < pipe_num - 1)
        dup2(pipes[2 * i + 1], 1);

      if (input_redi)
        fin = open_input_file();
      else if (i > 0)
        dup2(pipes[2 * i - 2], 0);

      int j;
      for (j = 0; j < 2 * pipe_num - 2; j++)
        close(pipes[j]);

      if (execvp(cmd_tokens[0], cmd_tokens) < 0)
      {
        perror("Execvp error!\n");
        _exit(-1);
      }
    }
  }

  for (i = 0; i < 2 * pipe_num - 2; i++)
    close(pipes[i]);

  if (is_background == 0)
  {
    tcsetpgrp(shell, pgid); /* Assign terminal to pg of the pipe commands */

    for (i = 0; i < pipe_num; i++)
    {

      int cpid = waitpid(-pgid, &status, WUNTRACED);

      /* Wait for this process, return even if it has stopped without trace */

      if (!WIFSTOPPED(status))
        remove_process(cpid);
    }

    tcsetpgrp(shell, my_pgid); /* Give control of terminal back to the executable */
  }
}
