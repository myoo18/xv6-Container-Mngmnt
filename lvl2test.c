#include "types.h"
#include "user.h"

void test_priority_setting() {
    printf(1, "Test Case 1: Setting priorities\n");

    int pid1 = fork();
    if (pid1 == 0) {
        int result = prio_set(getpid(), 5);
        if (result == 0) {
            printf(1, "Child 1 priority set successfully.\n");
        } else {
            printf(1, "Error setting priority for Child 1.\n");
        }
        exit();
    } else {
        wait();
    }

    int pid2 = fork();
    if (pid2 == 0) {
        int result = prio_set(getpid(), 3);
        if (result == 0) {
            printf(1, "Child 2 priority set successfully.\n");
        } else {
            printf(1, "Error setting priority for Child 2.\n");
        }
        exit();
    } else {
        wait();
    }

    int pid3 = fork();
    if (pid3 == 0) {
        int result = prio_set(getpid(), 7);
        if (result == 0) {
            printf(1, "Child 3 priority set successfully.\n");
        } else {
            printf(1, "Error setting priority for Child 3.\n");
        }
        exit();
    } else {
        wait();
    }

    printf(1, "Test Case 1 complete.\n");
}

void test_sleep_print_order() {
    printf(1, "Test Case 2: Sleeping and printing order\n");

    int pid1 = fork();
    if (pid1 == 0) {
        sleep(100);
        printf(1, "Child 1\n");
        exit();
    }

    int pid2 = fork();
    if (pid2 == 0) {
        sleep(100);
        printf(1, "Child 2\n");
        exit();
    }

    int pid3 = fork();
    if (pid3 == 0) {
        sleep(100);
        printf(1, "Child 3\n");
        exit();
    }

    prio_set(pid1, 3);
    prio_set(pid2, 8);
    prio_set(pid3, 4);

    wait();
    wait();
    wait();

    printf(1, "Test Case 2 complete.\n");
}

void test_round_robin() {
    printf(1, "Test Case 3: Round Robin Scheduling\n");

    int pid1 = fork();
    if (pid1 == 0) {
        sleep(100);
        printf(1, "Child 1\n");
        exit();
    }

    int pid2 = fork();
    if (pid2 == 0) {
        sleep(100);
        printf(1, "Child 2\n");
        exit();
    }

    int pid3 = fork();
    if (pid3 == 0) {
        sleep(100);
        printf(1, "Child 3\n");
        exit();
    }

    prio_set(pid1, 5);
    prio_set(pid2, 5);
    prio_set(pid3, 5);

    wait();
    wait();
    wait();

    printf(1, "Test Case 3 complete.\n");
}

void test_lower_priority() {
    printf(1, "Test Case 4: Lowering Own Priority\n");

    int pid = fork();
    if (pid == 0) {
        prio_set(pid, 7);
        printf(1, "Original Priority: 7\n");
        printf(1, "Lowering Own Priority to 3\n");
        prio_set(pid, 3);
        printf(1, "New Priority: 3\n");

        exit();
    } else {
        wait();
    }

    printf(1, "Test Case 4 complete.\n");
}

int main() {
    test_priority_setting();
    test_sleep_print_order();
    test_round_robin();
    test_lower_priority();

    exit();
}