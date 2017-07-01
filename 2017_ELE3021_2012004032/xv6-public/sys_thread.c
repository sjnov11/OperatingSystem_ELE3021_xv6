#include "types.h"
#include "defs.h"


//int
//thread_create(thread_t * thread, void * (*start_routine)(void *), void *arg)
//{
//  uint oldsz, sz, sp, ustack[3+MAXARG+1];
//  struct proc *np;
//  int i;
//
//  // Allocate process for thread. kstack is also allocated here.
//  if((np = allocproc()) == 0){
//    return -1;
//  }
//
//  // Allocate process user stack.
//  //sp = proc->tf->esp;
//  sz = proc->sz;
//  oldsz = PGROUNDUP(sz);
//  if((sz = allocuvm(proc->pgdir, oldsz, oldsz + 2*PGSIZE)) == 0)
//    return -1;
//  clearpteu(proc->pgdir, (char*)(sz - 2*PGSIZE));
//  sp = sz;
//
//  // Push argument .
//  ustack[0] = 0xffffffff;
//  ustack[1] = arg;
//
//  sp -= 2 * 4;
//    
//  if(copyout(proc->pgdir, sp, ustack, 2*4) < 0){
//    deallocuvm(proc->pgdir, oldsz, sz);
//    return -1;
//  }
//
//
//  np->pgdir = proc->pgdir;
//  proc->sz = sz;
//  np->sz = sz;
//  np->parent = proc;
//  np->tf->eip = start_routine;
//  np->tf->esp = sp;
//  
//  for(i = 0; i < NOFILE; i++)
//    if(proc->ofile[i])
//      np->ofile[i] = filedup(proc->ofile[i]);
//  np->cwd = idup(proc->cwd);
//
//  safestrcpy(np->name, proc->name, sizeof(proc->name));
//
//  *thread = np->pid;
//
//  acquire(&ptable.lock);
//
//  np->state = RUNNABLE;
//
//  release(&ptable.lock);
//
//  return 0;
//
//}

//void
//thread_exit(void *retval)
//{
//  acquire(&ptable.lock);
//  proc->state = SLEEPING;
//  proc->tret = retval;
//  release(&ptable.lock);
//
//  sched();
//
//  return 0;
//
//}
//
//int 
//thread_join(thread_t thread, void **retval)
//{
//  struct proc *p;
//  acquire(&ptable.lock);
//  // Get thread which pid is equal to the given arg thread.
//  for(p = ptable.proc; p < ptable.proc[NPROC]; p++){
//    if(p->pid == (int)thread)
//      break;
//  }
//  if(p->state == SLEEPING){
//    retval = p->tret;
//    kfree(p->kstack);
//    p->pid = 0;
//    p->parent = 0;
//    p->name[0] = 0;
//    p->killed = 0;
//    p->state = UNUSED;
//    p->tick = 0;
//    p->priority = 0;
//    p->isstride = 0;   
//    return 0;
//  }
//  return -1;
//}
//

// Wrapper function
int
sys_thread_create(void)
{
  thread_t *thread;
  void *(*start_routine)(void*);
  void *arg;

  if(argint(0, (int *)&thread) < 0 || argint(1, (int *)&start_routine) < 0
          || argint(2, (int *)&arg) < 0)
    return -1;


  return thread_create(thread, start_routine, arg);
}


int
sys_thread_exit(void)
{
  void *retval;
  
  if(argint(0, (int *)&retval) < 0)
    return -1;
  thread_exit(retval);
  return 0;
}

int sys_thread_join(void)
{
  thread_t thread;
  void **retval;

  if(argint(0, (int *)&thread) < 0 || argint(1, (int *)&retval) < 0)
    return -1;
  return thread_join(thread, retval);
}

