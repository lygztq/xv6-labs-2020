#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
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


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
extern pte_t *walk(pagetable_t, uint64, int);
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  static char kbuf[512]; // 4096 / 8
  memset(kbuf, 0, 512);

  uint64 va, ubuf, currva;
  int npages;
  char *bufptr;
  uint8 bitnum;
  struct proc *p = myproc();

  if (argaddr(0, &va) < 0 || argint(1, &npages) < 0 || argaddr(2, &ubuf) < 0)
    return -1;
  
  // the upper limit on the number of pages that can be scanned
  // the actural limit is about 20000 pages.
  if (npages > 4096) return -1;

  // for all virtual pages
  for (int pidx = 0; pidx < npages; ++pidx) {
    currva = va + pidx * PGSIZE;
    pte_t *pte = walk(p->pagetable, currva, 0);
    if ((PTE_FLAGS(*pte) & PTE_A) != 0) {
      bufptr = (char *)kbuf + pidx / 8;
      bitnum = ((uint8)1) << (pidx % 8);
      *bufptr = (uint8)(*bufptr) | (char)bitnum;
    }
    *pte = *pte & (~PTE_A); // clear PTE_A after checking if it is set
  }
  copyout(p->pagetable, ubuf, (char *)kbuf, (npages - 1) / 8 + 1);
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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
