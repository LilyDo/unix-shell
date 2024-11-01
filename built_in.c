#include "header.h"

/*
   Changes the command prompt to the specified new prompt,
*/
void change_prompt(char *new_prompt)
{
  if (new_prompt != NULL)
  {
    strncpy(prompt, new_prompt, MAX_BUF_LEN - 1);
    prompt[MAX_BUF_LEN - 1] = '\0';
  }
}

/*
   Prints the current working directory if no additional arguments are given;
   otherwise, executes the command with the provided tokens.
*/
void pwd(char **cmd_tokens)
{
  char pwd_dir[MAX_BUF_LEN];
  getcwd(pwd_dir, MAX_BUF_LEN - 1);
  if (cmd_tokens[1] == NULL)
    printf("%s\n", pwd_dir);
  else
    execute_command(cmd_tokens);
}

/*
 * Changes the directory based on command tokens, defaulting to base if none or "~" is given.
 * Updates the current directory path; returns 0 on success, -1 on failure.
 */
int cd(char **cmd_tokens, char *cwd, char *base_dir)
{
  if (cmd_tokens[1] == NULL || strcmp(cmd_tokens[1], "~\0") == 0 || strcmp(cmd_tokens[1], "~/\0") == 0)
  {
    chdir(base_dir);
    strcpy(cwd, base_dir);
    update_cwd_relative(cwd);
    return 0;
  }
  else if (chdir(cmd_tokens[1]) == 0)
  {
    getcwd(cwd, MAX_BUF_LEN);
    update_cwd_relative(cwd);
    return 0;
  }
  else
  {
    perror("Error executing cd command");
    return -1;
  }
}

int history_index = 0;
int history_count = 0;

/*
   Adds a command to the history, updating the index and count,
   and ensuring the command is null-terminated.
*/
void add_to_history(char *cmd)
{
  strncpy(history[history_index], cmd, MAX_BUF_LEN - 1);
  history[history_index][MAX_BUF_LEN - 1] = '\0';

  history_index = (history_index + 1) % MAX_HISTORY;
  if (history_count < MAX_HISTORY)
  {
    history_count++;
  }
}

/*
   Prints the command history in order, starting from the oldest command.
*/
void print_history(void)
{
  printf("\nCommand History:\n");

  int start = (history_count < MAX_HISTORY) ? 0 : history_index;
  for (int i = 0; i < history_count && i < MAX_HISTORY; i++)
  {
    int index = (start + i) % MAX_HISTORY;
    printf("%d: %s", i + 1, history[index]);
  }
}

/*
   Searches the command history in reverse to find the most recent command
   starting with the given prefix. Returns the command or NULL if not found.
*/
char *find_command_by_prefix(char *prefix)
{
  size_t prefix_len = strlen(prefix);
  for (int i = history_count - 1; i >= 0; i--)
  {
    int index = (history_index - 1 - i + MAX_HISTORY) % MAX_HISTORY;
    if (strncmp(history[index], prefix, prefix_len) == 0)
    {
      return history[index];
    }
  }
  return NULL;
}
