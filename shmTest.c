#include "types.h"
#include "stat.h"
#include "user.h"
#include <stddef.h>

#define TEST_REGION_NAME "testRegion"

//same operation in exit and vm.c edit exit in pro.c
//using assert TODO get remove and get again. show what happens remove twice. edge cases. get remove and get again. 

void update_shared_memory(char *shared_mem, char *new_data) {
    strcpy(shared_mem, new_data);
    exit();
    return;
}


int edgeing_testing() {
    char *shared_mem;
    char *mem_name = "SharedMemoryTest";
    char *new_data = "Updated Successfully!";
    char *initial_data = "Initial State";

    // Test 1: Allocate and write to shared memory
    shared_mem = shm_get(mem_name);
    if (shared_mem == (char *)-1) {
        printf(1, "Error: Failed to allocate shared memory\n");
        exit();
    }
    //f(1, "Test 1: Allocated memory at %p\n", shared_mem);
    strcpy(shared_mem, initial_data);

    // Fork a child process to modify shared memory
    if (fork() == 0) {
        update_shared_memory(shared_mem, new_data);
        exit();
    }

    wait();
    //printf(1, "\tShared memory after child process: %s\n", shared_mem);

    if (strcmp(shared_mem, new_data) != 0) {
        printf(1, "Error: Shared memory did not update correctly in Test 1!\n");
        exit();
    }

    // Test 2: Remove and then reallocate shared memory
    if (shm_rem(mem_name) != 0) {
        printf(1, "Error: Failed to remove shared memory\n");
        exit();
    }

    shared_mem = shm_get(mem_name);
    if (shared_mem == (char *)-1) {
        printf(1, "Error: Failed to reallocate shared memory\n");
        exit();
    }
    //printf(1, "Test 2: Reallocated memory at %p\n", shared_mem);

    if (strcmp(shared_mem, initial_data) == 0) {
        printf(1, "Error: Shared memory was not cleared correctly in Test 2!\n");
        exit();
    }

    //printf(1, "Shared Memory Test Completed Successfully\n");
    shm_rem(mem_name);


    // Test 3: Double remove on the same shared memory
    if (shm_rem(mem_name) != 0) {
        printf(1, "Error: Failed to remove shared memory for the first time\n");
        exit();
    }

    // Attempt to remove the same shared memory again
    int remove_result = shm_rem(mem_name);
    if (remove_result == 0) {
        printf(1, "Error: Second removal should have failed but succeeded\n");
        exit();
    } else {
        //printf(1, "Double remove test passed\n");
    }

    // Test 4: Attempt to allocate shared memory with a null name
    shared_mem = shm_get(NULL);
    if (shared_mem != (char *)-1) {
        printf(1, "Error: Allocation with null name should have failed\n");
        exit();
    } else {
        //printf(1, "Null name test passed\n");
    }

    // Test 5: Attempt to allocate shared memory with an invalid name
    char *invalid_name = "";  // Empty string as an example of an invalid name
    shared_mem = shm_get(invalid_name);
    if (shared_mem != (char *)-1) {
        printf(1, "Error: Allocation with invalid name should have failed\n");
        exit();
    } else {
        //printf(1, "Invalid name test passed\n");
    }
    shm_rem(invalid_name);
    return 0;
}



int main(){    

    edgeing_testing();
    edgeing_testing();
    exit();
}
