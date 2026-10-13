#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

addr_t
sys_sbrk(void)
{
  addr_t addr;
  addr_t n;

  argaddr(0, &n);
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

#define MMAP_FAILED ((~0lu))
  addr_t
sys_mmap(void)
{
  int fd, flags;
  struct file *f;
  addr_t start;

  if(argint(0,&fd) < 0 || argint(1,&flags) < 0)
    return MMAP_FAILED;

  // 1. Validate fd -- same check as argfd() line 7477
  if(fd < 0 || fd >= NOFILE || (f = proc->ofile[fd]) == 0)
    return MMAP_FAILED;
  if(f->type != FD_INODE)          // pipes have no inode / no size
    return MMAP_FAILED;

  // 2. Room for another mapping? (mmaps[] has 10 slots)
  if(proc->mmapcount >= NELEM(proc->mmaps))
    return MMAP_FAILED;

  // 3. Reserve address space -- same shape as sys_sbrk (3950-3961)
  start = (addr_t)proc->mmaptop;                    // old top = where this mapping begins
  proc->mmaps[proc->mmapcount].fd = fd;             // remember what's mapped where
  proc->mmaps[proc->mmapcount].start = start;
  proc->mmapcount++;
  proc->mmaptop += PGROUNDUP(f->ip->size);          // next mapping starts on the next page

  // Stage 3 goes here: if(flags == 0) allocate + fill the pages now
  return start;
}

  int
handle_pagefault(addr_t va)
{
  // TODO: your code here
  return 0;
}
