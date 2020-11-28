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

//original one
// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // head.next is most recently used.
//   struct buf head;
// } bcache;

//my implementation
struct {
  uint unused[NBUF];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf hashbuckets[NBUCKET];
  struct spinlock hash_locks[NBUCKET];
} bcache;

// //original one
// void
// binit(void)
// {
//   struct buf *b;

//   initlock(&bcache.lock, "bcache");

//   // Create linked list of buffers
//   bcache.head.prev = &bcache.head;
//   bcache.head.next = &bcache.head;
//   for(b = bcache.buf; b < bcache.buf+NBUF; b++){
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     initsleeplock(&b->lock, "buffer");
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
// }

//my implementation
void
binit(void)
{
    struct buf* b;

    for (int i = 0; i < NBUCKET; i++){
        bcache.hashbuckets[i].prev = &bcache.hashbuckets[i];
        bcache.hashbuckets[i].next = &bcache.hashbuckets[i];
        initlock(&bcache.hash_locks[i], "bcache");
    }

    for (b = bcache.buf; b < bcache.buf+NBUF; b++){
        //get hash value
        uint hash_id = ((((uint64)b->dev) << 32) | b->blockno) % NBUCKET;
        b->next = bcache.hashbuckets[hash_id].next;
        b->prev = &bcache.hashbuckets[hash_id];
        initsleeplock(&b->lock, "buffer");
        bcache.hashbuckets[hash_id].next->prev = b;
        bcache.hashbuckets[hash_id].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.

//original one
// static struct buf*
// bget(uint dev, uint blockno)
// {
//   struct buf *b;

//   acquire(&bcache.lock);

//   // Is the block already cached?
//   for(b = bcache.head.next; b != &bcache.head; b = b->next){
//     if(b->dev == dev && b->blockno == blockno){
//       b->refcnt++;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }

//   // Not cached; recycle an unused buffer.
//   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//     if(b->refcnt == 0) {
//       b->dev = dev;
//       b->blockno = blockno;
//       b->valid = 0;
//       b->refcnt = 1;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }
//   panic("bget: no buffers");
// }


//my implementation
static struct buf*
bget(uint dev, uint blockno)
{
    struct buf *b;
    uint hash_id = (blockno) % NBUCKET;
    acquire(&bcache.hash_locks[hash_id]);

    // Is the block already cached?
    for(b = bcache.hashbuckets[hash_id].next; b != &bcache.hashbuckets[hash_id]; b = b->next){
        if(b->dev == dev && b->blockno == blockno){
            b->refcnt++;
            release(&bcache.hash_locks[hash_id]);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // Not cached; recycle an unused buffer.
    for (int i = 0; i < NBUCKET; i++){
        if(i != hash_id){   //look for buffer in other hash lists
            for (b = bcache.hashbuckets[i].prev; b != &bcache.hashbuckets[i]; b = b->prev) {
                //if unused
                if (b->refcnt == 0){
                    acquire(&bcache.hash_locks[i]);
                    b->dev = dev;
                    b->blockno = blockno;
                    b->valid = 0;
                    b->refcnt = 1;

                    //dropped from hashbuckets[i]
                    b->next->prev = b->prev;
                    b->prev->next = b->next;
                    //added to hashbuckets[hash_id]
                    b->next = bcache.hashbuckets[hash_id].next;
                    b->prev = &bcache.hashbuckets[hash_id];

                    bcache.hashbuckets[hash_id].next->prev = b;
                    bcache.hashbuckets[hash_id].next = b;

                    release(&bcache.hash_locks[i]);
                    release(&bcache.hash_locks[hash_id]);
                    acquiresleep(&b->lock);
                    return b;
                }
            } 
        }
    }
    panic("no free buffer");
}



// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
//original one
// void
// brelse(struct buf *b)
// {
//   if(!holdingsleep(&b->lock))
//     panic("brelse");

//   releasesleep(&b->lock);

//   acquire(&bcache.lock);
//   b->refcnt--;
//   if (b->refcnt == 0) {
//     // no one is waiting for it.
//     b->next->prev = b->prev;
//     b->prev->next = b->next;
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
  
//   release(&bcache.lock);
// }

//my implementation
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);

  uint hash_id = ((((uint64)b->dev) << 32) | b->blockno) % NBUCKET;

  acquire(&bcache.hash_locks[hash_id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbuckets[hash_id].next;
    b->prev = &bcache.hashbuckets[hash_id];
    bcache.hashbuckets[hash_id].next->prev = b;
    bcache.hashbuckets[hash_id].next = b;
  }
  
  release(&bcache.hash_locks[hash_id]);
}

//orginal one
// void
// bpin(struct buf *b) {
//   acquire(&bcache.lock);
//   b->refcnt++;
//   release(&bcache.lock);
// }

// void
// bunpin(struct buf *b) {
//   acquire(&bcache.lock);
//   b->refcnt--;
//   release(&bcache.lock);
// }

//my implementation
void
bpin(struct buf *b) {
  uint hash_id = ((((uint64)b->dev) << 32) | b->blockno) % NBUCKET;

  acquire(&bcache.hash_locks[hash_id]);
  b->refcnt++;
  release(&bcache.hash_locks[hash_id]);
}

void
bunpin(struct buf *b) {
  uint hash_id = ((((uint64)b->dev) << 32) | b->blockno) % NBUCKET;

  acquire(&bcache.hash_locks[hash_id]);
  b->refcnt--;
  release(&bcache.hash_locks[hash_id]);
}
