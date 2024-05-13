// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem;

int cowCount[PHYSTOP / PGSIZE];
struct spinlock cowlock;
void
kinit()
{
    char name[8] = {'k','m','e','m','_','!','\0'};
    for(int i=0;i<NCPU;i++){
      name[5] = i+'0';
      initlock(&kmem.lock[i], name);
    }
    freerange(end, (void*)PHYSTOP);
    initlock(&cowlock,"cowlock");
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
	cowCount[(uint64)p/PGSIZE] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int id,remain;
  acquire(&cowlock);
    remain = --cowCount[(uint64)pa/PGSIZE];
  release(&cowlock);

  if(remain)return;

  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  id = cpuid();
  acquire(&kmem.lock[id]);
  r->next = kmem.freelist[id];
  kmem.freelist[id] = r;
  release(&kmem.lock[id]);
  pop_off();  
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int org = cpuid(),id;
  id = org;
  while(1){  
      acquire(&kmem.lock[id]);
      r = kmem.freelist[id];
      if(r){
        kmem.freelist[id] = r->next;
        release(&kmem.lock[id]);
        break;
      }
      release(&kmem.lock[id]);
      //steal from the next CPU, until get one or none left
      id = (id + 1)%NCPU;
      if(id == org){
        break;
      }
  }
  pop_off();
  if(r){
	cowCount[(uint64)r/PGSIZE] = 1;
    memset((char*)r, 5, PGSIZE); // fill with junk
  }

  return (void*)r;
}

uint64
kfreemem(void){
    struct run *r;
    uint64 ans = 0;

    for(int id = 0;id < NCPU;id++){
        acquire(&kmem.lock[id]);
        r = kmem.freelist[id];
        while(r){
            ans += PGSIZE;
            r = r->next;
        }
        release(&kmem.lock[id]);
    }
    return ans;
}

void
add_count(uint64 pa){
    acquire(&cowlock);
    cowCount[pa/PGSIZE]++;
    release(&cowlock);
}
