#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "stride.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;


static struct proc *initproc;

// Time quantum for each level of MLFQ.
int time_quantum[3] = {5, 10, 20};

int nextpid = 1;

// Total spend time on MLFQ
int totaltick = 1;

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);
struct proc* GetMainThread(struct proc *p);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->tick = 0;
  p->priority = 0;
  p->isstride = 0;
  p->isthread = 0;  

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  struct proc *main_t;
  uint theap;

  //sz = proc->sz;
  main_t = GetMainThread(proc);
  theap = main_t->theap;
  if(n > 0){
    if((theap = allocuvm(main_t->pgdir, theap, theap + n)) == 0)
      return -1;
  } else if(n < 0){
    if((theap = deallocuvm(main_t->pgdir, theap, theap + n)) == 0)
      return -1;
  }
  main_t->theap = theap;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->theap, proc->bstack)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->theap = proc->theap;
  np->bstack = proc->bstack;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}


// Return main thread of LWP.
struct proc *
GetMainThread(struct proc *p){
  if(p->isthread != 1){      
    return p;
  }
  return GetMainThread(p->parent);
}

// Return number of thread including main thread.
int
NumberOfThread(struct proc *main_t)
{
  struct proc *p;
  int num = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->isthread && p->mthread_p == main_t)
      num++;
  }
  return num + 1;
}


void
set_cpu_share_LWP(void)
{
  struct proc *p;
  int stride;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->isthread == 1 && p->mthread_p == proc){
      stride = set_thread_cpu_share(p, 1);
      if(stride == 0){
        panic("thread cpu share(2) panic!");
        return;
      }
      
      
      //p->stride = main_t->stride * NumberOfThread(main_t);
      //set_all_thread_stride(p->mthread_p, NumberOfThread(p->mthread_p));
    }
  }
  stride = stride * NumberOfThread(proc);
  set_all_LWP_stride(proc, stride);
 // cprintf("set up stride : %d\n",stride);
//  for(s = stable.str; s < &stable.str[NPROC]; s++){
//    if(s->dummy != 1 && s->proc->mthread_p == main_t)
//      cprintf("Thread pid : %d stride : %d\n", s->proc->pid, s->stride);
//  }
}

// Return 1 if current process is LWP.
int
isLWP(struct proc *t)
{
  struct proc *p;
  
  if(proc->mthread_p != 0){
    return 1;
  }
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->mthread_p == t)
      return 1;
  }
  return 0;

}

// arg = 0 exit all, arg = 1 let current proc.
void
ExitLWP(int arg)
{
  struct proc *p, *child_p, *main_t;
  int fd;
#ifdef D_ExitLWP
  cprintf("[ExitLWP] Start...\n");
#endif
  main_t = GetMainThread(proc);
#ifdef D_ExitLWP
  cprintf("[ExitLWP] main_t : %d\n", main_t->pid);
#endif
  // To prevent interrupt when cleaning up other PCB
  //pushcli();
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->mthread_p == main_t || p == main_t){
      if(arg == 1 && p == proc) {
#ifdef D_ExitLWP
        cprintf("[ExitLWP] Pass current process..\n");
#endif
        continue;
      }
      // Because when exec call, the memory should remain.
      // When the exec done, call exitLWP again, then clean mem.
      if(arg == 1 && p == main_t){
#ifdef D_ExitLWP
        cprintf("[ExitLWP] Pass main thread process..\n");
#endif
        continue;
      }
#ifdef D_ExitLWP
      cprintf("[ExitLWP] Find one pid :%d\n", p->pid);
#endif
      // Close all open files.
      for(fd = 0; fd < NOFILE; fd++){
        if(p->ofile[fd]){
          fileclose(p->ofile[fd]);
          p->ofile[fd] = 0;
        }
      }

      begin_op();
      iput(p->cwd);
      end_op();
      p->cwd = 0;

      acquire(&ptable.lock);

      wakeup1(p->parent);

      // Pass abandoned child process to init,
      // Pass abandoned thread to main_t
      for(child_p = ptable.proc; child_p < &ptable.proc[NPROC]; child_p++){
        if(child_p->parent == proc){
          if(child_p->isthread != 1){
            child_p->parent = initproc;
            if(child_p->state == ZOMBIE)
              wakeup1(initproc);
          }
          else if(child_p->isthread == 1){
            child_p->parent = main_t;
            if(child_p->state == ZOMBIE)
              wakeup1(main_t);
          }
        }
      }
#ifdef D_ExitLWP
      cprintf("[ExitLWP] pid : %d Change State ZOMBIE\n", p->pid);
#endif
      p->state = ZOMBIE;
      release(&ptable.lock);
#ifdef D_ExitLWP
      //cprintf("[ExitLWP] release ptable lock success\n");
#endif
    }
  }
  //popcli();
  if(arg == 1){
    return;
  }
#ifdef D_ExitLWP
  cprintf("[ExitLWP] f1\n");
  if(proc->state == ZOMBIE){
    cprintf("[ExitLWP] current state : ZOMBIE\n");
  }
#endif
  acquire(&ptable.lock);
  sched();
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;
#ifdef D_exit
  cprintf("[exit] process(pid : %d) call exit\n",proc->pid);
#endif
  if(proc == initproc)
    panic("init exiting");

  // LWP exit. never return.
  if(isLWP(proc) == 1){
    ExitLWP(0);
  }

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){ 
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
#ifdef D_exit
  cprintf("[exit] Done Exit! pid :%d\n", proc->pid);
#endif
  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p, *t;
  int havekids, pid;
#ifdef D_wait
  cprintf("[Wait] pid : %d\n", proc->pid);
#endif
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
#ifdef D_wait
        cprintf("[wait] pid:%d find child(pid:%d) zombie to clear\n",proc->pid,p->pid);
#endif
        // Clear thread.
        for(t = ptable.proc; t < &ptable.proc[NPROC]; t++){
          if(t->mthread_p == p && t->state == ZOMBIE){
#ifdef D_wait
            cprintf("[wait] pid:%d find thread(pid:%d) zombie to clear\n",p->pid,t->pid);
#endif
            kfree(t->kstack);
            t->kstack = 0;
            t->pid = 0;
            t->parent = 0;
            t->name[0] = 0;
            t->killed = 0;
            t->state = UNUSED;
            t->tick = 0;
            t->priority = 0;
            t->isstride = 0;
            t->tret = 0;
            t->isthread = 0;
            t->mthread_p = 0;
            t->theap = 0;
            t->bstack = KERNBASE - PGSIZE;
          }
        }
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        p->tick = 0;
        p->priority = 0;
        p->isstride = 0;
        p->tret = 0;
        p->isthread = 0;
        p->mthread_p = 0;
        p->theap = 0;
        p->bstack = KERNBASE - PGSIZE;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}


void
MLFQ_scheduler(void)
{
  struct proc *temp_p, *p;
  int top_priority;
  int pflag;

  // Get top priority
  top_priority = MLFQLEV - 1; 
  for(temp_p = ptable.proc; temp_p < &ptable.proc[NPROC]; temp_p++){
    if(temp_p->state != RUNNABLE || temp_p->isstride == 1)
      continue;
    if(top_priority > temp_p->priority)
      top_priority = temp_p->priority;
  }

  // Find next process by MLFQ policy.
  // Search ptable from next ptable enrty which was scheduled just before.
  temp_p = stable.str[STRIDEMLFQ].proc;     // Get scheduled proc entry
  temp_p++;                             // Searching from next.
  pflag = 0;

  // Search from next ptable entry to end.
  for(p = temp_p; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE || p->priority != top_priority 
       || p->isstride != 0) {
      continue;
    }
    pflag = 1;
#ifdef DEBUGSEACHING
    cprintf("[P to End] pid : %d, priority : %d, tick : %d \n"
            ,pflag, p->pid, p->priority, p->tick);
#endif
    break;
  }

  // Search from start to ptable entry scheduled just before.
  if(pflag != 1){
    for(p = ptable.proc; p < temp_p; p++){
      if(p->state != RUNNABLE || p->priority != top_priority 
         || p->isstride != 0) {
        continue;
      }
      pflag = 1;
#ifdef DEBUGSEARCHING
      cprintf("[Start to P] pid : %d, priority : %d, tick : %d \n"
              ,pflag, p->pid, p->priority, p->tick);
#endif
      break;
    }
  }

  if(pflag == 1){
#ifdef DEBUG
    cprintf("[MLFQ] pid : %d, stride : %d, lev: %d, tt : %d\n",
            p->pid, stable.str[0].stride, p->priority, totaltick);
#endif

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
   
    stable.str[STRIDEMLFQ].proc = p;
    proc = p;
    switchuvm(p);
    p->state = RUNNING; 
    swtch(&cpu->scheduler, p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    proc = 0;

    // Add totaltick and process spend tick.
    // Whether process released CPU or not, the scheduler counts its and
    // totaltick. This will prevent so called "Gaming of Scheduler"
    if(p->isstride == 0){
      totaltick++;
      p->tick++;
    }
    // Check whether the process spend all time quantum. If process spend
    // all time quantum, Reset process' tick value
    // and lower down its priority.        
    if(p->tick >= time_quantum[p->priority]){
      if(p->priority < 2) p->priority++;
      p->tick = 0;
    }
  
    // Priority boost.
    if(totaltick > 100) {
      for(temp_p = ptable.proc; temp_p<&ptable.proc[NPROC]; temp_p++){
        temp_p->priority = 0;
        temp_p->tick = 0;
      }
      totaltick = 0;
      //cprintf("[Priority Boost]\n");
    }
  }
}

void
Stride_scheduler(void)
{
  struct stride *temp_s, *s;

  // Initialize Stride table.
  // MLFQ is allocated in stable[STRIDEMLFQ].
  stable.str[STRIDEMLFQ].proc = ptable.proc;
  stable.str[STRIDEMLFQ].pass = 0;
  stable.str[STRIDEMLFQ].stride = 0;
  stable.str[STRIDEMLFQ].portion = 100;
  stable.str[STRIDEMLFQ].dummy = 1;
  stable.str[STRIDEMLFQ].enabled = 1;


  // initialize Stride table
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Acquire Lock
    acquire(&ptable.lock);
    s = stable.str;
   
    // The process that has smallest pass will be next one.
    for(temp_s = stable.str; temp_s < &stable.str[NPROC]; temp_s++){
      if(temp_s->enabled == 1 && temp_s->proc->state == RUNNABLE){
        if(s->pass > temp_s->pass){
          s = temp_s;
        }
      }
    }

    // Switch to process selected by Stride Scheduler.
    // If dummy is 1, Call MLFQ. Else, Start process in stable.
    if(s->dummy == 1){
      s->pass += s->stride;
      MLFQ_scheduler();
    }
    else{
#ifdef DEBUG
      cprintf("[Stride] pid : %d, stride: %d\n",s->proc->pid,s->stride); 
#endif
      proc = s->proc;
      switchuvm(s->proc);
      s->proc->state = RUNNING;
      s->pass += s->stride;
      swtch(&cpu->scheduler, s->proc->context);
      
      switchkvm();
      proc = 0;
      // If process end, set stable entry disabled 
      // And add process portion to MLFQ portion 
      if(s->proc->state == UNUSED || s->proc->state == ZOMBIE){
        s->enabled = 0;
        stable.str[0].portion += s->portion;
      }
      //if(s->proc->state == SLEEP)
    }
    // Release lock
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

int 
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
  uint sp, ustack[3+MAXARG+1], bstack;
  struct proc *np;
  int i;
#ifdef TDEBUG
  cprintf("[CREATE]\n");
#endif

  // Allocate process for thread. kstack is also allocated at here.
  if((np = allocproc()) == 0){
#ifdef TDEBUG
    cprintf("allocproc fail\n");
#endif
    return -1;
  }
 
  // Allocate thread(process) user stack.
  bstack = proc->bstack;
  bstack = PGROUNDDOWN(bstack);
  bstack -= 2*PGSIZE;
  if((sp = allocuvm(proc->pgdir, bstack, bstack + 2*PGSIZE)) == 0){
#ifdef TDEBUG
    cprintf("allocuvm failure. sp : %d\n", sp);
#endif
    return -1;
  }
  clearpteu(proc->pgdir, (char*)(sp - 2*PGSIZE));
  //sp = sz;

  // Push argument .
  ustack[0] = 0xffffffff;
  ustack[1] = (uint)arg;

  sp -= 2 * 4;
    
  if(copyout(proc->pgdir, sp, ustack, 2*4) < 0){
#ifdef TDEBUG
    cprintf("copy ustack failure\n");
#endif
    deallocuvm(proc->pgdir, bstack + 2*PGSIZE, bstack);
    return -1;
  }

  np->pgdir = proc->pgdir;
  //proc->sz = sz;
  np->sz = proc->sz;
  np->theap = proc->theap;
  np->bstack = bstack;
  proc->bstack = bstack;
  np->parent = proc;
  
  *np->tf = *proc->tf;
  np->isthread = 1;
  np->mthread_p = GetMainThread(proc);
  //Stride setting
  acquire(&stable.lock);
  if(proc->isstride == 1){
    if(set_thread_cpu_share(np, 0) == 0) {
      panic("thread cpu share panic!");
      return -1;
    }
    //set_all_thread_stride(np->mthread_p, NumberOfThread(np->mthread_p));
  }
  release(&stable.lock);
  
  np->tf->eip = (uint)start_routine;

  np->tf->esp = sp;
 
  // Copy parent's file descriptor
  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  *thread = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

#ifdef TDEBUG
  cprintf("[CREATE] Done! pid : %d parent : %d\n", np->pid, np->parent->pid);
#endif
  return 0;
}

void
thread_exit(void *retval)
{
  struct proc *p;
  int fd;
#ifdef TDEBUG
  //cprintf("[EXIT]\n");
#endif
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait();
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    // Find child process
    if(p->parent == proc){
      if(p->isthread != 1){
        p->parent = initproc;        
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }

      else if(p->isthread == 1){
        p->parent = p->mthread_p;
        if(p->state == ZOMBIE)
          wakeup1(p->mthread_p);
      }
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  proc->tret = retval;
  //proc->parent->hasthread -= 1;
#ifdef TDEBUG
  cprintf("[EXIT] Done! pid : %d parent : %d\n", proc->pid, proc->parent->pid);
#endif
  sched();
  panic("zombie exit thread");
}

int 
thread_join(thread_t thread, void **retval)
{
  struct proc *p, *p1;
  int havethread;
  uint bstack;
#ifdef TDEBUG
  cprintf("[JOIN] join thread pid : %d\n", (int)thread);
#endif
  acquire(&ptable.lock);
  for(;;){
    havethread = 0;   
    // Scan through table looking for exited thread.
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc || p->pid != (int)thread)
        continue;
      havethread = 1;

      if(p->state == ZOMBIE){
        // Found one.
        *retval = p->tret;

        // Reclaim allocated thread stack from sz to sz of thread 
        // which exited already (state is ZOMBIE). 
        // If exited thread isn't top of thread stack, 
        // then just end with ret value.

        // Main base stack.
        bstack = proc->bstack;
        // Only stack erea. Ignore heap called by sbrk.
        // bstack = PGROUNDDOWN(tsz); 
        // If exited thread is top of thread stack
        if(p->bstack == bstack){
#ifdef TDEBUG
          cprintf("[JOIN]Free pid : %d, parent : %d, mt : %d\n",p->pid,p->parent->pid,p->mthread_p->pid);
#endif
          kfree(p->kstack);
          p->kstack = 0;
          deallocuvm(p->pgdir, p->bstack + 2*PGSIZE, p->bstack);
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          p->state = UNUSED;
          p->tick = 0;
          p->priority = 0;
          p->isstride = 0;   
          p->tret = 0;
          p->isthread = 0;
          p->mthread_p = 0;
          p->theap = 0;
          p->bstack = KERNBASE - PGSIZE;
       
          for(;;){
            // Find next base stack.
            bstack += 2*PGSIZE;
            // tsz = PGROUNDUP(tsz);
            if(bstack >= KERNBASE - PGSIZE){
              break;
            }
            
            // Find thread with bstack
            for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++){
              if(p1->bstack == bstack && p1->parent == proc 
                      && p1->isthread == 1)
                break;
            }
  
            if(p1->state != ZOMBIE)
              break;
#ifdef TDEBUG
            cprintf("[JOIN]Free2(%d) pid : %d, parent : %d, mt : %d\n",p->pid,p1->pid,p1->parent->pid,p1->mthread_p->pid);
#endif
            kfree(p1->kstack);
            p1->kstack = 0;
            deallocuvm(p1->pgdir, p1->bstack + 2*PGSIZE, p1->bstack);
            p1->pid = 0;
            p1->parent = 0;
            p1->name[0] = 0;
            p1->killed = 0;
            p1->state = UNUSED;
            p1->tick = 0;
            p1->priority = 0;
            p1->isstride = 0;   
            p1->tret = 0;
            p1->isthread = 0;
            p1->mthread_p = 0;
            p1->theap = 0;
            p1->bstack = KERNBASE - PGSIZE;

          }
          p->pid = 0;
          proc->bstack = bstack;
        }
        //if(isLWP
        release(&ptable.lock);
        return 0;     
      }
    }

    if(!havethread || proc->killed){
      release(&ptable.lock);
      return -1;
    }
    sleep(proc, &ptable.lock);    

  }
  return -1;
}
//         pid = p->pid;
//         *retval = p->tret;
//         kfree(p->kstack);
//         p->kstack = 0;
//         deallocuvm(p->pgdir, p->sz, p->sz - 2*PGSIZE);
//         p->pid = 0;
//         p->parent = 0;
//         p->name[0] = 0;
//         p->killed = 0;
//         p->state = UNUSED;
//         p->tick = 0;
//         p->priority = 0;
//         p->isstride = 0;   



//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
