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
    perror("Child Process not created\n");
    return -1;
  }
  else if (pid == 0)
  {
    int fin, fout;
    setpgid(pid, pid); // Set the process group id to the process id

    if (input_redi) // Handle input redirect
    {
      fin = open_input_file();
      if (fin == -1)
        _exit(-1);
    }

    if (output_redi) // Handle output redirect
    {
      fout = open_output_file();
      if (fout == -1)
        _exit(-1);
    }

    // Assign terminal control to process if it's not running in the background
    if (is_background == 0)
      tcsetpgrp(shell, getpid());

    // Restore default signal handlers in the child process
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    // Execute command
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
    // Assign terminal control to the child process
    tcsetpgrp(shell, pid);
    add_process(pid, cmd_tokens[0]);

    int status;
    fgpid = pid;
    waitpid(pid, &status, WUNTRACED); // Wait for this process

    // if the process was stopped by a signal
    if (!WIFSTOPPED(status))
      remove_process(pid);

    else
      fprintf(stderr, "\n%s with pid %d has stopped!\n", cmd_tokens[0], pid);

    // Return terminal control to the shell
    tcsetpgrp(shell, my_pgid);
    return 0;
  }
  else
  {
    printf("\[%d] %d\n", job_num, pid); // Print job information of background processes
    add_process(pid, cmd_tokens[0]);    // Add proc. to the process list
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
          cmd_tokens[j] = NULL;

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
void handle_piping_and_redirect(char *cmd)
{
  int pid, pgid, fin, fout;

  pipe_num = 0;

  int *pipes = (int *)malloc(sizeof(int) * (2 * (pipe_num - 1)));

  int i;

  // Create the necessary pipes for inter-process communication
  for (i = 0; i < 2 * pipe_num - 3; i += 2) // pipes creataion
  {
    if (pipe(pipes + i) < 0)
    {
      perror("Pipe not opened!\n");
      return;
    }
  }

  int status, j;
  for (i = 0; i < pipe_num; i++)
  {
    char **cmd_tokens = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN); // Allocate memory for command tokens

    int tokens = parse_for_redirect(strdup(pipe_cmds[i]), cmd_tokens); //
    is_background = 0;
    pid = fork();
    if (i < pipe_num - 1)
      add_process(pid, cmd_tokens[0]); // Add the process to the process list

    if (pid != 0)
    {
      if (i == 0)
        pgid = pid;
      setpgid(pid, pgid); // Assign the process group ID to the current process
    }
    if (pid < 0)
    {
      perror("Fork Error!\n");
    }
    else if (pid == 0)
    {
      // Restore default signals in child process
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);
      signal(SIGTTIN, SIG_DFL);
      signal(SIGTTOU, SIG_DFL);
      signal(SIGCHLD, SIG_DFL);

      // output redirection or pipe output
      if (output_redi)
        fout = open_output_file();
      else if (i < pipe_num - 1)
        dup2(pipes[2 * i + 1], 1);

      // input redirection or pipe input
      if (input_redi)
        fin = open_input_file();
      else if (i > 0)
        dup2(pipes[2 * i - 2], 0);

      // Close all pipe file descriptors in child process
      int j;
      for (j = 0; j < 2 * pipe_num - 2; j++)
        close(pipes[j]);

      // Execute command
      if (execvp(cmd_tokens[0], cmd_tokens) < 0)
      {
        perror("Execvp error!\n");
        _exit(-1);
      }
    }
  }

  // Close all pipe file descriptors in the parent process
  for (i = 0; i < 2 * pipe_num - 2; i++)
    close(pipes[i]);

  if (is_background == 0)
  {
    // Assign terminal to the process group
    tcsetpgrp(shell, pgid);

    for (i = 0; i < pipe_num; i++)
    {

      // Wait for each process in the pipeline
      int cpid = waitpid(-pgid, &status, WUNTRACED);

      if (!WIFSTOPPED(status))
        remove_process(cpid);
    }

    // Return control back to shell
    tcsetpgrp(shell, my_pgid);
  }
}
