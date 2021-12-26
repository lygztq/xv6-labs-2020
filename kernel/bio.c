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

extern struct spinlock tickslock;
extern uint ticks;

// buffer bin
struct bbin {
  struct spinlock lock;
  struct buf* head;
  int allocing;
  char name[9];
};

// Detach a buffer from a bin's list.
// Caller should hold the bin, and b is not in use.
// Note: b must in bin
inline void
detach_buf(struct bbin* bin, struct buf* b) {
  if (b->refcnt != 0)
    panic("detach_buf");
  
  // if is head
  if (b == bin->head)
    bin->head = b->next;

  if (b->prev)
    b->prev->next = b->next;
  if (b->next)
    b->next->prev = b->prev;
  b->next = 0;
  b->prev = 0;
}

// Insert a buffer into bin
// Caller should hold the bin (except for binit), and b must not belong to any bin.
// Note: buf must not in bin
inline void
insert_bbin(struct bbin* bin, struct buf* b) {
  b->next = bin->head;
  if (bin->head) {
    bin->head->prev = b;
  }
  b->prev = 0;
  bin->head = b;
}

// Pop the least recently used buffer from bin.
// Caller should hold bin
struct buf*
pop_bbin(struct bbin* bin) {
  struct buf* ptr = bin->head;
  struct buf* topop = 0;
  uint min_tick = 0;

  while (ptr) {
    if (ptr->refcnt == 0 && (ptr->tick < min_tick || !topop)) {
      topop = ptr;
      min_tick = ptr->tick;
    }
    ptr = ptr->next;
  }
  if (topop)
    detach_buf(bin, topop);
  return topop;
}

struct {
  struct buf buf[NBUF];
  struct bbin bufbins[NBUFBIN];
} bcache;

void
binit(void)
{
  struct bbin* cbin;
  int bufperbin = NBUF / NBUFBIN;
  for (int i = 0; i < NBUFBIN; ++i) {
    cbin = &(bcache.bufbins[i]);
    snprintf(cbin->name, sizeof(cbin->name), "bcache_%d", i);
    initlock(&cbin->lock, cbin->name);
    cbin->allocing = 0;

    int end_bufid = (i == (NBUFBIN - 1)) ? NBUF : (i + 1) * bufperbin;
    for (int bufid = i * bufperbin; bufid < end_bufid; ++bufid) {
      initsleeplock(&bcache.buf[bufid].lock, "buffer");
      insert_bbin(cbin, &bcache.buf[bufid]);
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct bbin *bin;
  uint binid = blockno % NBUFBIN;
  bin = &(bcache.bufbins[binid]);

  acquire(&tickslock);
  uint ctick = ticks;
  release(&tickslock);

  acquire(&bin->lock);
  while (bin->allocing) sleep(bin, &bin->lock);

  // Is the block already cached?
  for(b = bin->head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->tick = ctick;
      release(&bin->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  bin->allocing = 1;
  release(&bin->lock);
  do {
    acquire(&bin->lock);
    b = pop_bbin(bin);
    release(&bin->lock);
    if (b) break;
    bin = bin + 1;
    if (bin == ((struct bbin*)bcache.bufbins + NBUFBIN))
      bin = (struct bbin*)bcache.bufbins;
  } while (bin != &(bcache.bufbins[binid]));

  bin = &bcache.bufbins[binid];
  if (b) {
    // now b is detached from all bins
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->tick = ctick;
    acquire(&bin->lock);
    insert_bbin(bin, b);
    bin->allocing = 0;
    wakeup(bin);
    release(&bin->lock);
    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no buffers");
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
  struct bbin *bin;
  uint binid = b->blockno % NBUFBIN;
  bin = &bcache.bufbins[binid];

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bin->lock);
  b->refcnt--;
  release(&bin->lock);
}

void
bpin(struct buf *b) {
  struct bbin *bin;
  uint binid = b->blockno % NBUFBIN;
  bin = &bcache.bufbins[binid];

  acquire(&bin->lock);
  b->refcnt++;
  release(&bin->lock);
}

void
bunpin(struct buf *b) {
  struct bbin *bin;
  uint binid = b->blockno % NBUFBIN;
  bin = &bcache.bufbins[binid];

  acquire(&bin->lock);
  b->refcnt--;
  release(&bin->lock);
}
