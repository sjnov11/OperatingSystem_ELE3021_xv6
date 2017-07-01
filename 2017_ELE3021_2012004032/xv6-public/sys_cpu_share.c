#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "stride.h"

void
portion_to_stride()
{
  struct stride *s;
  int product_portion = 1;
  int counter = 0;
  // Multiply each enabled stride entry's portion.
  for(s = stable.str; s < &stable.str[NPROC]; s++){
    if(s->enabled == 1 && s->proc->isthread == 0){
      product_portion *= s->portion;
      counter ++;
    }
  }
#ifdef D_cpushare
  cprintf("[cpushare] %d of stride product portion : %d\n",counter, product_portion);
#endif
  // Set each enabled stride entry's stride.
  // Stride = product of all of process portion / process portion
  for(s = stable.str; s < &stable.str[NPROC]; s++){
    if(s->enabled == 1 && s->proc->isthread == 0){      
      s->stride = product_portion / s->portion;
#ifdef D_cpushare
      if(s->dummy)
        cprintf("[cpushare] stride(MLFQ) : %d\n", s->stride);
      else
        cprintf("[cpushare] stride(pid:%d) : %d\n", s->proc->pid, s->stride);
#endif
      if(isLWP(s->proc) && s->dummy != 1){               
        s->stride = s->stride * NumberOfThread(s->proc);
        set_all_LWP_stride(s->proc, s->stride * NumberOfThread(s->proc));
#ifdef D_cpushare
        cprintf("[cpushare] Mod stride(pid:%d) : %d\n",s->proc->pid,s->stride);
#endif
      }
#ifdef D_cpushare
      if(s->dummy)
        cprintf("[cpushare] MLFQ stride / Stride : %d\n", s->stride);
      else
       cprintf("[cpushare] pid : %d / Stride : %d\n",s->proc->pid, s->stride);
#endif
    }
  }
}

// Reset all pass to 0
void
reset_pass()
{
  struct stride *s;
//  int l_pass;
//  l_pass = stable[0].pass;
////  for(s = stable; s < &stable[NPROC]; s++){
//    if(s->enabled && l_pass > s->pass)
//      l_pass = s->pass;
//  }
//  
//  for(s = stable; s < &stable[NPROC]; s++){
//    if(s->enabled){
//      s->pass -= l_pass;
//    }
//  }

  for(s = stable.str; s < &stable.str[NPROC]; s++){
    if(s->enabled)
      s->pass = 0;
  }
}

int 
set_cpu_share(int portion)
{
  struct stride *s;

  if(portion <= 0){
    cprintf("Portion can't be negative!\n");
    return -1;
  }
  if(stable.str[0].portion - portion < 20){
    cprintf("MLFQ needs at least 20 percents of CPU!\n");
    return -1;
  }
#ifdef D_cpushare
  cprintf("[cpushare] start CPU share(%d) pid : %d\n", portion, proc->pid);
#endif
  acquire(&stable.lock);
  
  for(s = stable.str; s < &stable.str[NPROC]; s++){      
    // Table entry that not used
    if(s->enabled != 1){
      proc->isstride = 1;
      s->proc = proc;
      s->portion = portion;
      stable.str[0].portion -= portion;
      s->enabled = 1;
      portion_to_stride();
      // caller process is LWP.
      if(isLWP(proc) == 1){
#ifdef D_cpushare
        cprintf("[cpushare] call set_cpu_share_LWP\n");
#endif
        set_cpu_share_LWP();


        //main_t = GetMainThread(proc);

        //numberofthread = NumberOfThread(main_t);
        //new_stride = s->stride * numberofthread;
        // thread -> stride .
        //set_cpu_share_LWP(proc);
        //s->stride = new_stride;


      }
      reset_pass();
      // The current process running in MLFQ stops running.
      // And it will be scheduled by Stride.
      release(&stable.lock);
      yield();
      return 1;

    }
  }
  
  /*
  for(s = stable; s < &stable[NPROC]; s++){
    if(s->enable == 1)
      cprintf("stride : %d\n", s->stride);
  }
  */
  
  release(&stable.lock);
  cprintf("Not enough stable entry!\n");
  return 0;
}

//void
//set_thread_stride(struct proc *t)
//{
//  struct stride *s;
//  int stride = 0;
//  for(s = stable.str; s < &stable.str[NPROC]; s++){
//    if(s->dummy != 1 && s->proc == t->mthread_p){
//      stride = s->stride;
//    }
//  }
//  for(s = stable.str; s < &stable.str[NPROC]; s++){
//    if(s->dummy != 1 && s->proc == t)
//      s->stride = stride * NumberOfThread(t->mthread_p);
//  }
//}

void
set_all_LWP_stride(struct proc *main_t, int stride)
{
  struct stride *s;
  
  // Find main thread.
//  for(s = stable.str; s < &stable.str[NPROC]; s++) {
//    if(s->dummy != 1 && s->proc == main_t)
//      break;
//  }
//  numberofthread = NumberOfThread(main_t);
//  cprintf("main thread(%d) s stride : %d\n",s->proc->pid, s->stride);
//  // Calcuate main thread stride according to portion.
//  main_stride = s->stride / (numberofthread - 1);
//  cprintf("main stride : %d\n", main_stride);
//  // LWP's new stride.
//  new_stride = main_stride * numberofthread;
//
//  // Change main thread's stride to new.
//  s->stride = new_stride;
//  cprintf("pid(%d) new stride : %d\n", s->proc->pid, s->stride);

  // Find sub thread.
  for(s = stable.str; s < &stable.str[NPROC]; s++){
    if(s->dummy != 1 && s->proc->isthread == 1 
            && s->proc->mthread_p == main_t){
      s->stride = stride;
    }
  }
}

// pass all thread to reset stride
// current thread num
int
set_thread_cpu_share(struct proc *thread_p, int type)
{
  struct stride *s, *main_s;
  int numberofthread, stride;

  for(s = stable.str; s < &stable.str[NPROC]; s++){      
    // Table entry that not used
    if(s->enabled != 1){
      thread_p->isstride = 1;
      s->proc = thread_p;
      // Thread entry has no portion.
      s->portion = 0;
      s->enabled = 1;

      for(main_s = stable.str; main_s < &stable.str[NPROC]; main_s++){
        if(main_s->dummy == 1 || main_s->proc != proc)
          continue;

        // Find main thread.
        numberofthread = NumberOfThread(proc);
        if(type == 0){
          stride = main_s->stride / (numberofthread - 1) * (numberofthread);
        }
        else if(type == 1){
          return main_s->stride;
          //tride += main_s->stride * (numberofthread);
        }
#ifdef D_cpushare
        cprintf("stride(pid:%d) : %d\n",main_s->proc->pid,stride);
#endif
        main_s->stride = stride;
        break;
      }
      
      set_all_LWP_stride(proc, stride);
      reset_pass();
      return 1;
    }
  }
  cprintf("Not enough stable entry!\n");
  return 0;

}
int
sys_set_cpu_share(void)
{
  int portion;
  if (argint(0, &portion) < 0)
    return -1;
  return set_cpu_share(portion);
}

