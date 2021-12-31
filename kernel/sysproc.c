#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

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

extern int argfd(int n, int *pfd, struct file **pf);
uint64
sys_mmap(void)
{
  struct proc *p = myproc();
  struct vma *vmap;
  struct file *f;
  uint64 uaddr;
  int len, offset, prot, flags;

  if (argaddr(0, &uaddr) < 0 || argint(1, &len) < 0 ||
      argint(2, &prot) < 0 || argint(3, &flags) < 0 ||
      argfd(4, 0, &f) < 0 || argint(5, &offset) < 0) {
    return -1;
  }
  len = PGROUNDUP(len);

  // 1. should be able to map file opened read-only with private writable
  // 2. doesn't allow read/write mapping of a file opened read-only.
  if ((!f->readable && (prot & PROT_READ)) ||
      (!f->writable && (prot & PROT_WRITE) && (flags & MAP_SHARED)) ||
      f->type != FD_INODE)
    return -1;

  // find a available vma in this proc
  for (vmap = &p->vmas[0]; vmap != &p->vmas[MAX_NVMA]; vmap++) {
    if (vmap->valid) continue;
    vmap->valid = 1;
    vmap->length = len;
    vmap->perm = prot;
    vmap->sharem = flags;
    vmap->f = filedup(f);
    vmap->basefileoff = 0;
    mmap(p->pagetable, vmap);
    break;
  }
  if (vmap == &p->vmas[MAX_NVMA])
    return -1;

  return vmap->addr;
}

uint64
sys_munmap(void)
{
  uint64 uaddr;
  int len;

  if (argaddr(0, &uaddr) < 0 || argint(1, &len) < 0)
    return -1;

  uaddr = PGROUNDDOWN(uaddr);
  len = PGROUNDUP(uaddr + len) - uaddr;
  if (munmap(uaddr, len) < 0)
    return -1;
  
  return 0;
}
