#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define WORD_SIZE 32

int parse_args(char **ptr, char *buff) {
    char *p = *ptr;
    if (strlen(p) == 0) return -1;
    while (*p != ' ' && *p != '\n' && *p != '\0') {
        *buff = *p; ++buff; ++p;
    }
    *buff = '\0';
    int ret = 0;
    switch (*p) {
        case '\n':
            ret = 1;
        case ' ':
            ++p; break;
        case '\0':
            ret = -1; break;
    }
    *ptr = p;
    return ret;
}

int main(int argc, char *argv[]) {
    char *exec_args[MAXARG];
    char buff[MAXARG * WORD_SIZE];
    memset(exec_args, 0, (uint)MAXARG);
    if (argc < 2) {
        fprintf(2, "xargs: need command");
        exit(1);
    }

    // first read command args
    int cmd_idx = 0;
    for (; cmd_idx < argc - 1; ++cmd_idx) {
        if (!exec_args[cmd_idx]) exec_args[cmd_idx] = malloc(strlen(argv[cmd_idx + 1]) + 1);
        strcpy(exec_args[cmd_idx], argv[cmd_idx + 1]);
    }

    // read from std
    int len = read(0, buff, sizeof(buff));
    if (len <= 0) exit(0);
    if (buff[len - 1] == '\n') --len;
    buff[len] = '\0';
    char *p = buff;

    int curr_idx = cmd_idx;
    while (1) {
        if (!exec_args[curr_idx]) exec_args[curr_idx] = malloc(WORD_SIZE);
        int parse_ret = parse_args(&p, exec_args[curr_idx]);
        if (parse_ret == 0) {
            ++curr_idx;
            continue;
        } else {
            curr_idx = cmd_idx;
            if (fork() == 0) {
                exec(exec_args[0], exec_args);
                exit(0);
            } else {
                wait(0);
            }
            if (parse_ret == -1) break;
        }

    }

    for (int i = 0; i < MAXARG; ++i) {
        if (exec_args[i]) free(exec_args[i]);
    }

    exit(0);
}
