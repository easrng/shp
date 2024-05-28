#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;

ssize_t r;
char buf[4096];
int i;
char c;
int in_fileno;
const char begin[14] = "\nprintf '%s' '";
const char end[2] = "'\n";
const char esq[4] = "'\\''";
const char enl[17] = "';printf '\\0%s' '";
void check(int status) {
  if (status)
    _exit(status);
}
int checkn(int status) {
  if (status < 0)
    _exit(errno);
  return status;
}
void buf_getchar() {
  if (i + 1 < r) {
    c = buf[++i];
  } else {
    checkn(read(in_fileno, &c, 1));
  }
}
int main(int argc, char *argv[]) {
  pid_t pid;
  posix_spawn_file_actions_t child_fd_actions;
  int pipefd[2];
  char **child_cmd = calloc(argc + 1, sizeof(char *));
  child_cmd[0] = "/bin/sh";
  child_cmd[1] = "/dev/stdin";
  for (i = 2; i < argc; i++)
    child_cmd[i] = argv[i];
  check(pipe(pipefd));
  check(posix_spawn_file_actions_init(&child_fd_actions));
  check(posix_spawn_file_actions_adddup2(&child_fd_actions, pipefd[0], 0));
  check(posix_spawn_file_actions_addclose(&child_fd_actions, pipefd[1]));
  check(posix_spawn_file_actions_addclose(&child_fd_actions, pipefd[0]));
  check(posix_spawn(&pid, child_cmd[0], &child_fd_actions, NULL, child_cmd,
                    environ));
  free(child_cmd);
  if (argc > 1) {
    in_fileno = checkn(open(argv[1], O_RDONLY));
  } else {
    in_fileno = STDIN_FILENO;
  }
  char mode = 2;
  while ((r = read(in_fileno, buf, sizeof buf)) > 0) {
    ssize_t last_write = 0;
    if (mode) {
      checkn(write(pipefd[1], begin, sizeof begin));
    }
    for (i = 0; i < r; i++) {
      if (mode == 2) {
        mode = 1;
        if (buf[i] == '#') {
          buf_getchar();
          if (c == '!') {
            do {
              buf_getchar();
            } while (c != '\n');
            last_write = i + 1;
          }
        }
      }
      if (mode) {
        if (buf[i] == '<') {
          buf_getchar();
          if (c != '?') {
            checkn(write(pipefd[1], buf + last_write, i - last_write));
            if (c == '\'') {
              checkn(write(pipefd[1], esq, sizeof esq));
            } else if (c) {
              checkn(write(pipefd[1], &c, 1));
            } else {
              checkn(write(pipefd[1], enl, sizeof enl));
            }
            last_write = i + 1;
          } else {
            checkn(write(pipefd[1], buf + last_write, i - 1 - last_write));
            checkn(write(pipefd[1], end, sizeof end));
            last_write = i + 1;
            mode = 0;
          }
        } else if (buf[i] == '\'') {
          checkn(write(pipefd[1], buf + last_write, i - last_write));
          checkn(write(pipefd[1], esq, sizeof esq));
          last_write = i + 1;
        } else if (buf[i] == 0) {
          checkn(write(pipefd[1], buf + last_write, i - last_write));
          checkn(write(pipefd[1], enl, sizeof enl));
          last_write = i + 1;
        }
      } else if (buf[i] == '?') {
        buf_getchar();
        if (c != '>') {
          checkn(write(pipefd[1], buf + last_write, i - last_write));
          checkn(write(pipefd[1], &c, 1));
          last_write = i + 1;
        } else {
          checkn(write(pipefd[1], buf + last_write, i - 1 - last_write));
          checkn(write(pipefd[1], begin, sizeof begin));
          last_write = i + 1;
          mode = 1;
        }
      }
    }
    checkn(write(pipefd[1], buf + last_write, i - last_write));
    if (mode) {
      checkn(write(pipefd[1], end, sizeof end));
    }
  }
  close(pipefd[1]);
  return waitpid(pid, &i, 0) || i;
}
