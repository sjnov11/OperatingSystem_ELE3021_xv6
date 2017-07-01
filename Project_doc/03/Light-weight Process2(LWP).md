

# Light-weight Process(LWP)

## Operating Systems Programming Project with xv6

Updated 05/21 2017 (with 1st milestone)

First in English, then Korean.



### 1. Basic LWP Operations 

#### 1) Create

##### int thread_create(thread \*thread, void \* (\* start_routine)(void *), void \*arg);

- **thread** - returns the thread id.

- **start_routine** - the pointer to the function to be threaded. The function has a single argument : pointer to void.

- **arg** - the pointer to an argument for the function to be threaded. To pass multiple arguments, send a pointer to a structure.

- **Return value** - On success, **thread_create** return 0. On error, it returns a non-zero.

  ​

  ##### (1) Design

  ​

  - In xv6, the address space of a process allocates *code* and *data* from VA(virtual address) 0. *Guard page* and *user stack* are allocated one page each just above them.

  - When process(*main thread*) creates new thread(*sub thread*), it allocates two pages to the process's(*main thread*) address space, one for guard page and one for user stack.

  - Also, *sub thread* creates and sets a kernel stack for its own trap frame and context.

  - All of this sequence is simillar to creating new process with *fork*, except that it allocates its own user stack on parent process's(*main thread*) *pgdir* rather than creating new *pgdir* and copying the parent's.

    ![design](C:\Users\u_nov\Desktop\갓갓갓갓\17-1학기 수업자료\운영체제\project\03\design.JPG)


  - xv6에서 address space 는 virtual address 0 에서 부터 code 와 data를 할당하고, 그 위에 guardpage와 user stack을 PAGESIZE (4096) 만큼 할당해준다.

  - 프로세스가 새 스레드를 생성하였을 때, 그것은 두 페이지를 address space에 하나는 guard page로, 또 하나는 user stack으로 할당한다.

  - 또한, sub thread는 자기만의 trap frame 과 context를 담아 두기 위한 kernel stack을 만든다.

  - 이 모든 과정은 pgdir을 새로 만들고 부모 프로세스의 pgdir를 복사하기보다, 부모 프로세스의 pgdir 에 자신의 stack을 할당한다는 점을 빼고 새 프로세스를 fork 로 만드는 과정과 유사하다.

    ​

  #####  (2) Implementation

  ​

  - Allocate **process** for thread by *allocproc* function. 
    *allocproc* finds UNUSED state ptable entry, intializes it. Then it allocate kernel stack for process(thread), sets trapframe and context pointer.

    - Thread의 process를 allocproc 함수를 이용하여 만든다.
      allocproc은 ptable의 UNUSED 상태 entry 를 찾고 그것을 초기화 한다. 그런 다음, process(thread)를 위한 kernel stack을 할당하고, trapframe 과 context 의 pointer 를 설정한다.

    ```c
    if((np = allocproc()) == 0){
      return -1;
    }
    ```

  - Allocate **user stack** for thread by *allocuvm* function.
    *allocuvm* allocates page tables and physical memory to process pgdir from oldsz to newsz. It passes current process's pgdir as parameter, because *sub thread* should share its address space with *main thread*. And we allocates double PGSIZE, one for *guard page*, another for *user stack*. *clearpteu* sets inaccessible for *guard page*.

    - Thread의 user stack을 allocuvm 함수를 통해 생성한다.
      allocuvm은 pgdir의 oldsz 부터 newsz 까지 page table 과 physical memory 를 할당한다. 현재 process의 pgdir을 parameter로 넘겨주는데, 이는 sub thread가 main thread의 address space를 쓰기 위함이다. 그리고 우리는 2개의 PGSIZE를 할당하는데, 하나는 guard page를 위해, 또 하나는 user stack으로 사용한다. clearpteu는 guard page에 접근할 수 없도록 설정한다.

    ```c
    if((sz = allocuvm(proc->pgdir, oldsz, oldsz + 2*PGSIZE)) == 0){
      return -1;
    }
    clearpteu(proc->pgdir, (char *)(sz - 2*PGSIZE));
    ```

  - For new thread execution, set up thread's user stack. each of stacks represents a return address and a variable.

    - 새 thread 실행을 위해 thread의 user stack 을 설정한다. 각각 return address와 실행 argument를 나타낸다.

    ```c
    ustack[0] = 0xffffffff;
    ustack[1] = (uint)arg;
    sp -= 2*4;
    if(copyout(proc->pgdir, sp, ustack, 2*4) < 0){
      deallocuvm(proc->pgdir, sz, oldsz);	// deallocate when copy failed.
    }
    ```

  - Set *sub thread's* pgdir to current process(*main thread*)'s pgdir. Change sz of main thread to the top of stack of sub thread(*thread's sz*). And set *sub thread'*s *trap frame, eip, esp.* Finally, change *sub thread's* state to RUNNABLE to scheduled by scheduler.

    - 생성된 thread의 pgdir을 현재 process(main thread)의 pgdir으로 설정한다. main thread의 sz를 생성된 thread의 sz가 가리키고 있는 곳(stack의 가장 윗부분)으로 두고, thread의 trapframe, eip, esp 를 설정한다. 마지막으로 sub thread의 state를 RUNNABLE로 두어, scheduler 가 스케쥴링 할 수 있도록 한다.

    ```c
    np->pgdir = proc->pgdir;
    proc->sz = sz;
    np->sz = sz;
    np->parent = proc;
    *np->tf = *proc->tf;
    np->tf->eip = (uint)start_routine;
    np->tf->esp = sp;

    // Ellipsis ..

    np->state = RUNNABLE;
    release(&ptable.lock);
    ```



#### 2) Exit

##### void thread_exit(void \*retval);

- **retval** - return value of the thread.

  ​

  ##### (1) Design

  - Change state of thread process to ZOMBIE to stop being scheduled, and jump to scheduler.

  - Set thread's proc->retval to input parameter *retval*. return of thread will be managed by thread's PCB.

    ```c
    struct proc {
      // Ellipsis ..
      void *tret;
    }
    ```

  - All sequence of **thread_exit** are simillar to **exit** call.

    ​

  - Thread 가 더이상 schedule 되지 않도록 프로세스의 상태를 ZOMBIE 로 바꾼 다음, scheduler 로 jump 한다.

  - Thread 프로세스의 retval 값을 입력받은 retval 로 설정한다. thread 의 return 값은 thread의 PCB에 의해서 관리 되어진다.

  - **thread_exit** 는 기존 process **exit** 와 비슷하다.

    ​

  ##### (2) Implementation

  - Wake up *main thread* sleeping in **thread_join**.

    - **thread_join** 에서 sleep 하고 있는 *main thread* 를 깨운다.

    ```c
    wakeup1(proc->parent);		// proc is sub thread. parent is main thread.
    ```

  - Change thread's *state* to ZOMBIE, thread's *tret* to input parameter *retval*. Then jumps to the **sched()** to schedule another process.

    - Thread 의 상태를 ZOMBIE 로 바꾸고, tret 값을 입력받은 retval로 둔다.  

    ```c
    proc->state = ZOMBIE;
    proc->tret = retval;
    sched();
    panic("zombie exit thread");
    ```



#### 3) Join

**int thread_join(thread_t thread, void \*\*retval);**

- **thread** - thread id allocated on **thread_create**.

- **retval** - the pointer for return value.

- **return value** - On success, **thread_join** returns 0. On error, it returns  a non-zero.

  ​

  ##### 1) Design	

  - If the thread of input variable isn't exited, the main thread will go to sleep.

    - To manage new stack allocation easy, proc->sz of *main thread* always points on the top of allocated stack.

    - When the thread which of user stack is located on the top of allocated memory terminates, it also free the consecutive stack of terminated processes. 

      ​

    - 입력받은 thread가 아직 종료되지 않았다면, sleep 상태로 바뀐다.

    - 추가 stack 할당을 쉽게 하기 위해서, main thread 의 proc->sz 는 항상 할당된 stack의 가장 윗부분을 가리키고 있게 하였다.

    - 가장 위에 위치한 user stack의 thread가 종료될 때, 연속되는 종료된 프로세스의 stack도 함께 free 해준다. 

  ​

  ##### 2) Implementation

  - Scan through *ptable*, find thread to join. if there are no such thread, return -1.

    - Process table 을 조회하여, join 하는 thread를 찾는다. 해당 thread가 없을 경우, -1을 리턴한다.

    ```c
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc || p->pid != (int)thread)
      	continue
      havethread = 1;
    // Ellipsis ..
    } 	
    if(!havethread || proc->killed){
      release(&ptable.lock);
      return -1;
    }
    ```


  - Check whether thread state is ZOMBIE. If not, sleep current process(*main thread*).

    - 찾은 thread의 상태가 ZOMBIE인지 확인한다. 아니라면 현재 프로세스 (*main thread*)를 sleep 시킨다.

  - If thread state is ZOMBIE, set retval to p->tret which we set at exit. Then compare thread size to main thread size. If these values are same, because it means the thread its user stack is located on top, free thread user stack. If not, just return 0, not free stack memory.

    - thread 상태가 ZOMBIE면 retval 을 exit에서 저장해뒀던 p->tret으로 설정한다. 그리고 thread의 sz 가 main thread의 sz와 같은지 비교한다. 같다면 가장 위에 위치한 stack을 의미하므로 free 시켜준다. 다르다면, stack 메모리를 free 시키지 않고 return 한다.

    ```c
    if(p->state == ZOMBIE){
      *retval = p->tret;
      tsz = proc->sz;
      
      if(p->sz == tsz){
        kfree(p->kstack);		// free thread kernel stack
        p->stack = 0;
        deallocuvm(p->pgdir, p->sz, p->sz-2*PGSIZE);  // free thread user stack
        // Ellipsis ..
      }
    }
    // Ellipsis..
    sleep(proc, &ptable.lock);
    ```

  - When top stack is freed, check the thread state of next stack (located at sz - 2*PGSIZE). When the state is ZOMBIE, it also frees its stack. This process will continue until the state is not ZOMBIE or it points to the main thread stack.

    - 가장 윗부분의 stack이 free 되었을 때, 다음 stack (-2*PGSIZE) 의 thread 상태를 확인하여, 상태가 ZOMBIE 이면 마찬가지로 free 한다. 이 과정을 상태가 ZOMBIE 가 아닐 때 까지 혹은 main thread 스택을 가리킬 때 까지 진행한다. 

    ```c
    for(;;){
      tsz -= 2*PGSIZE;
      // Check if sz exceeds the main stack.
      if(tsz < proc->tf->esp + 2*PGSIZE)	
      	break;
      // Find thread with tsz.
      for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++){
        if(p1->sz == tsz)
       	  break;
      }
      // Check if found thread is terminated.
      if(p1->state != ZOMBIE)
      	break;
      	
      kfree(p1->kstack);		// free thread kernel stack
      p1->kstack = 0;			
      deallocuvm(p1->pgdir, p1->sz, p1->sz-2*PGSIZE); // free thread user stack
      // Ellipsis ..
    }
    ```
    ### 

### 2. Interaction with other servieces in xv6

#### 0) Data Structure

##### (1) Proc structure

- To capture the main thread of process, add *mthread_p* member to PCB.

- To seperate heap and stack area,  add *bstack* member to PCB.

  - 프로세스의 메인 스레드를 알기 위해, PCB에 mthread_p 라는 멤버 변수를 추가.
  - 힙 영역과 스택 영역을 분리 시키기 위해, 스택의 bottom을 가리키는 멤버 변수를 추가.

  ```c
  struct proc {
    ...
    struct proc *mthread_p;      // Main thread process

    uint bstack;                 // Bottom of Stack
  };
  ```

##### (2) Stable structure

- When LWPs call **cpu_share** (running on **stride**), multiple threads access *stable* so it causes concurency problems. To prevent this, *stable* also has *spinlock*.

  - LWP가 cpu_share 할 경우, 여러 스레드가 동시에 stable 에 접근하여 발생하는 concurrency 문제를 해결하기 위해, stable 에도 spinlock을 추가였다.

  ```c
  struct {
    struct spinlock lock;
    struct stride str[NPROC];
  }stable;
  ```

#### 1) Exit

- When a LWP calls the exit system call, all LWPs are terminated and all resources used for each LWP must be cleaned up and the kernel can reuse it at a later time. Also, no LWP should survive for a long time after the exit system call is executed.

#####  (1) Design and Implement

**ExitLWP(int arg)** 

-  If *arg* is 0, then exits all LWP. 

  - arg 가 0인 경우, 모든 LWP를 종료한다. 

  ```c
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->mthread_p == main_t || p == main_t){      
          // Below codes are Similar to exit().
          ...     
          p->state = ZOMBIE;
      }
      sched();  
  }
  ```


- When the LWP calls **exit()**, exits all LWPs through **ExitLWP(0)**. The parent process of main thread will free the LWP memory.

  - LWP 가 exit() 를 호출하였을 때, ExitLWP(0) 함수를 통해 모든 LWP를 종료하고, 메인 thread의 부모 프로세스가 LWP 메모리를 free 한다.

  ```c
  // In wait() function..
  if(p->state == ZOMBIE){
  	// Found one.

      // Clear thread.
      for(t = ptable.proc; t < &ptable.proc[NPROC]; t++){
      	if(t->mthread_p == p && t->state == ZOMBIE){    		
      		kfree(t->kstack);
               t->kstack = 0;
               t->pid = 0;
               t->parent = 0;             
               // ...
            }
          }
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);			// free memory
    	    // ...
      }
  }      
  ```



#### 2) Fork

- Even if multiple LWPs call the fork system call at the same time, a new process must be created according to the fork’s behavior and the address space of the LWP must be copied normally. You should also be able to wait for the child process normally by the wait system call. Note the parent-child relationship between processes after the fork.

##### (1) Design and Implement

- Copy the process's pgdir by **copyuvm** like normal process **fork**.

- Make parent-child relationship between the process calling **fork** and new process by **fork**, so that the parent process could waits for child process.
  - 일반적인 process 의 fork와 같이 copyuvm으로 process 의 pgdir 을 그대로 복사한다.
  - thread 가 fork 로 생성한 프로세스와 thread 사이의 부모 자식 관계를 두어 부모 프로세스가 자식 프로세스를 기다릴 수 있도록 한다.

  ​

#### 3) Exec

- If you call the exec system call, the resources of all LWPs are cleaned up so that the image of another program can be loaded and executed normally in one LWP. At this time, the process executed by the exec must be guaranteed to be executed as a general process thereafter.

##### (1) Design and Implement

**ExitLWP(int arg)** 

-  If *arg* is 1, then exits all LWP except caller and main thread. 

  - arg 가 1인 경우, caller 프로세스와 main thread를 제외한 모든 LWP를 종료한다. 

  ```c
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->mthread_p == main_t || p == main_t){          
          // Below codes are Similar to exit().
          if(arg == 1 && p == proc)
              continue;
          if(arg == 1 && p == main_t)
              continue;              
        
          // ...     
          p->state = ZOMBIE;
      }
      sched();  
  }
  ```

- When LWP calls **exec**, exits all LWP except main thread and current thread. If it exits main thread, then parent process of main thread will free its memory before current thread exec. So main thread will be terminated when sub thread execute exec sucessful. Likewise, if LWP calls **exec**, remain *oldpgdir*, so that parent of main thread will be able to free its memory.

  - LWP 가 exec을 호출하였을 때, 메인 thread 와 호출한 현재 LWP 를 제외한 모든 프로세스들을 종료한다. 메인 thread를 종료 시켜버리면, 메인의 부모가 메모리를 free 시켜버리게 되므로, sub thread의 exec이 다 진행되고 exit를 부를 때 종료시킨다. 마찬가지로, LWP를 exec할 경우는 기존 oldpgdir 을 남겨두어 메인 thread의 부모가 메모리를 free 할 수 있도록 한다.

  ```c
  // In exec() function.. 
  if(isLWP(proc) == 1){
      ExitLWP(1);
  }
  // ...
  if(isLWP(proc) == 0)
      freevm(oldpgdir);
  ```



#### 4) Sbrk

- When multiple LWPs simultaneously call the sbrk system call to extend the memory area, memory areas must not be allocated overlapping with each other, nor should they be allocated a space of a different size from the requested size. The area expended by it must be shared among LWPs.

##### (1) Design and Implement

- Place **heap** area above code and data, **stack** area below to KERNBASE - PGSIZE.

  - 힙 영역을 데이터와 코드 위에, 스택 영역을 KERNBASE - PGSIZE 에 위치 시킴으로써, 두 segment 를 효율적으로 분리하였다.

- The confilction between the heap and the stack are is not considered.

  - 힙 영역과 스택 영역의 충돌은 고려하지 않았다. (corner case를 따로 만들지 않음)

- All codes related to managing memory has been modified.

  - 메모리 영역을 관리하는 모든 코드들을 수정하였다.

- Fork : copy its memory through **copyuvm**.

  - Fork 에서 copyuvm을 통해 메모리를 복사할 때.

  ```c
  pde_t*
  copyuvm(pde_t *pgdir, uint sz, uint bstack)
  {
    // Copy heap.
    for(i = 0; i < sz; i += PGSIZE){
      // ...
    }
    // Copy stack.
    for(i = bstack; i < KERNBASE - PGSIZE; i += PGSIZE){
      // ...
    }

    return d;
  }
  ```

- Exec : Allocate two pages for user stack and guard page.

  - Exec 에서 user stack과 guard page를 만들 때.

  ```c
  // In Exec() function ..
  // ...
  bstack = KERNBASE - 1;
  bstack = PGROUNDDOWN(bstack);
  bstack = bstack - 2*PGSIZE;
  if((sp = allocuvm(pgdir, bstack, bstack + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sp - 2*PGSIZE));
  // ...
  ```

- growproc : When grow the heap area by **growproc**, it uses its top of heap pointer by 

  *main thread->sz*.

  - growproc에서 heap영역을 늘일 때, main thread 의 sz를 통해서 늘인다.

  ```c
  // In growproc() function..
  sz = main_t->sz;
  if(n > 0){
    if((sz = allocuvm(main_t->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(main_t->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  main_t->sz = sz;
  ```



#### 5) Kill

- If more than one LWP is killed, all LWPs must be terminated and the resources for each LWPs in that process must be cleaned up. After the kill to a LWP is called, no LWP should survive for a long time.

##### (1) Design and Implement

- When thread process is killed, the killed process calls exit in trap when it has been scheduled. At this time, as mentioned in the above, **exit()**  terminates and clear all LWPs.

  - thread 가 kill 되면, trap 에서 해당 thread 프로세스가 exit를 호출하여 종료한다. 이 때, 위exit 에서 말했듯이 exit 호출 시 자연스레 모든 LWP를 종료하고 clear 한다.

  ```c
  // In trap() function..
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
  ```



#### 6) Sleep

- When a specific LWP executes a sleep system call, only the requested LWP should be in the sleeping state for the requested time. If a LWP is terminated, sleeping LWP also has to be terminated.

##### (1) Design and Implement

- When thread process calls **sleep**, like normal process calling sleep, it changes its state to SLEEP and doesn't change until signals come in through the channel.
  - thread가 sleep 호출 시 일반적인 process 와 마찬가지로 상태를 SLEEP 으로 바꾸고 channel을 통해 신호가 들어 오기 전까지 상태를 바꾸지 않는다. 
- When the other LWP has exited, it goes **exit** and exit all LWPs including SLEEP state LWP.
  - 다른 LWP 가 종료되었을 때, 자연스럽게 exit로 넘어가서 SLEEP 상태의 LWP를 포함하여 모든 LWP를 종료한다.



#### 7) Pipe

- All LWPs must share a pipe and when reading or writing and data should be synchronized and not be duplicated.

##### (1) Design and Implement

- To share synchronoize pipe between LWPs, shares main thread file descriptor when creates new thread.

  - 동기화된 pipe를 LWP끼리 공유하기 위해서, thread를 생성시에 main thread의 file descriptor를 공유시킨다.

  ```c
  // Copy parent's file descriptor
  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
  ```



#### 8) Scheduler

- LWPs are scheduled by MLFQ scheduler like the normal process by default. If a LWP calls set_cpu_share function, all threads belonging the same process are scheduled by Stride scheduler and share the time slice for that process. When a process is already being scheduled by Stride scheduler, subsequent threads spawned in the process will share the time slice for the process.

##### (1) Design and Implement

- When create thread after **cpu_share**, set stride entry for creating thread at **thread_create** and reset all LWPs stride value. The New stride value will be (main thread's stride * the number of LWPs).

  - cpu_share를 하고 thread를 생성할 경우, thread_create 에서 thread를 위한 stride entry 를 만들고 main thread의 stride 를 통해 모든 LWP 의 stride를 재설정한다. 이 때, stride 값은 main thread의 stride * LWP 의 갯수이다.

  ```c
  int
  set_thread_cpu_share(struct proc *thread_p, int type)
  {
    //...
    for(s = stable.str; s < &stable.str[NPROC]; s++){      
      // Stride entry for LWP
      if(s->enabled != 1){
        thread_p->isstride = 1;
        s->proc = thread_p;
        // ...
        stride = main_s->stride / (numberofthread - 1) * (numberofthread);
        // ...
    }
  }
  ```

  ```c
  // In thread_create() function..
  if(proc->isstride == 1){	
  	if(set_thread_cpu_share(np, 0) == 0) {
          panic("thread cpu share panic!");
          return -1;
      }
  }
  ```

- When create thread before **cpu_share**, set stride entry for all of created thread at **set_cpu_share**. The stride value will be (main thread's stride * the number of LWPs). When another **cpu_share** is called, first set the main thread's stride by its portion then set all LWPs stride value to (main thread stride * number of LWPs).

  - thread를 생성하고 cpu_share 를 할 경우, set_cpu_share에서 생성된 모든 thread를 위한 stride entry를 설정한다. stride 값은 main thread의 stride * LWP의 갯수이다. 새로운 cpu_share 가 불렸을 때, 먼저 main thread의 stride를 portion에 맞게 설정한 다음, LWP에 맞게 새로운 stride 값을 설정한다.

  ```c
  for(s = stable.str; s < &stable.str[NPROC]; s++){
    if(s->enabled == 1 && s->proc->isthread == 0){      
      // Reset stride for process by its portion
      s->stride = product_portion / s->portion;
     
      // Reset stride for LWP
      if(isLWP(s->proc) && s->dummy != 1){               
        s->stride = s->stride * NumberOfThread(s->proc);
              set_all_LWP_stride(s->proc, s->stride * NumberOfThread(s->proc));    
      }    
    }
  }
  ```

  ​