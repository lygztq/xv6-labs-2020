#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "user/user.h"

void find(char *path, char *target) {
  char buf[512], *p; // buffer for path
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_DIR) {
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
      fprintf(2, "find: path too long\n");
      return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0 ||
          strcmp(de.name, ".") == 0 ||
          strcmp(de.name, "..") == 0) continue;

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (strcmp(target, de.name) == 0) {
        printf("%s\n", buf);
      }
      if (stat(buf, &st) < 0) {
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      if (st.type == T_DIR) find(buf, target);
    }
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "find: invalid number of arguments.\n");
    exit(0);
  }

  find(argv[1], argv[2]);
  exit(0);
}
