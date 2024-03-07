#define MUX_MAXNUM 16


/*
struct mutex {
	uint muxid;				//mutex id
	char* name;             //mux name
	uint locked; 			//is mutex locked by thread 0:false !0:true
	struct spinlock lock; 	//similar to hw 4, lock for mutex
};

struct{
	struct mutex mutexes[MUX_MAXNUM]; //array of that holds mutexes
	struct spinlock lock;     //spinlock for table
}mutex_table;                 //Meant to be global mutex table init here
*/

/*
Helper function so that when a pid is entered it will be removed form a specified muxid
Note this is a kernal mode function user cannot call it
*/
int wseq_remove(int muxid, int pid);

/*
Initialize the mutex table, not in original specs
Similar to lock_init implemented in hw4
*/
void mutex_init(void);

/*
return a muxid which is an integer identifier for a
mutex (analogous to a file descriptor which is an integer that identifies a file), or return -1
if it cannot allocate the mutex.
*/
int mutex_create(char *name);

/*
deallocate the mutex if we are the last process
referencing (i.e. all other processes that previously mutex_created it have
mutex_deleted it, or have exited/execed).
*/
int mutex_delete(int muxid);


/*
Lock a mutex identified by the mutex id. If the mutex
is not owned by another thread, then the calling thread now owns it, and has mutual
exclusion until the mutex is unlocked. If the mutex is already owned, then we must
block, awaiting the other process to release the mutex.
*/
int mutex_lock(int muxid);

/*
Unlock a mutex that was previously locked by us.
If other threads have since tried to take the mutex, and is blocked awaiting entry, we
should wake one of them up to go into the critical section.
*/
int mutex_unlock(int muxid);


/*
The condition variable API is vastly simplified compared
to the pthread version. We assume that each mutex can have a single condition
associated with it. This works well if we just want the CM waiting on the condition that a
client sends a request, and that clients will block waiting on the condition that the CM
replies to them. Because of this, we need only specify the mutex id that the condition
variable is associated with, and not which condition variable to use.
*/
int cv_wait(int muxid);

/*
wake up any threads that are blocked on the condition
variable associated with the mutex identified by its mutex id. This doesn’t alter the
mutex at all, and only wakes up any blocked threads. You can implement this such that
it wakes all threads, or just one.
*/
int cv_signal(int muxid);