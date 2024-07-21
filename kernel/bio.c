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
#define NBUCKET 13      // 哈希桶的个数
#define HASH(dev, blockno) (((dev % NBUCKET + (blockno % NBUCKET)) % NBUCKET)) // 哈希函数

struct {
  struct spinlock lock;
  struct buf buf[NBUF];// 缓冲区数组
  struct buf bufMap[NBUCKET];// 缓冲区对应的桶
  struct spinlock bufMapLock[NBUCKET];
} bcache;

void
binit(void)
{
  initlock(&bcache.lock, "bcache");

  //初始化
  for(int i=0;i<NBUCKET;i++){
    initlock(&bcache.bufMapLock[i],"bcache_bufMap");
    bcache.bufMap[i].next = 0;
  }
  for(int i=0;i<NBUF;i++){
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->lastUse=0;
    b->refcnt=0;
    int num = HASH(b->dev, b->blockno); // 根据 dev 和 blockno 计算哈希值
    b->next=bcache.bufMap[num].next;
    bcache.bufMap[num].next=b;
  }
}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int num = HASH(dev,blockno);
  acquire(&bcache.bufMapLock[num]);

  // 是否已缓存
  for(b = bcache.bufMap[num].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufMapLock[num]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bufMapLock[num]);
  acquire(&bcache.lock);
  //检查是否存在未使用的缓冲区
  struct buf *before=0;
  uint hold=-1;
  for(int i=0;i<NBUCKET;i++){
    acquire(&bcache.bufMapLock[i]);
    int find=0;
    // 找到最久未使用的
    for(b=&bcache.bufMap[i];b->next;b=b->next){
      if(b->next->refcnt==0&&(!before||b->next->lastUse<before->next->lastUse)){
        before=b;
        find=1;
      }
    }
    if(!find){// 未找到
      release(&bcache.bufMapLock[i]);
    }
    else{
      if(hold!=-1){
        release(&bcache.bufMapLock[hold]);
      }
      hold=i;
    }
  }
  if(!before){
    panic("bget:no buffer");
  }
  b=before->next;
  if(hold!=num){//新的桶
    before->next = b->next;//移除
    release(&bcache.bufMapLock[hold]);
    acquire(&bcache.bufMapLock[num]);
    b->next = bcache.bufMap[num].next;
    bcache.bufMap[num].next=b;
  }
  b->dev=dev;
  b->blockno=blockno;
  b->refcnt=1;
  b->valid=0;
  release(&bcache.bufMapLock[num]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
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

  int num = HASH(b->dev,b->blockno);
  acquire(&bcache.bufMapLock[num]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->lastUse=ticks;//最后使用时间设为滴答数
  }
  
  release(&bcache.bufMapLock[num]);
}

void
bpin(struct buf *b) {
  int num = HASH(b->dev,b->blockno);
  acquire(&bcache.bufMapLock[num]);
  b->refcnt++;
  release(&bcache.bufMapLock[num]);
}

void
bunpin(struct buf *b) {
  int num = HASH(b->dev,b->blockno);
  acquire(&bcache.bufMapLock[num]);
  b->refcnt--;
  release(&bcache.bufMapLock[num]);
}


