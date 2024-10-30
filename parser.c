#include "header.h"

/*
- Allocate memory for the command string.
- Read input from standard input using fgets.
- Check for signal interruptions and retry reading if necessary.
- Handle errors by freeing memory and exiting if reading fails.
- Return the command string.
*/
char *read_command_line()
{
  int len = 0, c;
  char *cmd = malloc(sizeof(char) * MAX_BUF_LEN);

  int again = 1;
  char *linept;

  while (again)
  {
    again = 0;
    linept = fgets(cmd, MAX_BUF_LEN, stdin);
    if (linept == NULL)
    {
      if (errno == EINTR)
      {
        again = 1; // signal interruption, read again
      }
      else
      {
        free(cmd);
        perror("Error reading input");
        exit(EXIT_FAILURE);
      }
    }
  }
  return cmd;
}

/*
- parse command input
- Initialize command count and tokenize the command input into command lines by semicolons (;)
- For each semicolon-separated token/command, further tokenize by ampersands (&)
- Allocate memory and append '&' back to indicate background when handle execute command
- Store each command in the cmds array and increment the command count.
- Return the total count of parsed commands.
*/
int parse_command_line(char *cmdline, char **cmds)
{
  int num_cmds = 0;
  char *semicolon_saveptr;
  char *ampersand_saveptr;
  char *semicolon_token = strtok_r(cmdline, ";", &semicolon_saveptr);

  while (semicolon_token != NULL)
  {
    char *ampersand_token = strtok_r(semicolon_token, "&", &ampersand_saveptr);

    while (ampersand_token != NULL)
    {
      char *next_ampersand_token = strtok_r(NULL, "&", &ampersand_saveptr);
      size_t len = strlen(ampersand_token);

      if (next_ampersand_token != NULL)
      {
        char *temp_cmd = malloc(len + 2); // +2 for '&' and '\0'
        if (temp_cmd == NULL)
        {
          fprintf(stderr, "Memory allocation failed\n");
          return num_cmds;
        }
        strcpy(temp_cmd, ampersand_token);
        temp_cmd[len] = '&';
        temp_cmd[len + 1] = '\0';
        cmds[num_cmds++] = temp_cmd;
      }
      else
      {
        cmds[num_cmds++] = ampersand_token;
      }
      ampersand_token = next_ampersand_token;
    }
    semicolon_token = strtok_r(NULL, ";", &semicolon_saveptr);
  }
  return num_cmds;
}

/*
- Tokenize the command string and store tokens in cmd_tokens.
- Return the count of parsed tokens.
*/
int parse_command(char *cmd, char **cmd_tokens)
{
  int tok = 0;
  char *token = strtok(cmd, CMD_DELIMS);
  while (token != NULL)
  {
    cmd_tokens[tok++] = token;
    token = strtok(NULL, CMD_DELIMS);
  }
  return tok;
}

/*
- Initialize indices and flags for input/output redirection and piping.
- Iterate through the command string to detect pipes and redirection symbols.
- Set flags and indices for piping, input, and output redirection.
- Return 1 if piping is detected, otherwise return -1.
*/
int is_piping(char *cmd)
{
  int i;
  input_idx = ouput_idx = output_redi_type = piping = input_redi = output_redi = 0;
  for (i = 0; cmd[i]; i++)
  {
    if (cmd[i] == '|')
    {
      piping = 1;
    }
    if (cmd[i] == '<')
    {
      input_redi = 1;
      if (input_idx == 0)
        input_idx = i;
    }
    if (cmd[i] == '>')
    {
      output_redi = 1;
      if (output_redi_type == 0)
        output_redi_type = 1;
      if (ouput_idx == 0)
        ouput_idx = i;
    }
    if (cmd[i] == '>' && cmd[i + 1] == '>')
      output_redi_type = 2;
  }
  if (piping)
    return 1;
  else
    return -1;
}

/*
- Duplicate the command string to preserve the original.
- Tokenize the command by the pipe symbol '|'.
- Store each tokenized command segment in the pipe_cmds array.
- Set pipe_num to the total number of pipe-separated segments.
*/
void parse_for_piping(char *cmd)
{
  char *copy = strdup(cmd);
  char *token;
  int tok = 0;
  token = strtok(copy, "|");
  while (token != NULL)
  {
    pipe_cmds[tok++] = token;
    token = strtok(NULL, "|");
  }
  pipe_num = tok;
}

/*
- Duplicate the command and initialize redirection flags and indices.
- Detect input/output redirection symbols and set flags.
- Tokenize and set in_file and out_file for both input and output redirection.
- Handle input redirection by extracting the input file name.
- Handle output redirection by extracting the output file name.
- Parse the command normally if no redirection is detected.
- Return the count of command tokens.
*/
int parse_for_redirect(char *cmd, char **cmd_tokens)
{
  char *copy = strdup(cmd);
  input_idx = ouput_idx = output_redi_type = input_redi = output_redi = 0;
  in_file = out_file = NULL;
  int i, tok = 0;
  for (i = 0; cmd[i]; i++)
  {
    if (cmd[i] == '<')
    {
      input_redi = 1;
      if (input_idx == 0)
        input_idx = i;
    }
    if (cmd[i] == '>')
    {
      output_redi = 1;
      if (output_redi_type == 0)
        output_redi_type = 1;
      if (ouput_idx == 0)
        ouput_idx = i;
    }
    if (cmd[i] == '>' && cmd[i + 1] == '>')
      output_redi_type = 2;
  }
  if (input_redi == 1 && output_redi == 1)
  {
    char *token;
    token = strtok(copy, " <>\t\n");
    while (token != NULL)
    {
      cmd_tokens[tok++] = strdup(token);
      token = strtok(NULL, "<> \t\n");
    }
    if (input_idx < ouput_idx)
    {
      in_file = strdup(cmd_tokens[tok - 2]);
      out_file = strdup(cmd_tokens[tok - 1]);
    }
    else
    {
      in_file = strdup(cmd_tokens[tok - 1]);
      out_file = strdup(cmd_tokens[tok - 2]);
    }
    cmd_tokens[tok - 2] = cmd_tokens[tok - 1] = NULL;

    return tok - 2;
  }

  if (input_redi == 1)
  {
    char *token;
    char *copy = strdup(cmd);

    char **input_redirect_cmds = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN);
    token = strtok(copy, "<");
    while (token != NULL)
    {
      input_redirect_cmds[tok++] = token;
      token = strtok(NULL, "<");
    }
    copy = strdup(input_redirect_cmds[tok - 1]);

    token = strtok(copy, "> |\t\n");
    in_file = strdup(token);

    tok = 0;
    token = strtok(input_redirect_cmds[0], CMD_DELIMS);
    while (token != NULL)
    {
      cmd_tokens[tok++] = strdup(token);
      token = strtok(NULL, CMD_DELIMS);
    }

    cmd_tokens[tok] = NULL;

    free(input_redirect_cmds);
  }

  if (output_redi == 1)
  {
    char *copy = strdup(cmd);
    char *token;
    char **output_redirect_cmds = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN);
    if (output_redi_type == 1)
      token = strtok(copy, ">");
    else if (output_redi_type == 2)
      token = strtok(copy, ">>");
    while (token != NULL)
    {
      output_redirect_cmds[tok++] = token;
      if (output_redi_type == 1)
        token = strtok(NULL, ">");
      else if (output_redi_type == 2)
        token = strtok(NULL, ">>");
    }

    copy = strdup(output_redirect_cmds[tok - 1]);
    token = strtok(copy, "< |\t\n");
    out_file = strdup(token);

    tok = 0;
    token = strtok(output_redirect_cmds[0], CMD_DELIMS);
    while (token != NULL)
    {
      cmd_tokens[tok++] = token;
      token = strtok(NULL, CMD_DELIMS);
    }

    free(output_redirect_cmds);
  }

  if (input_redi == 0 && output_redi == 0)
    return parse_command(strdup(cmd), cmd_tokens);
  else
    return tok;
}