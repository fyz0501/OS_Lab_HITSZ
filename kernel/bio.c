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

#define NBUCKETS 13
struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];
  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  //struct buf head;
  struct buf hashbucket[NBUCKETS]; //每个哈希队列一个linked list及一个lock
} bcache;

void
binit(void)
{
  struct buf *b;
  char lock_name[8] = {'b','c','a','c','h','e','0','\0'};
  for(int i = 0;i < NBUCKETS;i++){
    lock_name[8] += i;
    initlock(&bcache.lock[i], lock_name);
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  // Create linked list of buffers
  // 循环地给各个内存桶分配缓存块
  // 桶内部缓存为一个双向链表结构
  int i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hashbucket[i].next;
    b->prev = &bcache.hashbucket[i];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[i].next->prev = b;
    bcache.hashbucket[i].next = b;
    i = (i+1)%NBUCKETS;
  }
}

// 哈希函数
uint
bhash(uint blockno){
  uint hash_res = blockno % NBUCKETS;
  return hash_res;
}

//从桶i中找到空闲的缓存,查找的方向是逆向查找
static struct buf*
bfind(uint i){
  for(struct buf *b = bcache.hashbucket[i].prev;b!=&bcache.hashbucket[i]; b = b->prev){
    if(b->refcnt == 0){
      b->prev->next = b->next;
      b->next->prev = b->prev;
      b->prev = b->next = 0;
      return b;
    }
  }
  return 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint i = bhash(blockno);
  acquire(&bcache.lock[i]);

  // Is the block already cached?
  // 顺序查找是否命中
  for(b = bcache.hashbucket[i].next; b != &bcache.hashbucket[i]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[i]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  b = bfind(i);
  if(b == 0){
    // 每一个哈希桶都从后一个哈希桶开始寻找空闲的缓存,以防死锁
    for(int j = (i+1)%NBUCKETS; j!=i;j = (j+1)%NBUCKETS){
      acquire(&bcache.lock[j]);
      b = bfind(j);
      if(b!=0){
        release(&bcache.lock[j]);
        break;
      }
      release(&bcache.lock[j]);
    }
  }
  // 找到了空闲的和缓存，改变指针进行获取
  if(b!=0){
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    // 连接到原哈希桶中
    b->next = bcache.hashbucket[i].next;
    b->prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next->prev = b;
    bcache.hashbucket[i].next = b;
    release(&bcache.lock[i]);
    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
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
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int i = bhash(b->blockno);
  acquire(&bcache.lock[i]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[i].next;
    b->prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next->prev = b;
    bcache.hashbucket[i].next = b;
  }
  
  release(&bcache.lock[i]);
}

void
bpin(struct buf *b) {
  int i = bhash(b->blockno);
  acquire(&bcache.lock[i]);
  b->refcnt++;
  release(&bcache.lock[i]);
}

void
bunpin(struct buf *b) {
  int i = bhash(b->blockno);
  acquire(&bcache.lock[i]);
  b->refcnt--;
  release(&bcache.lock[i]);
}


