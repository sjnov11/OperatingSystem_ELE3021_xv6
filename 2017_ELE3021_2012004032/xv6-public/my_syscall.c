#include "types.h"
#include "stat.h"
#include "user.h"

int main () {
    int c,d;

    c = set_cpu_share(20);
    d = set_cpu_share(30);

    printf(1, "%d\n", c);
    printf(1, "%d\n", d);


    return 0;
}
