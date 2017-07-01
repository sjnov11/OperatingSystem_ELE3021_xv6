# Project 2 - Scheduling

-------

## 1. Current xv6 Scheduler
This part is just about how the xv6 scheduler is working. You can skip this section.


There is process table structure that has lock and proc list members. 
**lock** is to avoid race conditions when multiple processors are running scheduling code.
The **ptable** initialize at main. **pinit()** initialize lock and **allocproc()** which is called by **userinit()** initialize proc.
```c
struct {
    struct spinlock lock;
    struct pro proc[NPROC];
}ptable;
```
When timer interrupt has occured, below codes will be executed.
When there is timer interrupt, current process yield processor to next scheduled process.
```c
if(proc && proc->state == RUNNING && tf->trapno = T_IRQ0+IRQ_TIMER)
    yield();
```

```c
void
yield(void)
{
    acquire(&ptable.lock);
    proc->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}
```
The function **sched()** do context switch from current process' context(old) to cpu->scheduler context(new).

```c
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
  swtch(&proc->context, cpu->scheduler);    // context switch from current to schedular
  cpu->intena = intena;
}
```
The function **scheduler()** is actually scheduling the process.
It checks process state is runnable, then switch from cpu->scheduler context(old) to that process(new).
```c
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
```

In Summary, whenever timer interrupt ocurrs, current process switch context from itself to CPU->scheduler context, and then scheduler find next process by scheduling policy (which is Round-Robin in xv6). Finally, CPU->scheduler context switch to next scheduled process.


## 2. MLFQ 
-----
### 0) Flow Chart
![mlfq_flow](/uploads/2a5a51ee610b06a334143d6f0dc1e755/mlfq_flow.JPG)
### 1) Data Structure
Add member properties to the ***proc*** structure to determine the number of ticks to process spending and the priority of the process.

```c
struct proc {
  (...)
  // added property 
  int ticks;                   // Times of ticks the process has spent on current priority.
  int priority;                // Priority of process
};
```

### 2) Abstraction
- MLFQ consists of three queueswith each queue applying the round robin scheduling.
- The scheduler chooses a next ready process from MLFQ. If any process is found
  in the higher priority queue, a process in the lower queue cannot be selected
  until the upper level queue becomes empty.
- Each queue has different time quantum.
  – The highest priority queue: 5 ticks
  – Middle priority queue: 10 ticks
  – The lowest priority queue: 20 ticks
- To prevent starvation, priority boosting needs to be performed periodically.
  – The priority boosting is the only way to move the process upward.
  – Frequency of the priority boosting: 100 ticks

### 3) Implementation
- Get current highest priority (***top_priority***) from all process which are in ***ptable***.

    ```c
        // Get top priority
        top_priority = MLFQLEV - 1; 
        for(temp_p = ptable.proc; temp_p < &ptable.proc[NPROC]; temp_p++){
            if(temp_p->state != RUNNABLE)
              continue;
            if(top_priority > temp_p->priority)
            top_priority = temp_p->priority;
        }
    ```

- Find next process by MLFQ policy.  
    - Find the process which priority is same to ***top_priority***.



​     
    - To adopt **Round-Robin** policy, Search all of **ptable** entries **from next of entry scheduled just before**.
    This allows you not only the same level of processes use CPU fairly but also responds new or higher process quickly as it check priority in every tick.
    Still, It doesn't change its level until it spends all time allotment.![RR](/uploads/d94d6343a363a24437d61bd9be4904c6/RR.JPG)
    
    ​```c
        // Search from next ptable entry to end.
        // temp_p is the next of entry that scheduled just before.
        for(p = temp_p; p < &ptable.proc[NPROC]; p++){
            if(p->state != RUNNABLE || p->priority != top_priority
                || p->isstride != 0) {
                continue;
            }
            pflag = 1;
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
                break;
            }
        }
    ​```

- Switch to chosen process.
- When a process switches context from itself to ***CPU->scheduler***, 
  we assume that process spend all of the time allotment regardless of whether it **yields** CPU itself. 
  This will prevent gaming of scheduler and I will talk about it in the last part.
- Check if the process spend all time quantum. If it spent all, reset process' tick value and lower its level.


    ​```c
    // Check whether the process spend all time quantum.
    // Reset process' tick value and lower down its priority.        
    if(p->tick >= time_quantum[p->priority]){
      if(p->priority < 2) p->priority++;
      p->tick = 0;
    }
    ​```

- Check total spend tick is over than 100.
    If it is, set all process' priority on top and spend tick to 0. Set **totaltick** to 0. 


    ​```c
    // Priority boost.
    if(totaltick > 100) {
      for(temp_p = ptable.proc; temp_p<&ptable.proc[NPROC]; temp_p++){
        temp_p->priority = 0;
        temp_p->tick = 0;
      }
      totaltick = 0;
      cprintf("[Priority Boost]\n");
    }
    ​```





## 3. Combination of Stride scheduling with MLFQ
-----
### 0) Flow Chart
![stride_flow](/uploads/ee325618113aa2706acc63138af414f7/stride_flow.JPG)
### 1) Data Structure
Add member properties to the ***proc*** structure to determine whether the process call ***set_cpu_share***.
When the OS running MLFQ and search for next process, MLFQ will pass the process entry if  ***p->isstride == 1***.
It is because it should **not run in MLFQ, but in Stride**.

```c
struct proc {
  (...)
  // New member for Stride
  int isstride;                  // Check whether stride or not
};
```

```c
if(p->state != RUNNABLE || p->priority != top_priority || p->isstride != 0)
    continue;
```

Create new data structure type of ***stride***. It will indicate Stride process.
It has members that are needed for Stride Scheduling.
Then, Create ***stable*** which is 64(the maximum number of process is NPROC in xv6) array of ***stride***.
The ***stable*** will contain **MLFQ process**(dummy) and the stride process calling ***set_cpu_share*** in MLFQ. 

```c
struct stride {
  struct proc *proc;        // Dummy has proc that scheduled just before in MLFQ
  int pass;                 // Pass amount of process
  int stride;               // Stride of process.
  int portion;              // Portion of CPU that process has.
  int dummy;                // Check Stride process is for MLFQ.
  int enabled;              // Check entry is occupied or not.
};

struct stride stable[NPROC];
```



### 2) Abstraction

- If a process wants to get a certain amount of CPU share, then it invokes a new
  system call to set the amount of CPU share.
- When a process is newly created, it initially enters MLFQ. The process will be
  managed by the stride scheduler only if the set_cpu_share() system call has been
  invoked.
- The total sum of CPU share requested from processes in the stride queue can
  not exceed 80% of CPU time. Exception handling needs to be properly implemented
  to handle oversubscribed requests.

### 3) Implementation
- Make a syscall , ***set_cpu_share(portion)***.
   It loops stable and find empty entry for stride process, set the member value properly.
    By setting ***proc->isstride = 1*** , MLFQ will just pass by this process entry.
    Then it yields current process. It will not execute the code after ***set_cpu_share(portion)***,
    and CPU will schedule next process by Stride Scheduling policy.
- Every time process calls ***set_cpu_share***, it will reset all of stride process' pass and stride. 
  But it will not reset pass and stride, when some process is dead, because it still have same portion stride and proper pass.

    ```c
    int 
    set_cpu_share(int portion)
    {
      (...)
      for(s = stable; s < &stable[NPROC]; s++){      
        // Table entry that not used
        if(s->enabled != 1){
          proc->isstride = 1;
          s->proc = proc;
          s->portion = portion;
          stable[0].portion -= portion;
          s->enabled = 1;
          portion_to_stride();
          reset_pass();
          yield();
          return 1;
        }
      }
      (...)
    }
    ```


- Initialize **Stride process for MLFQ**.
   ***proc*** has the pointer of ***ptable*** entry which was scheduled just before.
    Everytime MLFQ loops ***ptable*** for next process, it start searching from next of entry scheduled just before.  If MLFQ schedules next process, renew ***stable[STRIDEMLFQ].proc*** to that one.
    This is the extension of **Round-Robin** policy which we discussed in MLFQ.

    ```c
    // Initialize Stride table.
    // MLFQ dummy is allocated in stable[SMLFQ].
    stable[STRIDEMLFQ].proc = ptable.proc;
    stable[STRIDEMLFQ].pass = 0;
    stable[STRIDEMLFQ].stride = 0;
    stable[STRIDEMLFQ].portion = 100;
    stable[STRIDEMLFQ].dummy = 1;
    stable[STRIDEMLFQ].enabled = 1;
    ```

    ```c
    stable[0].proc = p;
   ...
    swtch(&cpu->scheduler, p->context);
    ...
    ```
- Searching ***stable*** to find the process that has smallest pass.
- Switch to chosen Stride process. 
    - If dummy is 1 , Run MLFQ scheduler. 
    - Else switch to stride process. When process yield CPU, check process state.
        If state is ***UNUSED*** (ended process), disable its entry from ***stable***.

    ```c
    // Switch to process selected by Stride Scheduler.
    // If dummy is 1, Call MLFQ. Else, Start process in stable.
    if(s->dummy == 1){
      s->pass += s->stride;
      MLFQ_scheduler();
    }
    else{
      proc = s->proc;
      switchuvm(s->proc);
      s->proc->state = RUNNING;
      s->pass += s->stride;
      swtch(&cpu->scheduler, s->proc->context);
      
      switchkvm();
      proc = 0;
      if(s->proc->state != UNUSED){
        s->enabled = 0;
        stable[0].portion += s->portion;
      }
    }
    ```

### 4. Gaming of scheduler
--------------------------

In MLFQ, if the process yields without using all of the given time allotment, it can cause problems if you do not record the used time. It will always stay on highest prior level, and this is what we called gaming scheduler.

We can prevent gaming of scheduler by adding ticks regardless of whether the process yield CPU before its time allotment.  Because even though process spend 0.1 tick and yield the CPU, the scheduler consider the process use its time allotment tick. 

I referenced this sentence on our textbook.
![gaming](/uploads/7a5efd93d4e050075b159affa4dd9dcc/gaming.JPG)

So when we are running the process continuously yield the CPU(mlfq_test 1 case), we will face many time of priority boost. 
Because it will add tick faster than normal. But we can prevent gaming by this.