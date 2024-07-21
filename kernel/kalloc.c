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
  struct spinlock lock;
  struct run *freelist;
  char lockName[8];//用于记录锁的名称
} kmems[NCPU];

void
kinit()
{
  for(int i=0;i<NCPU;i++){
    snprintf(kmems[i].lockName, 8, "kmem_%d", i); //设置锁的名称
    initlock(&kmems[i].lock, kmems[i].lockName);
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
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  // 释放
  r = (struct run*)pa;
  push_off();//关闭中断，禁止CPU切换
  int id = cpuid();//得到当前CPU的ID
  pop_off();

  acquire(&kmems[id].lock);
  r->next =kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void* kalloc(void) {
    struct run *r;
    push_off(); // 禁用中断
    int id = cpuid(); // 获取当前 CPU ID

    acquire(&kmems[id].lock);
    r = kmems[id].freelist;
    if (r) {
        kmems[id].freelist = r->next;
    } else if ((r = steal(id))) {
        kmems[id].freelist = r->next;
    }
    release(&kmems[id].lock);
    pop_off(); // 恢复中断

    if (r)
        memset((char*)r, 5, PGSIZE); // 用垃圾填充
    return (void*)r;
}

struct run* steal(int cpuID) {
    int find = 0, i = 0;
    struct run *r;
    if (cpuID != cpuid()) {
        panic("steal");
    }

    for (i = 0; i < NCPU; i++) {
        if (i == cpuID) {
            continue;
        } else {
            acquire(&kmems[i].lock);
            r = kmems[i].freelist;
            if (r) {
                find = 1;
                break;
            }
            release(&kmems[i].lock);
        }
    }

    if (find == 0) {
        return 0;
    }

    // 分割 free list
    struct run *fast, *slow, *head;
    fast = slow = head = kmems[i].freelist;
    while (fast&&fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    kmems[i].freelist = slow->next;
    release(&kmems[i].lock);
    slow->next = 0;
    return head;
}
