#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/fcntl.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"

// stat a symbolic link using O_NOFOLLOW
static int
stat_slink(char *pn, struct stat *st)
{

  //  printf("funny\n");  
  int fd = open(pn, O_RDONLY | O_NOFOLLOW);

  
  if(fd < 0)
    return -1;
  if(fstat(fd, st) != 0)
    return -1;
  return 0;
}

int main(){
  unlink("/fuck/y");
  unlink("/fuck/z");
  mkdir("/fuck");
  int fd;
  struct stat st;
  fd = open("/fuck/z", O_CREATE | O_RDWR);
  close(fd);
  
          printf("create\n");

  symlink("/fuck/z","/fuck/y");

  stat_slink("/fuck/y",&st);

          printf("unlink\n");
  unlink("/fuck/y");

          printf("unlink\n");
  unlink("/fuck/y");

          printf("create\n");
  symlink("/fuck/z","/fuck/y");
  stat_slink("/fuck/y",&st);
  printf("finish\n");
  exit(0);
}
