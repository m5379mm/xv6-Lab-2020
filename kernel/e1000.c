#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"
//每个环将包含 16 个描述符。描述符用于指示 E1000 网络卡在内存中放置或获取数据包的位置。

#define TX_RING_SIZE 16
//传输环
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
//mbuf 指针数组，与 tx_ring 对应。mbuf 结构用于存储网络数据包。
//tx_mbufs 数组中的每个条目指向一个 mbuf，用于存放在 tx_ring 描述符中的数据包
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
//接收环或接收队列,当卡或驱动程序到达数组的末尾时，它会回到开头。
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// 指向 E1000 第一个控制寄存器的指针
// 通过对 regs 指针进行操作，可以访问和修改 E1000 的控制寄存器，从而控制其操作，如启动传输或接收数据包，设置中断等。
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  uint32 tail;
  struct tx_desc *desc;

  acquire(&e1000_lock);
  tail = regs[E1000_TDT];
  desc = &tx_ring[tail];//取到末尾描述符，也就是拿到一个新的描述符
  if((desc->status&E1000_TXD_STAT_DD)==0){//尚未完成相应的先前传输请求
    return -1;
  }
  // 检查是否释放，及时释放以保证可用
  if(tx_mbufs[tail]){
    mbuffree(tx_mbufs[tail]);
  }
  desc->addr=(uint64)m->head;//指向的数据包的内容存储在 m->head 指向的内存区域中
  desc->length=m->len;//数据包的长度
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  tx_mbufs[tail] = m;
   __sync_synchronize();
  // 更新尾部指针
  regs[E1000_TDT] = (tail + 1) % TX_RING_SIZE;
  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  uint32 head;
  struct rx_desc *desc;

  head = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
  desc = &rx_ring[head];
  while (desc->status & E1000_RXD_STAT_DD) { // 完成相应的先前传输请求
    rx_mbufs[head]->len = desc->length;
    net_rx(rx_mbufs[head]);
    rx_mbufs[head] = mbufalloc(0);
    if(!rx_mbufs[head]){
      panic("new buf create fail");
    }
    desc->addr = (uint64) rx_mbufs[head]->head;
    desc->status=0;
    head=(head+1)%RX_RING_SIZE;
    desc=&rx_ring[head];
  }
  regs[E1000_RDT]=(head-1)%RX_RING_SIZE;
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
