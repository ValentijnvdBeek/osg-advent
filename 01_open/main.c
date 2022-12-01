#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

void print_error() {
  char *error = "\nCat failed: ";
  char *msg = strerror(errno);
  write(2, error, 13);
  write(2, msg, strlen(msg));
  write(2, "\n", 1);
  exit(1);
}

void read_file(int fd) {
  struct stat file_stat;
  if (fstat(fd, &file_stat) == -1)	print_error();
  
  char *buf = malloc(file_stat.st_size);
  int res = read(fd, buf, file_stat.st_size);

  if (res == 0) return;
  if (res == -1) print_error();

  buf[res] = '\0';  
  write(0, buf, res+1);
  free(buf);
}

int main(int argc, char **argv, char **envp) {
  if (argc == 1) while (true) read_file(1);
  
  for (int i = 1; i < argc; i++) {
	int fd = open(argv[i], O_RDONLY);
	read_file(fd);
	close(fd);
  }
   
}
