#include "types.h"
#include "fcntl.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "file.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();
extern int cowCount[]; // kallc.c

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();



  if(r_scause() == 8){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  }else if(r_scause() == 13 || r_scause() == 15){
    //consider read page fault and check if it is because mmap or simply cow
    //important:
    //difference between mmap and cow
    //mmap should allocate new physical page and store content from file
    //while cow allocate new physical page and store the same content from the
    //origin memory space

	uint64 va = r_stval();
	if(va >= MAXVA){
		printf("usertrap(): read or store page fault: va exceed MAXVA\n");
		setkilled(p);
        goto haskill;
	}
    if(va > p->sz){
		printf("usertrap(): read or store page fault: va exceed process size\n");
		setkilled(p);
        goto haskill;
	}

    int mmap_lazy = 0;
    uint64 mmap_va = PGROUNDDOWN(va);
    for(int i=0;i<VMA_NUM;i++){
        struct vma* vma = &p->vmas[i];
        if(vma->valid && va >= vma->addr && va < vma->addr + vma->length){
            mmap_lazy = 1;
            uint64 new;
	        if((new =(uint64)kalloc()) == 0){
		        printf("usertrap(): read or store page fault: no free memory(in mmap operation)\n");
		        setkilled(p);
                goto haskill;
	        }
            memset((void *)new,0,PGSIZE);
            ilock(vma->f->ip);
            if(readi(vma->f->ip,0,new,vma->offset + mmap_va - vma->addr,PGSIZE) < 0){
                iunlock(vma->f->ip);
                printf("usertrap(): unknown error, read file failure");
		        setkilled(p);
                goto haskill;
            }
            iunlock(vma->f->ip);
            int perm =  PTE_U | (vma->prot & PROT_READ ? PTE_R:0) | (vma->prot & PROT_WRITE ? PTE_W : 0) | (vma->prot & PROT_EXEC ? PTE_X : 0); // PTE_V is added int mappages
            if(mappages(p->pagetable,mmap_va,PGSIZE,new,perm) < 0){
                printf("usertrap(): unknown error, map pages failure");
		        setkilled(p);
                goto haskill;
            }
            break;
        }
    }
if(!mmap_lazy){
    //condition that this is not mmap
	pte_t *pte = walk(p->pagetable,va,0);
	if(pte == 0){
		printf("usertrap(): store page fault: pte not exist\n");
		setkilled(p);
        goto haskill;
	}
	if(((*pte) & (PTE_V | PTE_U)) != (PTE_V | PTE_U)){
		printf("usertrap(): store page fault: permission denied\n");
		setkilled(p);
        goto haskill;
	}
	if(((*pte) & (PTE_OW)) == 0){
		printf("usertrap(): store page fault: read only\n");
		setkilled(p);
        goto haskill;
	}
	
	//alloc new physical memory
	uint64 new;
	if((new =(uint64)kalloc()) == 0){
		printf("usertrap(): store page fault: no free memory\n");
		setkilled(p);
        goto haskill;
	}
	
	uint64 old = PTE2PA(*pte);
	memmove((void *)new,(void *)old,PGSIZE);

	kfree((void *)old);
	*pte = PA2PTE(new) | PTE_FLAGS(*pte) | PTE_W;
    *pte &= ~PTE_OW;
}
  }
  else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

haskill:
  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
    //increase the ticks passed 
  if(which_dev == 2){
    ++p->passed_ticks;
    if(p->trap_in == 0 &&p->alarm_interval!= 0 && p->passed_ticks == p->alarm_interval){
        p->trap_in=1;
        *p->alarmframe = *p->trapframe;
        p->trapframe->epc = p->handler_function;    
        p->passed_ticks = 0;
    }
    yield();
  }

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

