#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
#ifdef LAB_TRAPS
  backtrace();
#endif
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 addr;
  int num,bufflen;
  uint64 destaddr;
  pte_t *pte;
  int total = 0;

  pagetable_t pagetable = myproc()->pagetable;

  argaddr(0,&addr);
  argint(1,&num);
  argaddr(2,&destaddr);

  bufflen = num/8 + (num % 8 != 0);
  char tmp[bufflen];
  for(int i=0;i<bufflen;i++)tmp[i] = 0;
  for(int i=0;i<bufflen;i++){
    for(int j=0;j<8;j++){
    	pte = walk(pagetable,addr + (i * 8 + j) * PGSIZE,0);
        if((*pte) & PTE_A){
    	    tmp[i] |= (1 <<j);
	    total ++;
        }
	else{	
    	    tmp[i] &= ~(1 <<j);
	}
	(*pte) &= ~PTE_A;
    }
  }
  //for(int i=0;i<bufflen;i++){
  //  printf("%p\n",tmp[i]);
  //}
  if(copyout(pagetable,destaddr,tmp,sizeof(tmp)) < 0){// copyput的时候。从低位开始
    return -1;
  }

  return total;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void){
    int trace_sys_mask;
    argint(0,&trace_sys_mask);
    myproc()->tracemask = trace_sys_mask;
    return 0;
}

uint64
sys_sysinfo(void){
    uint64 destaddr;
    argaddr(0,&destaddr);
    //get the argument
    
    struct sysinfo info;
    info.freemem = kfreemem();
    info.nproc = count_free_proc();

    if(copyout(myproc()->pagetable,destaddr,(char *)&info,sizeof (info))<0){
        return -1;
    }
    return 0;
}

uint64
sys_sigalarm(void){
    int interval;
    uint64 handler;
    argint(0,&interval);
    argaddr(1,&handler);
    myproc()->passed_ticks=0;
    myproc()->alarm_interval=interval;
    myproc()->handler_function=handler;
    return 0;
}

uint64
sys_sigreturn(void){
    struct proc *p = myproc();
    *p->trapframe = *p->alarmframe;
    p->trap_in = 0;
    p->passed_ticks = 0;
    return (*p->trapframe).a0;
}
