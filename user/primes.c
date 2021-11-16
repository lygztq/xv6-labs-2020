#include "kernel/types.h"
#include "user/user.h"

void pipeline(int left_fd) {
  static char buff[4];

  // read n (a prime) from parent
  if (read(left_fd, buff, 4) == 0)
    exit(0);
  int n = *((int*)buff);
  printf("prime %d\n", n);

  // create following pipe
  int p[2];
  pipe(p);

  // process following numbers
  int has_right = 0;
  while (read(left_fd, buff, 4)) {
    int p_num = *((int*)buff);
    if (p_num % n) {
      has_right = 1;
      write(p[1], (void*)buff, 4);
    }
  }
  close(p[1]);

  if (has_right) {
    if (fork() == 0) {
      pipeline(p[0]);
    } else {
      wait(0);
    }
  }
  exit(0);
}

int main(int argc, char *argv[]) {
  int p[2];
  pipe(p);
  int i = 2;
  for (; i < 36; ++i) {
    write(p[1], (void*)(&i), 4);
  }
  // close should be put here because
  // we are transmitting int, not string,
  // no EOF for int. If we do not close
  // write end before read end finishs reading
  // the child process will wait input forever.
  close(p[1]);

  if (fork() == 0) {
    pipeline(p[0]);
  } else {
    wait(0);
  }
  exit(0);
}