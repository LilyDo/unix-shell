#include "header.h"

/*
   Opens the specified input file for reading, duplicates its file descriptor
   to standard input, and returns the file descriptor. Prints an error if opening fails.
*/
int open_input_file()
{
  int f = open(in_file, O_RDONLY, S_IRWXU);
  if (f < 0)
  {
    perror(in_file);
  }
  dup2(f, STDIN_FILENO);
  close(f);
  return f;
}

/*
   Opens the output file for writing (truncate/append), redirects to stdout,
   and returns the file descriptor. Prints error if opening fails.
*/
int open_output_file()
{
  int f;
  if (output_redi_type == 1)
    f = open(out_file, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
  else if (output_redi_type == 2)
    f = open(out_file, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
  if (f < 0)
  {
    perror(out_file);
  }
  dup2(f, STDOUT_FILENO);
  close(f);
  return f;
}
