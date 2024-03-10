#include "kernel/types.h"
#include "user/user.h"

int main(){
    int p[2];
    /*
    char *argv[2];

    argv[0] = "wc";
    argv[1] = 0;
    */
    char buf[64];
    pipe(p);


    //close(p[1]);
    int write_n = write(p[1],"aaa\n",4);
    int n = read(p[0],buf,64);
    
    printf("write n=%d,read n = %d\n",write_n,n);
    printf("buf=%s=\n",buf);
    //child
    /*
    if(fork() == 0) {
        //close(0);
        //dup(p[0]);
        //write at p[0],
        //father receive at p[1]
        //
        // if you don't want to write or if you have finish
        // write, then you had better close this end
        // One the other size, if you 
        //printf("child write");
        //close(p[1]);
        //write(p[0],"111\n",4);

        //close(p[0]);
        //printf("child finish");
        //close(p[1]);
        //exec("wc",argv);
        
        int n = read(p[0],buf,64);
        printf("read result = %d\n",n);
        printf("read %s\n",buf);
        //n = write(p[0],"111\n",4);
        printf("write result = %d\n",n);
        close(p[1]);
    }// farther
    else{
        //close(p[0]);
        //printf("father start");
        //wait(0);
        //printf("child finish");
        //read(p[1],buf,64);
        //printf("buf=%s\n",buf);
        //wait(0);
        //close(p[0]);
        //
        int n = write(p[1],"helloworld\n",12);
        wait(0);
        printf("father write result = %d\n",n);
        close(p[0]);
        n = read(p[0],buf,64);
        printf("read result = %d\n",n);
        printf("read %s\n",buf);
    }
    */
    exit(0);
}
