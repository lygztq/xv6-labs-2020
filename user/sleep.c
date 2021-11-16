#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  // If the user forgets to pass an argument
  // sleep should print an error message.
  if(argc < 2){
    fprintf(2, "sleep: an argument (int) should be given.\n");
    exit(1);
  }

  int sleep_time = atoi(argv[1]);
  if (sleep(sleep_time) == -1) {
    fprintf(2, "sleep: error\n");
    exit(1);
  }
  exit(0);
}