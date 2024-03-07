#include "types.h"
#include "user.h"

int main() {
    printf(1, "\nLevel 0 Test Starting\n");

    int priority = 10;
    int pid = getpid();

    int status = prio_set(pid, priority);

    if (status == 0) {
        printf(1, "\nSetting priority to %d...PASSED\n", priority);
    } 
    else {
        printf(1, "\nSetting priority to %d...FAILED: Incorrect priority\n", priority);
    }

    exit();
    return 0;
}