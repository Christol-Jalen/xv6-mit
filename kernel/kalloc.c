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

struct run { // 一个链表节点，用于表示一个内存页
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
  freerange(end, (void*)PHYSTOP); // 调用freerange将内存添加到空闲列表中
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

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// 目的：kalloc 函数从内核的空闲内存页链表中分配一个内存页，并返回其地址
void *
kalloc(void)
{
  struct run *r; // 声明一个指向 struct run 的指针 r，用于临时存储从 freelist 中获取的空闲内存页。

  acquire(&kmem.lock); // 调用 acquire 函数获取 kmem.lock 自旋锁
  r = kmem.freelist;  // r 将指向一个空闲内存页
  if(r) // 检查 r 是否为非空指针（即 freelist 是否有可用内存页）
    kmem.freelist = r->next; // 如果 r 非空，将 freelist 更新为下一个空闲页节点
  release(&kmem.lock); // 调用 release 函数释放 kmem.lock 自旋锁。 目的：允许其他线程访问和修改 freelist
	                      
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
                                 // memset() 的主要作用是将指定内存区域的每个字节设置为相同的值
                                 // 在这里，将指针 r 指向的内存区域的前 PGSIZE（4096） 个字节，每个字节都设置为 0x05（即十进制的 5）
  return (void*)r; // 返回一个指向分配的内存页的指针，供调用者使用
}
