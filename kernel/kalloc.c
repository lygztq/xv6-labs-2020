// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void kpageinit(void *pa, int cid);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  char name[6];
} kmems[NCPU];

void
kinit()
{ 
  char *saddr = (char*)PGROUNDUP((uint64)&end);
  char *eaddr = (char*)PGROUNDDOWN(PHYSTOP);
  uint npages = ((uint64)eaddr - (uint64)saddr) / PGSIZE;
  uint npagespc = npages / NCPU;  // num pages per cpu
  int cid = 0;                    // cpu id
  for (cid = 0; cid < NCPU; ++cid) {
    strncpy(kmems[cid].name, "kmem_", sizeof(kmems[cid].name));
    kmems[cid].name[5] = ('0' + cid);
    initlock(&kmems[cid].lock, kmems[cid].name);

    char *ceaddr = (char *)((cid == (NCPU - 1)) ? eaddr : saddr + PGSIZE * npagespc);
    
    for (; saddr + PGSIZE <= ceaddr; saddr += PGSIZE)
      kpageinit(saddr, cid);
  }
}

void kpageinit(void *pa, int cid) {
  struct run *r;

  if (cid >= NCPU) panic("kpageinit");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmems[cid].lock);
  r->next = kmems[cid].freelist;
  kmems[cid].freelist = r;
  release(&kmems[cid].lock);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cid = cpuid();

  // just put to my list, no metter where does this page come from.
  acquire(&kmems[cid].lock);
  r->next = kmems[cid].freelist;
  kmems[cid].freelist = r;
  release(&kmems[cid].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r = 0;

  push_off();
  int cid = cpuid();
  int cptr = cid;
  do {
    acquire(&kmems[cptr].lock);
    r = kmems[cptr].freelist;
    if (r)
      kmems[cptr].freelist = r->next;
    release(&kmems[cptr].lock);
    cptr = (cptr + 1) % NCPU;
  } while (cptr != cid && !r);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
