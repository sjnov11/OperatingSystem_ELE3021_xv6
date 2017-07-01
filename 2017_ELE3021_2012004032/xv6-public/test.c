#include "types.h"
#include "user.h"

void* myfunc2(void *arg)
{
  int tid = (int) arg;
  int rc;
  int wc;
  rc = fork();
  //printf(1, "Start~\n");
  if(rc < 0){
    printf(1, "fuckning err!\n");
    exit();
  }
  else if(rc==0){
    printf(1, "Hi , I am child (pid :%d)\n", (int)getpid());
    sleep(100);
    printf(1, "End sleep");
  }else{
    wc =1;
    //wc = wait();
    printf(1, "Hello, I am parent of %d (wc:%d) (pid:%d)\n", rc, wc, (int)getpid());
  }
    
  

  //sleep(50);

  //exit();
  thread_exit((void *)(tid + 1));
}

void* myfunc(void *arg)
{
  int tid = (int) arg;
  char *args[3] = {"echo", "echo is executed!", 0};
  sleep(1);
  printf(1, "Execute in thread %d\n", (int)getpid());
  exec("echo", args);

  //exit();
  thread_exit((void *)(tid + 1));
}

int main(){
  void* ret;
  int i;
  thread_t t[10];
  
  for(i = 0; i <10; i++){
    thread_create(&t[i], myfunc, (void *) 10);
  }
  for(i = 0; i<10; i++){
    thread_join(t[i], &ret);
  }
  //thread_exit(ret);
  //sleep(50);
  printf(1, "end!%d\n", (int)ret);
  exit();

  
}
