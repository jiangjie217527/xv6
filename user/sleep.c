#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
	if(argc != 2){
        char* msg = "usage: sleep [ticks num]\n";
		write(1,msg,strlen(msg));
		exit(1);
	}
	int ticks = atoi(argv[1]);
	int ret = sleep(ticks);
	exit(ret);
}
