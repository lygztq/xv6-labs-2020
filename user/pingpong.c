#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int ping[2], pong[2];
  char byte = 'a';
  if (pipe(ping) < 0) {
    fprintf(2, "pipe error for ping\n"); exit(1);
  }
  if (pipe(pong) < 0) {
    fprintf(2, "pipe error for pong\n"); exit(1);
  }

  int pid = fork();
  int curr_pid = getpid();
  if (pid == 0) {
    // child
    close(pong[0]);
    close(ping[1]);

    read(ping[0], &byte, sizeof(byte));
    printf("%d: received ping\n", curr_pid);
    
    write(pong[1], &byte, sizeof(byte));
    close(pong[1]);
  } else if (pid > 0) {
    // parent
    close(ping[0]);
    close(pong[1]);

    write(ping[1], &byte, sizeof(byte));
    close(ping[1]);
    
    wait(0);
    
    read(pong[0], &byte, sizeof(byte));
    printf("%d: received pong\n", curr_pid);
  } else {
    fprintf(2, "fork error!\n");
    exit(1);
  }

  // must exit(0) here
  exit(0);
}
