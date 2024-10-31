#include "header.h"

char prompt[MAX_BUF_LEN] = "%";

/*
 * 1. Init shell and set up
 * 2. Enter infinite shell loop - quit with 'exit'
 *    2.1 Signal handling for child processes and interrupts
 *    2.2 Display shell prompt
 *    2.3 Read command input
 *    2.4 Parse command input into command lines
 *    2.5 Add command into history
 *    2.6 execute each command while managing piping, input/output redirection and background execution.
 */

int main(void)
{
  setup();

  // Shell loop
  while (1)
  {
    // signal handling for child processes and interrupts
    if (signal(SIGCHLD, handle_signal) == SIG_ERR)
      perror("can't catch SIGCHLD");
    if (signal(SIGINT, handle_signal) == SIG_ERR)
      perror("can't catch SIGINT!");

    // Display shell prompt
    printf("%s ", prompt);

    char **cmds = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN); // array of command lines

    // read command input consists of one or several command lines
    char *cmdline = read_command_line();

    // parse command input into separate command lines with '&' and/or ';'
    int num_cmds = parse_command_line(cmdline, cmds);

    for (int i = 0; i < num_cmds; i++)
    {
      // Add command into history, which is set to 10
      add_to_history(cmds[i]);
      is_background = 0, pipe_num = 0;

      char **cmd_tokens = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN); // array of command tokens

      for (int j = 0; j < MAX_BUF_LEN; j++)
        cmd_tokens[j] = NULL;

      // If not pipelines, handle command with/without IO redirect
      if (is_piping(strdup(cmds[i])) == -1)
      {
        int tokens;
        if (input_redi == 1 || output_redi == 1)
          tokens = parse_for_redirect(strdup(cmds[i]), cmd_tokens);
        else
          tokens = parse_command(strdup(cmds[i]), cmd_tokens);

        handle_normal_command(tokens, cmd_tokens);
      }
      else
        // Shell pipelines, handle piping and redirection
        handle_piping_and_redirect(cmds[i]);
    }

    free(cmds);
    free(cmdline);
  }
  return 0;
}
