#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    int pid,ppid;
    pid = getpid();
    ppid = getppid();

    printf(1, "My pid is : %d\n", pid);
    printf(1, "My ppid is : %d\n", ppid);
    exit();
}
