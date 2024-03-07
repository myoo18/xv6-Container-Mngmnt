# Project Report

Team Members:
 - Michael
 - Ivan
 - Josh
 - Jordan



## Module #1: Container Manager
Lead: Michael
 
 - [x] Level 0
 - [x] Level 1
 - [x] Level 2
 - [x] Level 3
 - [ ] Level 4
 - [ ] Extra Credit

## Module #2: Shared Memory
Lead: Ivan
 
 - [ x] Level 0
 - [ x] Level 1
 - [ x] Level 2
 - [ x] Level 3
 - [ ] Extra Credit

## Module #3: Synchronization
Lead:Jordan
 
 - [X] Level 0
 - [X] Level 1
 - [X] Level 2
 - [X] Level 3
 - [ ] Level 4
 - [ ] Extra Credit

## Module #4: Scheduling
 
 - [Z] Level 1
 - [Z] Level 2
 - [ ] Level 3
 - [ ] Level 4
 - [ ] Level 5
 - [ ] Level 6
       
## Overall Project
 
 - [x ] Level 0: Level 1 in four modules.
 - [ x] Level 1: Level 2 in four modules.
 - [x ] Level 2: Level 3 in four modules.  Two of the modules must be integrated together.  You must have tests to evaluate this integration.
 - [ ] Level 3: All four modules must be integrated together.  You must have tests to evaluate this integration.
 - [ ] Level 4: Highest level minus 1 in two modules, and highest level in two.
 - [ ] Level Super Saiyan: Highest level in all modules.

# Level Testing Documentation
## MOD 3

make qemu-nox
./mutextests
//Goes through mutliple mutex tests that tests mutex_create(), mutex_delete(), mutex_lock(), mutex_unlock(), cv_wait(), cv_signal(), and tests integration with module 2 during test 2. The test will end in a trap error as it intentionally seg faults in order to test level 3 of module 3. In order to continue testing run :
./mutextests1


## Implementation
The mutex table contains an array of mutexes that are init in mutex_init(), each mutex has an condition variable and lock. The cv uses a wseq to order the use of procs


Module 1: container manager testing

make qemu-nox

level 0: you can test level 0 (parsing) by just running cmlvl0 in the terminal. this just test if the parser is working.
level 1: you can test level 1 by running cmlvl1 in the terminal. This tests the basic functionality of the setroot function.
level 2: you can test level 2 by running cmlvl2 in the terminal. his tests the basic functionality of the maxproc function.
level 3: did not integrate with other modules... run dockv6 /cmspec.json or any other path to a json file. then run "cm"


## Module #2: Shared Memory Testing instructions
make qemu-nox
./shmTest
| nothing shound print of all is doing as expected.
# how i tested it
i tested by get shm a region. then i ctring copy then i try compare values to make sure fork workeed. the exit. I remove shm and then get again to see if properly deallocate. then check for invalid inputs for name. 

## Module #2: Implementation
The strucutre for shared memeroy is in shm.h Here we have a strucutre to store all shared memory instances and i track the number the shared mem instances. For each shared mem instance i store the name, references, physical address, and for integration with locks (shm_mutex_index). I also have a add field to proc struct in proc.h where i store shm mappings. it also stores the name and the virtual address.

# shm_get
here i took a lot of code from allocuvm to make shared memory allocated. before we allocate shared memory instance, i look through the shared mem struct for references and if there is any above 0 i wil check the name and if the name is correct ill update the mem var to the PA of the existing shm isntance. then increase counter. if it doesnt ill go ahead an allocate an instance. this includes updating the page tables , increaseing curproc sz and switchuvm(i took these from allocuvm). after all this i return the address



# shm_rem
i first find the current process's va. i check if its found or not. if it is foudn i remove all the PTE. also stole from deallocuvm.  then i clear mem pages entries. update the curproc size and then update my memory table for shm struct.






## Module #4: Scheduling

lvl0test: 
Just tests that prioset can succesfully assign a priority to a process. 

lvl1test: 
Using the prioset system call, the priority of a pid process is trying to be raised but will not be able to and should return a fail statement. 

lvl2test: 
In level 2 test, in my first test, I test that prioset is able to set priorities and unable to raise a priority of three forked process. 
In my second test, I fork three processes, put them to sleep and then have them print out a statement. The prio set function sets their priorities and when printing, the processes should print out in the order of their priority
In my third test, I test for round robin by setting all of their priorities to be the same in which they should all equally use CPU in order. 
In my fourth test, I test that a processes priority can be updates to a lower priority by reassigning using prioset. It passes when that priority is successfully updated.  

I worked on the prioset system call which sets the priority of a process on the FP_Queue and only succeeds if the pid is either the same or exists within the same ancestry of the process. The function returns 0 on success and returns -1 on failure. 
I had specific queueing functions (enqueue, dequeue) for both the FP_queue and the HFS_queue. 
FP_Enqueue uses an O(1) priority queue that utilizes the ptable and adds the new process to the tail of the queue. 
FP_Dequeue finds the process with the highest priority that is in the RUNNABLE state and dequeues it from the queue. 
HFS_Enqueue uses a similar queue scheduling process but for HFS Scheduling demotion is enforced in trap.c where if a tick occurs at each esxecution, the priority of the process on the HFS Queue is demoted. 
HFS_Dequeue similarly finds the process and dequeues the highest priority process.

Initially when the first process is initialized, HFS queue should be the default but when a process is forked, then the parent of the new process is examined and if the parent is on the FP_queue then the child follows the queue type as well as the priority of its parent. 