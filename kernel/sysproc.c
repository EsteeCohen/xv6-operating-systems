#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
extern struct proc proc[NPROC]; // to access the process table
extern struct cpu cpus[NCPU];   // ← חדש
extern void swtch(struct context*, struct context*);  // ← חדש
uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_memsize(void) //TASK 2: memsize system call
{
  return myproc()->sz;
}




uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_co_yield(void)
{
  // arguments from userspace: target pid and value to send
  int dest_pid;
  int my_value;
  argint(0, &dest_pid);
  argint(1, &my_value);

  struct proc *me = myproc();
  struct proc *peer = 0;
  uint64 received_value;

  // basic validation
  if (dest_pid <= 0 || dest_pid == me->pid) {
    return -1;
  }

  // locate target in proc table
  for (int i = 0; i < NPROC; i++) {
    if (proc[i].pid == dest_pid) {
      peer = &proc[i];
      break;
    }
  }

  if (!peer) {
    return -1;
  }

  // lock both — ours first to avoid deadlock with concurrent yields
  acquire(&me->lock);
  acquire(&peer->lock);

  // re-verify under lock: target may have been killed or recycled
  if (peer->state == UNUSED || peer->killed || peer->state == ZOMBIE) {
    release(&peer->lock);
    release(&me->lock);
    return -1;
  }

  // sleeping processes waiting in co_yield park on a unique channel:
  // chan = 1000000 + (pid they're waiting for). the offset keeps this
  // value distinct from any pointer used as a sleep channel elsewhere.
  if (peer->state == SLEEPING &&
      peer->chan == (void*)(uint64)(1000000 + me->pid)) {

    // rendezvous: peer is waiting for us — direct switch, no scheduler.

    // each side stashes its outgoing value in its own trapframe->a1.
    // pick up what peer left for us before we overwrite anything.
    received_value = peer->trapframe->a1;

    // deliver our value into peer's a0 (its syscall return register)
    peer->trapframe->a0 = my_value;

    // prepare ourselves to sleep
    me->state = SLEEPING;
    me->chan = (void*)(uint64)(1000000 + peer->pid);
    me->trapframe->a1 = my_value;

    // peer takes over the cpu — skip RUNNABLE entirely
    peer->state = RUNNING;
    mycpu()->proc = peer;

    // release our own lock before swtch. peer's lock stays held and is
    // released by peer's resume code (as its own me->lock). standard
    // xv6 hand-off pattern.
    release(&me->lock);

    // save intena across swtch — it's a per-thread property and swtch
    // only handles 14 cpu registers, like sched() does it
    int saved_intena = mycpu()->intena;
    swtch(&me->context, &peer->context);
    mycpu()->intena = saved_intena;

    // resumed here when someone yields back to us. our lock is held
    // by the cpu (the peer kept it through their swtch).
    me->chan = 0;
    release(&me->lock);
    return received_value;
  }

  // edge case: peer not ready yet. we sleep and wait to be yielded to.

  me->trapframe->a1 = my_value;
  me->chan = (void*)(uint64)(1000000 + dest_pid);
  me->state = SLEEPING;

  release(&peer->lock);

  // swtch to the scheduler. when someone yields to us, they'll set our
  // a0 and swtch us back here.
  int saved_intena = mycpu()->intena;
  swtch(&me->context, &mycpu()->context);
  mycpu()->intena = saved_intena;

  received_value = me->trapframe->a0;
  me->chan = 0;
  release(&me->lock);
  return received_value;
}