#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock; // 定义一个名为 tickslock 的自旋锁，用于保护对全局时钟计数器 ticks 的并发访问。
uint ticks; // 全局变量 ticks，用于记录自系统启动以来的时钟“滴答”次数，每次时钟中断时递增。

extern char trampoline[], uservec[], userret[]; // 外部引用，指向在汇编代码中定义的符号，用于在用户态和内核态之间切换时保存和恢复上下文。

// in kernelvec.S, calls kerneltrap().
void kernelvec();  // 声明一个外部函数 kernelvec，在 kernelvec.S 中定义，用于处理内核态下的中断和异常。

extern int devintr(); // 声明一个外部函数 devintr，用于处理设备中断。

void
trapinit(void) // 初始化自旋锁 tickslock，用于同步对 ticks 变量的访问。
{
  initlock(&tickslock, "time"); 
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void) // 为每个硬件线程（hart）设置中断向量表
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
// 处理来自用户态的中断、异常和系统调用。
void
usertrap(void) 
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0) // 使用 r_sstatus() 读取 sstatus 寄存器，确保中断来自用户态（即 SSTATUS_SPP 位未设置）
    panic("usertrap: not from user mode"); // 如果中断不是来自用户态，调用 panic("usertrap: not from user mode"); 触发内核恐慌

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec); // 将中断向量表指向内核态的 kernelvec，以处理在处理中断时可能发生的其他中断。

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc(); // 获取当前进程 p，并将用户程序计数器 sepc 保存到 p->trapframe->epc。
  
  if(r_scause() == 8){ // 如果 r_scause() == 8，表示这是一个系统调用。
    // system call

    if(p->killed) // 检查进程是否被标记为已终止，如果是，调用 exit(-1); 退出进程。
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4; // 将 p->trapframe->epc 增加 4，跳过 ecall 指令。

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on(); // 调用 intr_on(); 开启中断，然后调用 syscall(); 处理系统调用。

    syscall();
  } else if((which_dev = devintr()) != 0){ // 如果不是系统调用，调用 devintr();
    //---------------LAB 4--------------
    if(which_dev == 2){
      p->spend = p->spend + 1;
      if(p->spend == p->interval){
        p->spend = 0;
        p->trapframe->epc = (uint64)p->handler;
      }
    }
    //----------------------------------
  } 
  
  else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid); // 如果既不是系统调用，也不是已知的设备中断，打印错误信息，并将进程标记为已终止。
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed) // 如果进程被标记为已终止，调用 exit(-1); 退出进程。
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2) // 如果中断来自时钟（即 which_dev == 2），调用 yield(); 让出 CPU，调度其他进程运行。
    yield();

  usertrapret(); // 调用 usertrapret(); 函数，准备从内核态返回到用户态。
}

//
// return to user space
// 功能：从内核态返回到用户态，恢复用户进程的上下文。
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off(); // 关闭中断

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  // 准备 trapframe：
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  // 配置 sstatus 寄存器：
  unsigned long x = r_sstatus(); // 读取 sstatus 寄存器
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode // 清除 SSTATUS_SPP 位（设置为用户模式）
  x |= SSTATUS_SPIE; // enable interrupts in user mode // 设置 SSTATUS_SPIE 位（在用户模式下开启中断）
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc); // 调用 w_sepc(p->trapframe->epc); 将 sepc 寄存器设置为用户态程序的下一条指令地址

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable); // 计算用户页表的 satp 值，准备切换到用户页表。

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline); // 计算 userret 函数在跳板页（trampoline）的地址
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);  // 调用该地址的函数，传递 TRAPFRAME 和 satp，在汇编代码中完成从内核态到用户态的切换。
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
// 功能：处理发生在内核态的中断和异常。
void 
kerneltrap()
{
  int which_dev = 0;
  // 保存 sepc、sstatus 和 scause 寄存器的值：
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0) // 确保中断来自内核态（SSTATUS_SPP 位应设置）。
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0) // 确保在处理中断时中断被禁用（intr_get() 应返回 0）。
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){ // 调用 devintr(); 检查并处理设备中断。
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap"); // 如果中断未被识别，打印错误信息，并调用 panic("kerneltrap"); 触发内核恐慌。
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield(); // 如果中断来自时钟，并且当前有进程在运行，调用 yield(); 让出 CPU。

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  // 在返回之前，重新设置 sepc 和 sstatus 寄存器，以确保正确返回中断处理后的执行位置。
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// 功能：处理时钟中断。
void
clockintr() // 获取 tickslock 锁，增加 ticks 计数器，释放锁。
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks); // 调用 wakeup(&ticks); 唤醒等待 ticks 变量的进程。
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
// 功能：检查并处理设备中断。
int
devintr()
{
  uint64 scause = r_scause();
  // 检查外部中断：
  if((scause & 0x8000000000000000L) && // 检查 scause，如果最高位（0x8000000000000000L）被设置，
     (scause & 0xff) == 9){            // 并且低 8 位等于 9，表示这是一个来自 PLIC（Platform-Level Interrupt Controller）的外部中断。
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim(); // 调用 plic_claim(); 获取中断请求编号 irq。
    // 根据 irq 值，调用相应的中断处理函数：
    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1; // 返回 1：表示处理了其他设备中断。
  } else if(scause == 0x8000000000000001L){ // 如果 scause 等于 0x8000000000000001L，表示这是一个软件中断，通常由机器模式的计时器中断触发。
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2; // 返回 2：表示处理了时钟中断。
  } else {
    return 0; // 返回 0：表示未识别的中断。
  }
}

