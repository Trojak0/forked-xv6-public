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

// Allocate one page, fill it from the file, and map it at va.
// start = where this mapping begins; va = page-aligned address inside it.
static int
mmap_page(struct file *f, addr_t start, addr_t va)
{
  char *mem;
  uint off, n;

  if((mem = kalloc()) == 0)              // 1. physical page (kernel virtual addr)
    return -1;
  memset(mem, 0, PGSIZE);                // 2. zero it (kfree left junk; tail past EOF)

  off = va - start;                      // 3. which part of the file goes in this page
  n = f->ip->size - off;
  if(n > PGSIZE)
    n = PGSIZE;                          //    last page may be partial

  ilock(f->ip);                          // 4. read file bytes into the page
  if(readi(f->ip, mem, off, n) != n){    //    (same as loaduvm line 2169)
    iunlock(f->ip);
    kfree(mem);
    return -1;
  }
  iunlock(f->ip);

  if(mappages(proc->pgdir, (char*)va, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
    kfree(mem);                          // 5. user va -> physical page, writable, user
    return -1;
  }
  return 0;
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

  // 2. Room for another mapping?
  if(proc->mmapcount >= NELEM(proc->mmaps))
    return MMAP_FAILED;

  start = (addr_t)proc->mmaptop;

  // 3. Eager: allocate + fill every page now
  if(flags == 0){
    addr_t va;
    for(va = start; va < start + PGROUNDUP(f->ip->size); va += PGSIZE){
      if(mmap_page(f, start, va) < 0){
        deallocuvm(proc->pgdir, va, start);   // free pages [start, va) mapped so far
        return MMAP_FAILED;
      }
    }
  }

  // 4. Bookkeeping (same as Stage 2)
  proc->mmaps[proc->mmapcount].fd = fd;
  proc->mmaps[proc->mmapcount].start = start;
  proc->mmapcount++;
  proc->mmaptop += PGROUNDUP(f->ip->size);

  return start;
}

  int
handle_pagefault(addr_t va)
{
  int i;
  struct file *f;
  addr_t start, end;

  if(proc == 0)                       // fault with no process (kernel bug) → not ours
    return 0;

  for(i = 0; i < proc->mmapcount; i++){
    start = proc->mmaps[i].start;
    f = proc->ofile[proc->mmaps[i].fd];   // get the file back via the saved fd
    if(f == 0 || f->type != FD_INODE)     // fd was closed → can't fill the page
      continue;
    end = start + PGROUNDUP(f->ip->size);

    if(va >= start && va < end){          // 1. is va inside mapping i?
      if(mmap_page(f, start, PGROUNDDOWN(va)) < 0)  // 2. allocate + fill + map that one page
        return 0;                         //    out of memory → let it be killed
      return 1;                           // 3. handled: CPU retries the instruction
    }
  }
  return 0;                               // not in any mapping → bad access (bad.c)
}