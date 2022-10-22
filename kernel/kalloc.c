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

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void
kinit()
{
  char lock_name[6] = {'k','m','e','m','0','\0'};
  for(int i = 0;i < NCPU;i++){
    lock_name[4] += i;
    initlock(&kmems[i].lock, lock_name);
  }
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

// int init_cpuid;//定义初始的cpuid,令这个id在0~NCPU-1的范围内循环迭代，使得各个cpu对应的
//                   //freelist能够分配到相对平均的内存空间
// int flag = 0;
void
kfree(void *pa)
{
  struct run *r;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off(); //关闭中断获取当前cpu的id号
  int cur_cpuid = cpuid();
  pop_off();
  acquire(&kmems[cur_cpuid].lock);
  r->next = kmems[cur_cpuid].freelist;//每一次都将新的pa添加到链表的头部，并将freelist指向链表头部
  kmems[cur_cpuid].freelist = r;
  release(&kmems[cur_cpuid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off(); //关闭中断获取当前cpu的id号
  int cur_cpuid = cpuid();
  pop_off();
  for(int i = 0;i < NCPU;i++){
    int id = (cur_cpuid+i)%NCPU;  //每次都从当前cpu开始往后循环取，保证每个cpu对应的freelist都以同等的概率被取到，以减少竞争
    acquire(&kmems[id].lock);
    r = kmems[id].freelist;
    if(r)
      kmems[id].freelist = r->next;
    release(&kmems[id].lock);
    if(r){
      memset((char*)r, 5, PGSIZE); // fill with junk
      break;
    }
  }
  return (void*)r;
}