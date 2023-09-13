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
} kmem[NCPU];

char *CPU_NAMES[] = {
        "kmem_cpu_0",
        "kmem_cpu_1",
        "kmem_cpu_2",
        "kmem_cpu_3",
        "kmem_cpu_4",
        "kmem_cpu_5",
        "kmem_cpu_6",
        "kmem_cpu_7",
};

void
kinit() {
    for (int i = 0; i < NCPU; ++i) {
        initlock(&kmem[i].lock, CPU_NAMES[i]);
    }
    freerange(end, (void *) PHYSTOP);
}


void
freerange(void *pa_start, void *pa_end) {
    char *p;
    p = (char *) PGROUNDUP((uint64) pa_start);
    for (; p + PGSIZE <= (char *) pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa) {
    struct run *r;

    if (((uint64) pa % PGSIZE) != 0 || (char *) pa < end || (uint64) pa >= PHYSTOP)
        panic("kfree");
    push_off();
    int id = cpuid();
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run *) pa;

    acquire(&kmem[id].lock);
    r->next = kmem[id].freelist;
    kmem[id].freelist = r;
    release(&kmem[id].lock);
    pop_off(); //开中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void) {
    struct run *r;
    push_off();
    int id = cpuid();

    if (kmem[id].freelist != 0) {  //不为0，就是说还有剩余page，直接分配
        acquire(&kmem[id].lock);
        r = kmem[id].freelist;
        if (r)
            kmem[id].freelist = r->next;
        release(&kmem[id].lock);
        if (r)
            memset((char *) r, 5, PGSIZE); // fill with junk
        pop_off();
        return (void *) r;
    } else {
        //没有page了就去偷page
        int steal_num = 10;
        for (int i = 0; i < NCPU; i++) {
            if (i == id) continue;  //偷其他人的
            acquire(&kmem[i].lock);
            struct run *fs = kmem[i].freelist;
            while (fs != 0 && steal_num != 0) {
                kmem[i].freelist = fs->next;
                fs->next = kmem[id].freelist;
                kmem[id].freelist = fs;
                fs = kmem[i].freelist;
                steal_num--;
            }
            release(&kmem[i].lock);
            if (steal_num == 0) break;
        }

        acquire(&kmem[id].lock);
        r = kmem[id].freelist;
        if (r)
            kmem[id].freelist = r->next;
        release(&kmem[id].lock);
        if (r)
            memset((char *) r, 5, PGSIZE); // fill with junk
        pop_off();
        return (void *) r;

    }

    return (void *)r;
}
