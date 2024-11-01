#include "header.h"

/*
   Opens the specified input file for reading, duplicates its file descriptor
   to standard input, and returns the file descriptor. Prints an error if opening fails.
*/
int open_input_file(void)
{
  int fd = open(in_file, O_RDONLY, S_IRWXU); // open in read-only mode
  if (fd < 0)
  {
    perror(in_file);
    return fd;
  }

  if (dup2(fd, STDIN_FILENO) < 0) // Duplicate the file descriptor to standard input
  {
    perror("dup2 failed");
    close(fd);
    return -1;
  }
  close(fd);
  return STDIN_FILENO;
}

/*
   Opens the output file for writing (truncate/append), redirects to stdout,
   and returns the file descriptor. Prints error if opening fails.
*/
int open_output_file(void)
{
  int fd;
  if (output_redi_type == 1)
    fd = open(out_file, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU); // Opens for writing if exists, or creating if doesn't.
  else if (output_redi_type == 2)
    fd = open(out_file, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU); // Opens for appending if exists, or creating if doesn't.
  else if (output_redi_type == 2)

    if (fd < 0)
    {
      perror(out_file);
      return fd;
    }

  if (dup2(fd, STDOUT_FILENO) < 0) // Duplicate the file descriptor to standard output
  {
    perror("dup2 failed");
    close(fd);
    return -1;
  }

  close(fd);
  return STDOUT_FILENO;
}
