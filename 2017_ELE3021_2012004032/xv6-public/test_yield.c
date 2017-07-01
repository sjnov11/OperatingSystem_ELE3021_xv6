#include "types.h"
#include "stat.h"
#include "user.h"

int main() 
{
    int rc = fork();
    int i;

    if (rc < 0) {
        printf(1, "fork failed\n");
        exit();
    } else if (rc == 0) {       // child process
        for(i = 0; i < 100; i++) {
            printf(1, "Child\n");
            yield();
        }
    } else {
        for(i = 0; i < 100; i++) {
            printf(1, "Parent\n");
            yield();
        }
        wait();
    }
    exit();
    
    return 1;
}
