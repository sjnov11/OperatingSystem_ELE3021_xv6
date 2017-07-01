
struct stride {
  struct proc *proc;    // Dummy has proc pointer scheduled just before MLFQ
  int pass;             
  int stride;
  int portion;
  int dummy;            // If dummy is 1, this stride is for MLFQ
  int enabled;          // Check the entry is used or not

};

struct {
  struct spinlock lock;
  struct stride str[NPROC];
}stable;

