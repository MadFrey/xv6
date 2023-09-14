// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
    struct spinlock lock;
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    struct spinlock lock_map[NBUCKET];
    struct buf head[NBUCKET];
} bcache;

int
hash(int blockno) {
    return blockno % NBUCKET;
}


void
binit(void) {
    struct buf *b;

    initlock(&bcache.lock, "bcache_biglock");
    for (int i = 0; i < NBUCKET; ++i) {
        initlock(&bcache.lock_map[i], "bcache");
    }

    for (int i = 0; i < NBUCKET; i++) {
        bcache.head[i].next = &bcache.head[i];
        bcache.head[i].prev = &bcache.head[i];
    }

    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {   //将全部的块插入到head里
        b->next = bcache.head[0].next;
        b->prev = &bcache.head[0];
        initsleeplock(&b->lock, "buffer");
        bcache.head[0].next->prev = b;
        bcache.head[0].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno) {
    struct buf *b ,*b2=0;
    int key = hash(blockno);
    acquire(&bcache.lock_map[key]);

    // Is the block already cached?
    for (b = bcache.head[key].next; b!= &bcache.head[key] ; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.lock_map[key]);
            acquiresleep(&b->lock);
            return b;
        }
    }
    release(&bcache.lock_map[key]);
    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
    acquire(&bcache.lock);
    acquire(&bcache.lock_map[key]);
    for (b = bcache.head[key].next; b!=&bcache.head[key]; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.lock_map[key]);
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    int min_ticks = 0 ;
//    //先在key下找
//    for (b = bcache.head[key].next; b!=&bcache.head[key]; b = b->next) {
//        if (b->refcnt == 0 && (b2 == 0 || b->lastuse < min_ticks)){       //ticket
//            min_ticks = b->lastuse;
//            b2 = b;
//        }
//    }
//    if( b2 ){  //找到了
//        b2->dev = dev;
//        b2->blockno = blockno;
//        b2->refcnt++;
//        b2->valid = 0;
//        release(&bcache.lock_map[key]);
//        release(&bcache.lock);
//        acquiresleep(&b2->lock);
//        return b2;
//    }

    //如果key下没找到
    for (int i = 0; i < NBUCKET; ++i) {    //遍历所有的桶
        if (i==key){
            continue;
        }
        acquire(&bcache.lock_map[i]);
        for (b = bcache.head[i].next; b!=&bcache.head[i] ; b = b->next) {  //是否符合条件
            if (b->refcnt == 0 && (b2 == 0 || b->lastuse < min_ticks)){       //ticket
               min_ticks = b->lastuse;
                b2 = b;
            }
        }
        if( b2 ){  //找到了
            b2->dev = dev;
            b2->blockno = blockno;
            b2->refcnt++;
            b2->valid = 0;
            //移到key下
            b2->next->prev = b2->prev;
            b2->prev->next = b2->next;
            release(&bcache.lock_map[i]);
            // add block
            b2->next = bcache.head[key].next;
            b2->prev = &bcache.head[key];
            bcache.head[key].next->prev = b2;
            bcache.head[key].next = b2;
            release(&bcache.lock_map[key]);
            release(&bcache.lock);
            acquiresleep(&b2->lock);
            return b2;
        }
        release(&bcache.lock_map[i]);
    }
    release(&bcache.lock_map[key]);
    release(&bcache.lock);
    panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno) {
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);
    int key = hash(b->blockno);
    acquire(&bcache.lock_map[key]);
    b->refcnt--;
    if (b->refcnt == 0) {
        b->lastuse = ticks;
    }

    release(&bcache.lock_map[key]);
}

void
bpin(struct buf *b) {
    int key = hash(b->blockno);

    acquire(&bcache.lock_map[key]);
    b->refcnt++;
    release(&bcache.lock_map[key]);
}

void
bunpin(struct buf *b) {
    int key = hash(b->blockno);
    acquire(&bcache.lock_map[key]);
    b->refcnt--;
    release(&bcache.lock_map[key]);
}


