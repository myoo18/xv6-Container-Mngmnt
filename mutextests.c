#include "types.h"
#include "user.h"
#include "mutex.h"

int
mutex_test0(void){
    //mutex create
    printf(1,"module_3 test0 | mutex_create | ");
    if(mutex_create("mutex0") != 0) {
        printf(1, "FAILED | test0 | muxid incorrect \n"); //Get first available slot
        return -1;
    }
    if(mutex_create("mutex1") != 1) {
        printf(1, "FAILED | test0 | second muxid incorrect\n"); //Second available slot
        return -1;
    }
    printf(1, "PASSED\n");

    printf(1, "module_3 test0 | mutex_delete | delete mutex0 | ");
    mutex_delete(0); //delete the first mutex mutex0
    if(mutex_create("mutex0") != 0){
        printf(1, "FAILED | test0 | muxid wrong after deletion expected : 0 \n");
        return -1;
    } 
    printf(1, "PASSED\n");

    return 1; //pass
}

int
mutex_test1(void){
    printf(1, "module_3 test1 | test static table | ");
    if(mutex_create("mutex2") != 2) return 0; //if static, next create should 
    printf(1, "PASSED\n");
    

    printf(1, "module_3 test1 | mutex_lock | "); //get lock, 

    //create mux
    int muxid = mutex_create("mutex_test");
    if (muxid < 0) {
        printf(1, "FAILED | test1 | Failed to create mutex\n");
        return -1;
    }

    //parent (doesnt know it yet) is locking mutex
    mutex_lock(muxid);
    printf(1, "PASSED\n");

    //This test lets the child try and get mutex, will get blocked and then is unlocked and can proceed
    printf(1, "module_3 test1 | child proc blocked mutex | ");
    int pid = fork();
    if (pid == 0) {
        //child proc gets blocked
        mutex_lock(muxid);
        printf(1, "PASSED \n");
        exit();
    } else {
        // Parent process
        sleep(100); //1 secs for child to try and fail to get lock (gets blocked)
        mutex_unlock(muxid);
        wait();
    }

    return 1;
}

    int *sharedResource;

    void
    increase(int muxid){    
        for(int i = 0; i < 5; i++){
            mutex_lock(muxid);
            *sharedResource+=15;
            printf(1, "increased sharedResource by 15 %d\n", *sharedResource);
            mutex_unlock(muxid);
            cv_signal(muxid);  
            sleep(100);

        }
    }

    void
    decrease(int muxid){
        while(*sharedResource < 40){
            cv_wait(muxid); //get here first
            printf(1 , "Waiting on sharedResource\n");
            mutex_unlock(muxid);
        }
        *sharedResource -= 40;
        printf(1, "Got sharedResource, decreased by 40 %d\n", *sharedResource);
        cv_signal(muxid);
        sleep(50);
        cv_signal(muxid);
    }

    int
    mutex_test2(){
        printf(1, "module_3 test2 | test cv_wait cv_signal | \n");
        sharedResource = (int *)shm_get("sharedMemory");
        if (sharedResource == (int *)-1) {
            printf(1, "Failed to allocate shared memory\n");
            return -1;
        }

        *sharedResource = 0;

        int muxid = mutex_create("mutex2");  //Unlocked from before
        int pid = fork();
        if(pid == 0){
            increase(muxid);
            exit();
        }else{
            decrease(muxid);
            wait();
        }

        shm_rem("sharedMemory");
        printf(1, "module_3 test2 | PASSED\n");
        return 1;
    }


/*
    Two "threads" will bounce wait and signal, output will be 0101010101
    Expected Flow
    Thread1  locked                 --> Thread2 sleeping
    Thread2 waiting for signal      --> Thread2 cv_wait
    Thread1 signal thread2          --> Thread2 signaled Thread1 cv_wait
    Thread2 signaled try lock       --> Thread2 signed, set to RUNNABLE mutex_lock
    Thread2 got lock | signal t1    --> Thread2 has lock signal thread1 to wakeup and thread2 unlock mutex
    Thread1 signaled mutex unlocke  --> Thread1 set to RUNNABLE, print PASSED message
    PASSSED
*/

void
test3_thread1(int muxid){
    for(int i = 0; i < 5; i ++){
        printf(1, "0");  
        sleep(10);
        cv_signal(muxid);
        cv_wait(muxid);
        mutex_unlock(muxid);
    }
    printf(1, " | PASSED\n");
}

void
test3_thread2(int muxid){
    for(int i = 0; i < 5;i++){
        cv_wait(muxid);
        printf(1, "1");
        mutex_unlock(muxid);
        cv_signal(muxid);
    }
}


int
mutex_test3(){
    printf(1 ,"module_3 test3 | test cv_wait and cv_signal | ");
    int muxid = mutex_create("mutex3");
    int pid = fork();
    if(pid == 0){
        test3_thread1(muxid);
        exit();
    }else{
        test3_thread2(muxid);
        wait();
    }
    return 1;
}

//Stress test mutex_init (not part of required api) and mutex create
//Mostly checking edge cases, and limits (what if I init 50 times (not really))
int
mutex_test4(){
    printf(1 ,"module_3 test4 | mutex_init stress | ");
    //mutex_init already called, lets call it again
    //Presumeably should set al  mutexes to unlocked, and to unused, reset muxid(shouldnt change)
    //Reset all pid to -1, reset wseq, re init all spin locks
    mutex_init();
    //Test mutex_test0 again, testception
    if(mutex_create("mutex0") != 0) {
        printf(1, "FAILED | test4 | muxid incorrect \n"); //Get first available slot
        return -1;
    }
    if(mutex_create("mutex1") != 1) {
        printf(1, "FAILED | test4 | second muxid incorrect\n"); //Second available slot
        return -1;
    }
    mutex_delete(0); //delete the first mutex mutex0
    if(mutex_create("mutex0") != 0){
        printf(1, "FAILED | test4 | muxid wrong after deletion expected : 0 \n");
        return -1;
    } 
    printf(1,"PASSED\n");
    
    mutex_init();

    printf(1 ,"module_3 test4 | mutex_create stress | ");
    for(int i = 0 ; i < MUX_MAXNUM; i ++){ //make max mutexes
        if(mutex_create("i") != i) {
            printf(1, "FAILED | test4 | create index not correct \n");
            return -1;
        }
    }
    if(mutex_create("over_max") != -1){
        printf(1, "FAILED | test4 | mutex created past defined limit\n");
        return -1;
    }
 
    printf(1, "PASSED\n");
    return 1;
}

int
mutex_test5(){
    printf(1 ,"module_3 test4 | mutex_delete stress | ");
    mutex_init();
    if(mutex_delete(0) != -1) {
        printf(1,"FAILED | test5 | was able to delete non-existent mutex\n");
        return -1;
    }
    if(mutex_create("0") != 0){
        printf(1,"FAILED | test5 | mutex_create did not create correct muxid\n");
        return -1;
    }
    
    //Test if mutex can be deleted when locked
    if(mutex_lock(0) != 1) return -1;
    if(mutex_delete(0) != -3) return -1;
    mutex_unlock(0);
    //delete mutex twice , first should work, second should not
    if(mutex_delete(0) != 1){
        printf(1,"FAILED | test5 | mutex_delete returned incorrect\n");
        return -1;
    }
    //should not work
    if(mutex_delete(0) != -1){
        printf(1,"FAILED | test5 | mutex_delete deleted non-existent mutex\n");
        return -1;
    }

    if(mutex_delete(-1) != -2){
        printf(1, "FAILED | test5 | mutex_delete doesnt catch muxid out of bounds\n");
        return -1;
    }

    if(mutex_delete(MUX_MAXNUM + 1) != -2){
        printf(1, "FAILED | test5 | mutex_delete doesnt catch muxid out of bounds\n");
        return -1;
    }

    printf(1,"PASSED\n");
    return 1;
}

//Part of test6
#define NCHILDREN  8        /* should be > 1 to have contention */
#define CRITSECTSZ 3
#define DATASZ (1024 * 32 / NCHILDREN)


void
child(int muxid, int pipefd, char tosend)
{
    int i, j;

    for (i = 0 ; i < DATASZ ; i += CRITSECTSZ) {
        /*
         * If the critical section works, then each child
         * writes CRITSECTSZ times to the pipe before another
         * child does.  Thus we can detect race conditions on
         * the "shared resource" that is the pipe.
         */
        mutex_lock(muxid);
        for (j = 0 ; j < CRITSECTSZ ; j++) {
            write(pipefd, &tosend, 1);
        }
        mutex_unlock(muxid);
    }
    exit();
}



//Pipe check similar to hw4
int
mutex_test6(){
    printf(1 ,"module_3 test6 | mutex_lock/unlock | ");

    mutex_init();

    int i;
    int pipes[2];
    char data[NCHILDREN], first = 'a';
    int muxid;

    for (i = 0 ; i < NCHILDREN ; i++) {
        data[i] = (char)(first + i);
    }

    if (pipe(&pipes[0])) {
        printf(1, "Pipe error\n");
        return -1;
    }

    if ((muxid = mutex_create("0")) != 0) {
        printf(1, "mutex creation error\n");
        return -1;  
    }

    for (i = 0 ; i < NCHILDREN ; i++) {
        if (fork() == 0) child(muxid, pipes[1], data[i]);
    }
    
    close(pipes[1]);
    //Child func calls passed
    while (1) {
        char fst, c;
        int cnt;

        fst = '_';
        for (cnt = 0 ; cnt < CRITSECTSZ ; cnt++) {
            if (read(pipes[0], &c, 1) == 0) goto done;

            if (fst == '_') {
                fst = c;
            } else if (fst != c) {
                printf(1, "RACE!!!\n");
            }
        }
    }
done:

    for (i = 0 ; i < NCHILDREN ; i++) {
        if (wait() < 0) {
            printf(1, "Wait error\n");
            return -1;
        }
    }

    mutex_delete(muxid);
    printf(1, "PASSED\n");
    return 1;
}



/*
This test checks if a proces that faults successfully unlocks all of its held mutexes
Run mutextests1 immedietly after the program faults
*/
int
mutex_test7(){
    printf(1 ,"module_3 test7 | fault unlock, run mutextests1 | ");

    mutex_init();

    //Keep in mind muxid is 0 this will be used again for following test
    if(mutex_create("mutex0") != 0) return -1;
    if(mutex_lock(0) != 1) return -1;
    //proc has thread
    //induce seg fault
    int *ptr = 0;
    *ptr = 10;

    //If reached here, then test is invalid
    return -1;
}


int
main(void){
    mutex_init(); //Failure of create_delete etc will test init
    if(mutex_test0() != 1) printf(1, "FAILED\n"); //mutex_create / delete 
    if(mutex_test1() != 1) printf(1, "FAILED\n"); //mutex_lock / unlock
    if(mutex_test2() != 1) printf(1, "FAILEED\n"); //cv_wait/signal
    if(mutex_test3() != 1) printf(1, "FAILED\n"); //cv_wait cv_signal 
    if(mutex_test4() != 1) printf(1, "FAILED\n"); //Stress test mutex_init, create
    if(mutex_test5() != 1) printf(1, "FAILED\n"); //Stress test mutex_delete
    if(mutex_test6() != 1) printf(1, "FAILED\n"); //mutex_lock/unlock similar to hw 4 using pipes
    if(mutex_test7() != 1) printf(1, "FAILED\n"); //Test lvl3 trap fault  
    exit();
}

