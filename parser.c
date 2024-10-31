#include "header.h"

/*
- Allocate memory for the command string.
- Read input from standard input using fgets.
- Check for signal interruptions and retry reading if necessary.
- Handle errors by freeing memory and exiting if reading fails.
- Return the command string.
*/
char *read_command_line(void)
{
  char *cmd = malloc(sizeof(char) * MAX_BUF_LEN);

  int again = 1;
  char *linept; // pointer to the line buffer

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
    if (strchr(token, '*') || strchr(token, '?'))
    {
      int expanded_count = expand_wildcard_token(token, cmd_tokens, tok);
      tok += expanded_count; // Update tok based on the number of expanded tokens
    }
    else
    {
      if (tok < MAX_BUF_LEN - 1) // Add the token as is
        cmd_tokens[tok++] = strdup(token);
    }
    token = strtok(NULL, CMD_DELIMS);
  }

  return tok;
}

// Utility function to expand a token with wildcards using glob
int expand_wildcard_token(char *token, char **expanded_tokens, int start_index)
{
  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_result));

  int return_value = glob(token, GLOB_TILDE, NULL, &glob_result);
  int count = 0;

  if (return_value == 0)
  // Matches found, add each to the expanded_tokens array
  {
    for (size_t i = 0; i < glob_result.gl_pathc; ++i)
    {
      if (start_index + count < MAX_BUF_LEN - 1)
      {
        expanded_tokens[start_index + count] = strdup(glob_result.gl_pathv[i]);
        count++;
      }
      else
      {
        fprintf(stderr, "Too many tokens\n");
        break;
      }
    }
  }
  else
  {
    // If no matches, keep the original token
    if (start_index + count < MAX_BUF_LEN - 1)
    {
      expanded_tokens[start_index + count] = strdup(token);
      count++;
    }
  }

  globfree(&glob_result);
  return count;
}

/**
 * Detects '<' , '>' and '>>' in a command, updating flags and indices for redirection.
 */
void check_redirect(char *cmd, int *input_redi, int *input_idx, int *output_redi, int *output_redi_type, int *output_idx)
{
  for (int i = 0; cmd[i]; i++)
  {
    if (cmd[i] == '<')
    {
      *input_redi = 1;
      if (*input_idx == 0)
        *input_idx = i;
    }
    if (cmd[i] == '>')
    {
      *output_redi = 1;
      if (*output_redi_type == 0)
        *output_redi_type = 1;
      if (*output_idx == 0)
        *output_idx = i;
    }
    if (cmd[i] == '>' && cmd[i + 1] == '>')
    {
      *output_redi_type = 2;
    }
  }
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
  input_idx = 0;
  output_idx = 0;
  output_redi_type = 0;
  piping = 0;
  input_redi = 0;
  output_redi = 0;

  check_redirect(cmd, &input_redi, &input_idx, &output_redi, &output_redi_type, &output_idx);

  for (i = 0; cmd[i]; i++)
  {
    if (cmd[i] == '|')
    {
      piping = 1;
      break;
    }
  }

  if (piping)
    return 1;
  else
    return -1;
}

/*
- Detect input/output redirection symbols and set flags.
- Tokenize and set in_file and out_file for both input and output redirection.
- Handle input redirection by extracting the input file name.
- Handle output redirection by extracting the output file name.
- Parse the command normally if no redirection is detected.
- Return the count of command tokens excluding the redirection file names.
*/
int parse_for_redirect(char *cmd, char **cmd_tokens)
{
  input_idx = 0;
  output_idx = 0;
  output_redi_type = 0;
  input_redi = 0;
  output_redi = 0;
  in_file = NULL;
  out_file = NULL;
  int tok = 0;

  // Detect input/output redirection symbols and set flags.
  check_redirect(cmd, &input_redi, &input_idx, &output_redi, &output_redi_type, &output_idx);

  char *copy = strdup(cmd); // the cmd string to avoid modifying the original

  // command with both input and output redirection
  if (input_redi == 1 && output_redi == 1)
  {
    char *token;
    token = strtok(copy, " <>\t\n");
    while (token != NULL)
    {
      cmd_tokens[tok++] = strdup(token);
      token = strtok(NULL, "<> \t\n");
    }

    // check the order of the redirection and assign file names
    if (input_idx < output_idx)
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

  // command with input redirection
  if (input_redi == 1)
  {
    char *token;
    char *copy = strdup(cmd);

    char **input_redirect_cmds = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN);

    // Tokenize with '<' and store tokens in input_redirect_cmds array
    token = strtok(copy, "<");
    while (token != NULL)
    {
      input_redirect_cmds[tok++] = token;
      token = strtok(NULL, "<");
    }

    copy = strdup(input_redirect_cmds[tok - 1]);

    // tokenize to extract the input file name, ignoring other delimiters
    token = strtok(copy, "> |\t\n");

    // assign file name
    in_file = strdup(token);

    tok = 0;

    // Tokenize the first part of the command to extract command tokens and store in cmd_tokens array
    token = strtok(input_redirect_cmds[0], CMD_DELIMS);

    while (token != NULL)
    {
      cmd_tokens[tok++] = strdup(token);
      token = strtok(NULL, CMD_DELIMS);
    }

    cmd_tokens[tok] = NULL; // end of command tokens

    free(input_redirect_cmds);
  }

  // command with output redirection
  if (output_redi == 1)
  {
    char *copy = strdup(cmd);
    char *token;
    char **output_redirect_cmds = malloc((sizeof(char) * MAX_BUF_LEN) * MAX_BUF_LEN);

    // Determine the delimiter based on the type of output redirection
    // Tokenize the using the appropriate delimiter
    if (output_redi_type == 1)
      token = strtok(copy, ">");
    else if (output_redi_type == 2)
      token = strtok(copy, ">>");

    while (token != NULL)
    {
      output_redirect_cmds[tok++] = token; // store tokens in output_redirect_cmds array

      // Continue tokenizing based on the redirection type
      if (output_redi_type == 1)
        token = strtok(NULL, ">");
      else if (output_redi_type == 2)
        token = strtok(NULL, ">>");
    }

    // Duplicate last token, which contains the input file name
    copy = strdup(output_redirect_cmds[tok - 1]);
    token = strtok(copy, "< |\t\n"); // tokenize to extract the input file name, ignoring some delimiters
    out_file = strdup(token);        // assign file name

    tok = 0;
    token = strtok(output_redirect_cmds[0], CMD_DELIMS);

    while (token != NULL)
    {
      cmd_tokens[tok++] = token;
      token = strtok(NULL, CMD_DELIMS);
    }

    free(output_redirect_cmds);
  }
  // no redirection
  if (input_redi == 0 && output_redi == 0)
    return parse_command(strdup(cmd), cmd_tokens);
  else
    return tok; // Return command token count
}

/*
- Duplicate the command string to preserve the original.
- Tokenize the command by the pipe symbol '|'.
- Store each tokenized command segment in the pipe_cmds array.
- Set pipe_num to the total number of pipe-separated segments.
*/
void parse_for_piping(char *cmd)
{
  char *copy_cmd = strdup(cmd);
  char *token;
  int tok = 0;
  token = strtok(copy_cmd, "|");
  while (token != NULL)
  {
    pipe_cmds[tok++] = token;
    token = strtok(NULL, "|");
  }
  pipe_num = tok;
}
