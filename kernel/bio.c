// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define HASHSIZE 3
#define BUCKETSIZE 10
const int Hash = HASHSIZE;
struct {
  struct spinlock lock;
  struct buf buf[BUCKETSIZE];
} bcache[HASHSIZE];

void
binit(void)
{
  char name[10] = {'b','c','a','c','h','e','_','!','\0'};
  for(int i=0;i<Hash;i++){
    name[7] = i + '0';
    initlock(&bcache[i].lock, name);
    for(int j=0;j<BUCKETSIZE;j++){
        initsleeplock(&bcache[i].buf[j].lock, "buffer");
    }
  }
}
/*
int
btot(void){
    int tot = 0;
  struct buf *b;
    for(int i=0;i<HASHSIZE;i++)
        for(int j=0;j<BUCKETSIZE;j++){
            b = &(bcache[i].buf[j]);
            if(b->refcnt) tot++;
        }
    return tot;
}
*/
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int find = 0;
  int id = blockno%Hash;

  acquire(&bcache[id].lock);
  for(int i=0;i<BUCKETSIZE;i++){
      b = &(bcache[id].buf[i]);
      if(b->dev == dev && b->blockno == blockno){
          b->refcnt ++;
          find = 1;
          break; 
      }
  }
  if(find == 0){
#ifdef LAB_FS_UNUSE
    printf("get the first cache");
    b = &(bcache[id].buf[0]);    
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    find = 1;
#else
  for(int i=0;i<BUCKETSIZE;i++){
    b = &(bcache[id].buf[i]);
    if(b->refcnt == 0){
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        find = 1;
        break;
      }
    }
#endif
}
  release(&bcache[id].lock);
  if(find == 0)
    panic("bget: no unused buffer for recycle"); 
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  int bucket = b->blockno%Hash;
  acquire(&bcache[bucket].lock);
  releasesleep(&b->lock);
  b->refcnt--;
  release(&bcache[bucket].lock);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % Hash;
  acquire(&bcache[bucket].lock);
  b->refcnt++;
  release(&bcache[bucket].lock);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % Hash;
  acquire(&bcache[bucket].lock);
  b->refcnt--;
  release(&bcache[bucket].lock);
}


