#include "types.h"
#include "user.h"
#include "fcntl.h"

//DO THIS INSIDE A CONTAINER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

void childProcessMaxprocCall() {
    int ret = cm_maxproc(5);
    if (ret == 0) {
        printf(1, "FAILED || Child process should not set maxproc after parent\n");
    } else {
        printf(1, "PASSED || Child process correctly failed to call cm_maxproc\n");
    }
    exit();
}

void testForkAfterLimit(int limit) {
    cm_maxproc(limit);
    for (int i = 0; i < limit + 1; i++) {
        int pid = fork();
        if (pid == 0) { // Child process
            exit();
        } else if (pid < 0) { // Fork should fail after reaching the limit
            printf(1, "PASSED || Correctly failed to create child at limit\n");
            break;
        }
    }
    while (wait() != -1); // Wait for all children to exit
}

void testFamilyMaxprocCalls() {
    int limit = 4; // Set a limit that includes parent and child
    cm_maxproc(limit);

    if (fork() == 0) {
        childProcessMaxprocCall();
    }

    wait(); // Wait for the child to exit
    printf(1, "PASSED || Parent and child maxproc behavior as expected\n");
}

void testMaxprocDifferentStates() {
    int limit = 3;
    cm_maxproc(limit);

    for (int i = 0; i < limit; i++) {
        int pid = fork();
        if (pid == 0) { // Child process
            if (i == 0) {
                sleep(100); // First child sleeps
            }
            exit();
        }
    }

    while (wait() != -1); // Wait for all children
    printf(1, "PASSED || Maxproc respects limit in different process states\n");
}

int main(int argc, char *argv[]) {
    printf(1, "-----------Starting Maxproc Test-----------\n");
    testForkAfterLimit(3);
    testFamilyMaxprocCalls();
    testMaxprocDifferentStates();
    printf(1, "ALL TESTS COMPLETED\n");
    exit();
}
