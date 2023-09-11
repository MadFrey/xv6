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

int COW_COUNT[PHYSTOP>>12];

struct spinlock cow_lock;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&cow_lock,"cow");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
      COW_COUNT[((uint64)p)/4096] = 1;
      kfree(p);
  }
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
  int count = COW_COUNT_NUM((uint64)pa);
  if(count>1){
      COW_COUNT_DESC((uint64)pa);
      return;
  }

  if(count==1){
      COW_COUNT_DESC((uint64)pa);
  }

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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

  if(r){
      memset((char*)r, 5, PGSIZE); // fill with junk
      COW_COUNT_INCR((uint64)r);
  }

  return (void*)r;
}

void COW_COUNT_INCR(uint64 pa){
    acquire(&cow_lock);
    COW_COUNT[pa/4096]++;
    release(&cow_lock);
}

void COW_COUNT_DESC(uint64 pa){
    acquire(&cow_lock);
    COW_COUNT[pa/4096]--;
    release(&cow_lock);
}

int COW_COUNT_NUM(uint64 pa){
    acquire(&cow_lock);
    int index =  COW_COUNT[pa/4096];
    release(&cow_lock);
    return index;
}