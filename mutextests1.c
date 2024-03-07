#include "types.h"
#include "user.h"
#include "mutex.h"


//RUN THIS AFTER mutextests.c

int
mutex_test0(){
    printf(1, "module_3 part 2 test0 | check fault unlocks |  ");
    //First test for static mutex table
    //Since we made a lock on first file, this next mutex should be 1
    if(mutex_create("mutex1") != 1) return -1;

    //Now test that mutex0 is unlocked since other file crashed
    if(mutex_lock(0) != 1) return -1;
    if(mutex_unlock(0) != 1)return -1;
    printf(1, "PASSED\n");
    return 1;
}

//Child locks mutex, forces parent to cv_wait
void
test3_thread1(int muxid){
    sleep(50);
    cv_wait(muxid);
    //will start sleeping, added to wseq, now kill it
    //will be on wseq when crash happens
}
//I actually dont know what will happen
void
test3_thread2(int muxid){
    mutex_lock(muxid);
    sleep(200);
    int *ptr = 0;
    *ptr = 10;
    
}

int
mutex_test1(){
    printf(1, "module_3 part2 test1 | mutex_lock /unlock stress | ");

    mutex_init();
    
    //Test double lock
    int muxid = mutex_create("mutex0");
    if(muxid != 0) return -1;

    if(mutex_lock(0) != 1) return -1;
    if(mutex_lock(0) != -3) return -1; //Double lock should happen fine

    //Test double unlock
    if(mutex_unlock(0) != 1) return -1;
    if(mutex_unlock(0) != -1) return -1;
    if(mutex_delete(0) != 1) return -1;
    printf(1, "PASSED\n");
    return 1;
}



int
mutex_test2(){
    printf(1, "module_3 part 2 test2 | cv_wait / signal additonal tests | ");

    int muxid = mutex_create("mutex0");
    if(muxid != 0) return -1;
    //Test cv_wait on mutex alrady held
    if(mutex_lock(0) != 1) return -1;
    if(cv_wait(0) != -3) return -1;
    
    //Call signal on lock. should return 0
    if(cv_signal(0) != 0) return -1;
    
    int pid = fork();
    if(pid == 0){
        cv_wait(0);
        sleep(10);
        cv_signal(0);
        exit();
    }else{
        sleep(10); //let child enter sleep before testing
        //call signal many times before waking up
        //First should work, then others will return 0
        if(cv_signal(0) != 1) return -1;
        if(cv_signal(0) != 0) return -1;
        if(cv_signal(0) != 0) return -1;
        if(mutex_unlock(0) != 1) return -1;
        printf(1, "unlocked mutex | ");
        //Try to wait multiple times
        //First should work, second
        if(cv_wait(0) != 1) return -1;
        if(cv_wait(0) != -3) return -1;
        if(cv_wait(0) != -3) return -1;
        wait();
        printf(1, "PASSED\n");
    }
    return 1;
}



int
main(void){
    if(mutex_test0() != 1) printf(1, "FAILED\n");
    if(mutex_test1() != 1) printf(1, "FAILED\n");
    if(mutex_test2() != 1) printf(1, "FAILED\n");
	exit();
}
