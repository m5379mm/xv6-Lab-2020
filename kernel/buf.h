struct buf {
  int valid;   // 是否已从磁盘读取数据
  int disk;    // 该缓冲区是否与磁盘上的某个块相关联
  uint dev;    // 设备号
  uint blockno;//块号
  struct sleeplock lock;
  uint refcnt;
  struct buf *next;
  uchar data[BSIZE];
  uint lastUse;
};