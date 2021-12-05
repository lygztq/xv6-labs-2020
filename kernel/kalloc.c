// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "phypagecnt.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
uint8 pagerefcount[NUM_PHY_PAGES];
struct spinlock pgcntlock;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint8 getpgcnt(uint64 ppa)
{
  uint8 res = pagerefcount[PPA2ID(ppa)];
  return res;
}

void resetpgcnt(uint64 ppa)
{
  acquire(&pgcntlock);
  pagerefcount[PPA2ID(ppa)] = 0;
  release(&pgcntlock);
}

void incpgcnt(uint64 ppa, uint8 val)
{
  acquire(&pgcntlock);
  pagerefcount[PPA2ID(ppa)] += val;
  release(&pgcntlock);
}

void decpgcnt(uint64 ppa, uint8 val)
{
  acquire(&pgcntlock);
  pagerefcount[PPA2ID(ppa)] -= val;
  release(&pgcntlock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pgcntlock, "pgcntlock");
  memset(pagerefcount, PGCNTUNINIT, NUM_PHY_PAGES);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  uint8 pgcnt = getpgcnt((uint64)pa);
  if (pgcnt == 0) {
    return;
  } else if (pgcnt == 1 || pgcnt == PGCNTUNINIT) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  if (pgcnt != PGCNTUNINIT)
    decpgcnt((uint64)pa, 1);
  else
    resetpgcnt((uint64)pa);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    incpgcnt((uint64)r, 1);
  }
  return (void*)r;
}
