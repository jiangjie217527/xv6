#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int pid;
    int p_f_to_s[2], p_s_to_f[2];
    //father to son
    //son to father
    char buf[1] = {0};
    pipe(p_f_to_s);
    pipe(p_s_to_f);
    if (fork() == 0) {
        pid = getpid();
        close(p_f_to_s[1]);
        close(p_s_to_f[0]);
        read(p_f_to_s[0], buf, 1);
        close(p_f_to_s[0]);
        printf("%d: received ping\n", pid);
        write(p_s_to_f[1], buf, 1);
        close(p_s_to_f[1]);
    }
    else {
        pid = getpid();
        close(p_s_to_f[1]);
        close(p_f_to_s[0]);
        write(p_f_to_s[1], buf, 1);
        close(p_f_to_s[1]);
        wait(0);
        read(p_s_to_f[0], buf, 1);
        close(p_s_to_f[0]);
        printf("%d: received pong\n", pid);
    }
    exit(0);
}
