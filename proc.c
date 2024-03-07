#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "shm.h"

#define FP_POLICY 0 
#define HFS_POLICY 1 

static struct proc *initproc;

int         nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct {
	struct proc *pqueue[QUEUESIZE + 1]; 
	struct proc *tails[QUEUESIZE + 1]; 

} pqueue_1;

struct {
	struct spinlock lock;
	struct proc     proc[NPROC];

	int htindex[PRIO_MAX][2];
	struct proc *pqueues[PRIO_MAX][QUEUESIZE];
} ptable;

void
pinit(void)
{
	initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid()
{
	return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being rescheduled
// between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
	int apicid, i;

	if (readeflags() & FL_IF) panic("mycpu called with interrupts enabled\n");

	apicid = lapicid();
	// APIC IDs are not guaranteed to be contiguous. Maybe we should have
	// a reverse map, or reserve a register to store &cpus[i].
	for (i = 0; i < ncpu; ++i) {
		if (cpus[i].apicid == apicid) return &cpus[i];
	}
	panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
	struct cpu * c;
	struct proc *p;
	pushcli();
	c = mycpu();
	p = c->proc;
	popcli();
	return p;
}

// PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void)
{
	struct proc *p;
	char *       sp;

	acquire(&ptable.lock);

	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if (p->state == UNUSED) goto found;

	release(&ptable.lock);
	return 0;

found:
	p->state = EMBRYO;
	p->pid   = nextpid++;

	release(&ptable.lock);

	// Allocate kernel stack.
	if ((p->kstack = kalloc()) == 0) {
		p->state = UNUSED;
		return 0;
	}
	sp = p->kstack + KSTACKSIZE;

	// Leave room for trap frame.
	sp -= sizeof *p->tf;
	p->tf = (struct trapframe *)sp;

	// Set up new context to start executing at forkret,
	// which returns to trapret.
	sp -= 4;
	*(uint *)sp = (uint)trapret;

	sp -= sizeof *p->context;
	p->context = (struct context *)sp;
	memset(p->context, 0, sizeof *p->context);
	p->context->eip = (uint)forkret;

	p->maxproc = 0;           // Initialize maxproc
    p->maxprocset = 0;        // Initialize maxprocset
    p->newroot_inode = 0;     // Initialize newroot_inode to null
    p->in_newroot = 0;        // Initialize in_newroot to false
    p->in_container = 0;      // Initialize in_container to false
	p->namespace = 0;      // Initialize in_container to false

/*-----------------------------------------SHARED MEM-----------------------------------------*/
	memset(p->shm,sizeof(struct shm_mapping)*SHM_MAXNUM,0);
/*-----------------------------------------SHARED MEM-----------------------------------------*/
	return p;
}

// PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
	struct proc *p;
	extern char  _binary_initcode_start[], _binary_initcode_size[];

	p = allocproc();

	initproc = p;
	if ((p->pgdir = setupkvm()) == 0) panic("userinit: out of memory?");
	inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
	p->sz = PGSIZE;
	memset(p->tf, 0, sizeof(*p->tf));
	p->tf->cs     = (SEG_UCODE << 3) | DPL_USER;
	p->tf->ds     = (SEG_UDATA << 3) | DPL_USER;
	p->tf->es     = p->tf->ds;
	p->tf->ss     = p->tf->ds;
	p->tf->eflags = FL_IF;
	p->tf->esp    = PGSIZE;
	p->tf->eip    = 0; // beginning of initcode.S

	safestrcpy(p->name, "initcode", sizeof(p->name));
	p->cwd = namei("/");

	// this assignment to p->state lets other cores
	// run this process. the acquire forces the above
	// writes to be visible, and the lock is also needed
	// because the assignment might not be atomic.
	acquire(&ptable.lock);
	
	p -> queueType = HFS_POLICY; 
	p-> hfs_priority = 0; 
    hfs_enqueue(p); 

	p->state = RUNNABLE;

	release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
	uint         sz;
	struct proc *curproc = myproc();

	sz = curproc->sz;
	if (n > 0) {
		if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0) return -1;
	} else if (n < 0) {
		if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0) return -1;
	}
	curproc->sz = sz;
	switchuvm(curproc);
	return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
	int          i, pid;
	struct proc *np;
	struct proc *curproc = myproc();

	// Allocate process.
	if ((np = allocproc()) == 0) {
		return -1;
	}
	//carry over maxproc to new proc
	np->maxprocset = curproc->maxprocset;
	np->maxproc = curproc->maxproc;
	np->in_newroot = curproc->in_newroot;
	np->in_container = curproc->in_container;
	np->namespace = curproc->namespace;

	if (curproc->in_newroot) {
		np->newroot_inode = idup(curproc->newroot_inode);  //duplicate inode reference
	} else {
		np->newroot_inode = 0;  //set to null
	}

	// Copy process state from proc.
	if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
		//cprintf("MONKEY MONKEY MONEY\n");
		kfree(np->kstack);
		np->kstack = 0;
		np->state  = UNUSED;
		return -1;
	}
	np->sz     = curproc->sz;
	np->parent = curproc;
	*np->tf    = *curproc->tf;

/*-----------------------------------------SHARED MEM-----------------------------------------*/
	memmove(np->shm, curproc->shm, sizeof(struct shm_mapping)*SHM_MAXNUM);
/*-----------------------------------------SHARED MEM-----------------------------------------*/

	// Clear %eax so that fork returns 0 in the child.
	np->tf->eax = 0;

	for (i = 0; i < NOFILE; i++)
		if (curproc->ofile[i]) np->ofile[i] = filedup(curproc->ofile[i]);
	np->cwd = idup(curproc->cwd);

	safestrcpy(np->name, curproc->name, sizeof(curproc->name));

	pid = np->pid;

	acquire(&ptable.lock);

	np->state = RUNNABLE;

	if (np -> parent -> queueType == FP_POLICY){
		np -> fp_priority = np -> parent -> fp_priority; 
		np -> queueType = FP_POLICY; 
        fp_enqueue(np);
	}
    else if (np -> parent -> queueType == HFS_POLICY){
		np -> hfs_priority = np -> parent -> hfs_priority; 
		np -> queueType = HFS_POLICY; 
        hfs_enqueue(np);
	}
	
	release(&ptable.lock);

	return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
	struct proc *curproc = myproc();
	struct proc *p;
	int          fd;

	if (curproc == initproc) panic("init exiting");

	// Close all open files.
	for (fd = 0; fd < NOFILE; fd++) {
		if (curproc->ofile[fd]) {
			fileclose(curproc->ofile[fd]);
			curproc->ofile[fd] = 0;
		}
	}
/*-----------------------------------------SHARED MEM-----------------------------------------*/
	for(int i = 0; i < SHM_MAXNUM; i++){
		if(curproc->shm[i].va != (uint)0){
			//call rem to remove all the allocations and references
			shm_rem(curproc->shm[i].name);
		}
	}
	memset(curproc->shm,sizeof(struct shm_mapping)*SHM_MAXNUM, 0);
	//now global struct should be empty too. pog champ
/*-----------------------------------------SHARED MEM-----------------------------------------*/
	begin_op();
	iput(curproc->cwd);
	end_op();
	curproc->cwd = 0;

	acquire(&ptable.lock);

	// Parent might be sleeping in wait().
	wakeup1(curproc->parent);

	// Pass abandoned children to init.
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if (p->parent == curproc) {
			p->parent = initproc;
			if (p->state == ZOMBIE) wakeup1(initproc);
		}
	}

	// Jump into the scheduler, never to return.
	curproc->state = ZOMBIE;
	sched();
	panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
	struct proc *p;
	int          havekids, pid;
	struct proc *curproc = myproc();

	acquire(&ptable.lock);
	for (;;) {
		// Scan through table looking for exited children.
		havekids = 0;
		for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
			if (p->parent != curproc) continue;
			havekids = 1;
			if (p->state == ZOMBIE) {
				// Found one.
				pid = p->pid;
				kfree(p->kstack);
				p->kstack = 0;
				freevm(p->pgdir);
				p->pid     = 0;
				p->parent  = 0;
				p->name[0] = 0;
				p->killed  = 0;
				p->state   = UNUSED;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if (!havekids || curproc->killed) {
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(curproc, &ptable.lock); // DOC: wait-sleep
	}
}

// PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
	struct proc *p;
	struct cpu * c = mycpu();
	c->proc        = 0;

	for (;;) {
		// Enable interrupts on this processor.
		sti();

		acquire(&ptable.lock);

		p = fp_dequeue();
		
		if (p != NULL && p->state == RUNNABLE) {
			c->proc = p;
			switchuvm(p);
			p->state = RUNNING;

			swtch(&(c->scheduler), p->context);
			switchkvm();

			c->proc = 0;
		}

		// p = hfs_dequeue(); 

		// if(p != NULL){
		// 	if (p->state != RUNNABLE){
		// 		continue;
		// 	}
		// 	c->proc = p;
		// 	switchuvm(p);
		// 	p->state = RUNNING;

		// 	swtch(&(c->scheduler), p->context);
		// 	switchkvm();
		// 	c -> proc = 0;
		// }
		
		for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
			if (p->state != RUNNABLE) continue;

			// Switch to chosen process.  It is the process's job
			// to release ptable.lock and then reacquire it
			// before jumping back to us.
			c->proc = p;
			switchuvm(p);
			p->state = RUNNING;

			swtch(&(c->scheduler), p->context);
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->proc = 0;
		}
		release(&ptable.lock);
	}
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
	int          intena;
	struct proc *p = myproc();

	if (!holding(&ptable.lock)) panic("sched ptable.lock");
	if (mycpu()->ncli != 1) panic("sched locks");
	if (p->state == RUNNING) panic("sched running");
	if (readeflags() & FL_IF) panic("sched interruptible");
	intena = mycpu()->intena;
	swtch(&p->context, mycpu()->scheduler);
	mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
	acquire(&ptable.lock); // DOC: yieldlock
	myproc()->state = RUNNABLE;
	if(myproc() -> queueType == 1 && myproc() -> hfs_priority > PRIO_MAX){
		myproc() -> hfs_priority++; 
		hfs_enqueue(myproc()); 
	}
	else if(myproc() -> queueType == 0){
		fp_enqueue(myproc());
	}
	sched();
	release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
	static int first = 1;
	// Still holding ptable.lock from scheduler.
	release(&ptable.lock);

	if (first) {
		// Some initialization functions must be run in the context
		// of a regular process (e.g., they call sleep), and thus cannot
		// be run from main().
		first = 0;
		iinit(ROOTDEV);
		initlog(ROOTDEV);
	}

	// Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
	struct proc *p = myproc();

	if (p == 0) panic("sleep");

	if (lk == 0) panic("sleep without lk");

	// Must acquire ptable.lock in order to
	// change p->state and then call sched.
	// Once we hold ptable.lock, we can be
	// guaranteed that we won't miss any wakeup
	// (wakeup runs with ptable.lock locked),
	// so it's okay to release lk.
	if (lk != &ptable.lock) {      // DOC: sleeplock0
		acquire(&ptable.lock); // DOC: sleeplock1
		release(lk);
	}
	// Go to sleep.
	p->chan  = chan;
	p->state = SLEEPING;

	sched();

	// Tidy up.
	p->chan = 0;

	// Reacquire original lock.
	if (lk != &ptable.lock) { // DOC: sleeplock2
		release(&ptable.lock);
		acquire(lk);
	}
}

// PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
	struct proc *p;

	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if (p->state == SLEEPING && p->chan == chan){
			p->state = RUNNABLE;
		}
		if (p->queueType == FP_POLICY){
            fp_enqueue(p);
		}
        else if (p->queueType == HFS_POLICY){
			p -> hfs_priority = 0; 
            hfs_enqueue(p);
		}
	}
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
	acquire(&ptable.lock);
	wakeup1(chan);
	release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
	struct proc *p;

	acquire(&ptable.lock);
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if (p->pid == pid) {
			p->killed = 1;
			// Wake process from sleep if necessary.
			if (p->state == SLEEPING) p->state = RUNNABLE;

			if (p->queueType == FP_POLICY){
            	fp_enqueue(p);
			}
        	else if (p->queueType == HFS_POLICY){
				p -> hfs_priority = 0; 
				hfs_enqueue(p);
			}

			release(&ptable.lock);
			return 0;
		}
	}
	release(&ptable.lock);
	return -1;
}

// PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
	static char *states[] = {[UNUSED] "unused",   [EMBRYO] "embryo",  [SLEEPING] "sleep ",
	                         [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
	int          i;
	struct proc *p;
	char *       state;
	uint         pc[10];

	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if (p->state == UNUSED) continue;
		if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
			state = states[p->state];
		else
			state = "???";
		cprintf("%d %s %s", p->pid, state, p->name);
		if (p->state == SLEEPING) {
			getcallerpcs((uint *)p->context->ebp + 2, pc);
			for (i = 0; i < 10 && pc[i] != 0; i++) cprintf(" %p", pc[i]);
		}
		cprintf("\n");
	}
}


struct inode* 
resolve_path_to_inode(char *path) {
    struct inode *ip;

    begin_op();
    if ((ip = namei(path)) == 0) {
		//cprintf("ip after namei call in resolve path is: %p\n", ip);
        end_op();
        return 0; // Path does not exist or other error
    }
    end_op();
    return ip;
}

int
do_maxproc(int nproc) 
{
    struct proc *curproc = myproc();

    //check if maxproc has been set for the current process or any ancestor
    for (struct proc *p = curproc; p != 0; p = p->parent) {
        if (p->maxprocset) {
            return -1; //maxproc already been called in da lineage
        }
    }

    // // create and enter will set to 1 if in a container
	// if (curproc->in_container){
	// 	return -1;
	// }
    acquire(&ptable.lock);
    curproc->maxproc = nproc;
    curproc->maxprocset = 1;  //flag maxprocset so that we know maxproc has been called in the familly
    release(&ptable.lock);
	return 0;
}

int 
do_setroot(char *path, int path_len) 
{
    struct proc *curproc = myproc();
    struct inode *newroot_inode;

    if (curproc->in_newroot) {
        return -1; // Root already set
    }

    if (path[0] != '/' || path_len >= MAXPATH) {
        return -1; // Invalid path
    }

    newroot_inode = resolve_path_to_inode(path);
    if (newroot_inode == 0 || !is_inode_directory(newroot_inode)) {
        return -1; // Failed to resolve path
    }

	acquire(&ptable.lock);
    curproc->newroot_inode = newroot_inode;
    curproc->in_newroot = 1;
	release(&ptable.lock);
    return 0;
}

//start from 1 to avoid init of 0
static int namespace_id = 1;

int 
do_ce(void) 
{
    struct proc *p;
    struct proc *curproc = myproc();
    int found;

    if (curproc->in_container) {
        return -1; // container has already set
    }


    acquire(&ptable.lock);

    do {
        found = 1;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != UNUSED && p->namespace == namespace_id) {
                found = 0;
                break;
            }
        }
        if (!found) {
            namespace_id++;
        }
    } while (!found);

    curproc->in_container = 1;
    curproc->namespace = namespace_id++;

	// cprintf("Process %d is set to namespace %d\n", curproc->pid, curproc->namespace);

    release(&ptable.lock);

    return 0;
}


//MODULE 3
struct cv{ //Cond variable
    struct spinlock lock;
    //Inspired by the wseq (waiter sequence) idea in pthreads
    //Everytime a proc is blocked by a condditional variable it 
    //is added to the waiter array and then set to sleep
    //When it is time to signal, the next proc is woken up in FIFO order
    //This ensures some sort of bounded-weight and fairness 
    int waiters[MUX_MAXNUM]; //Array of waiters that will hold the pid
    int num_waiters;            //simply the number of waiters in array
};

struct mutex {
    uint muxid;             //mutex id
    char* name;             //mux name
    uint locked;            //is mutex locked by thread 0:false !0:true
    uint used;              //is it created 0:false !0:true 
    struct spinlock lock;   //similar to hw 4, lock for mutex
    struct cv cv;
    uint pid;               //pid holding mutex
    int nameSpace;
};

struct{
    struct mutex mutexes[MUX_MAXNUM]; //array of that holds mutexes
    struct spinlock lock;     //spinlock for table
}mutex_table;                 //Meant to be global mutex table init here



void
mutex_init(void){
    //lock for entire table
    initlock(&mutex_table.lock, "mux_table");

    //For every mutex in the table create its lock
    for(int i = 0 ; i < MUX_MAXNUM; i ++){
        mutex_table.mutexes[i].locked = 0;                  //0 not locked !0 is locked
        mutex_table.mutexes[i].used = 0;                    //Determines if mutex has been created
        mutex_table.mutexes[i].muxid = i;                   //id of mutex 0-MUX_MAXNUM(16)
        mutex_table.mutexes[i].pid = -1;                        //pid of proc holding mutex
        mutex_table.mutexes[i].nameSpace = 0;                   //nameSpace default to 0, meaning all procs can use it
        mutex_table.mutexes[i].cv.num_waiters = 0;          //number of procs in wait queue

        for(int j = 0; j < MUX_MAXNUM; j ++){
            mutex_table.mutexes[i].cv.waiters[j] = -1;      //Set wseq to -1, empty
        }
        initlock(&mutex_table.mutexes[i].lock, "lock");
        initlock(&mutex_table.mutexes[i].cv.lock, "cv");
    }
}


int 
mutex_create(char *name){
    acquire(&mutex_table.lock);
    for(int i = 0; i < MUX_MAXNUM ; i++){
        if(!mutex_table.mutexes[i].used){
            mutex_table.mutexes[i].used = 1;
            strncpy(mutex_table.mutexes[i].name, name, sizeof(name));
            mutex_table.mutexes[i].nameSpace = myproc()->namespace; 
            
            release(&mutex_table.lock);
            return i; // return muxid
        }
    }
    release(&mutex_table.lock);
    return -1; //all mutexes are being used
}

int
mutex_delete(int muxid){
    if(muxid > MUX_MAXNUM || muxid < 0){
        return -2; //muxid higher than max num of mutexes
    }
    acquire(&mutex_table.lock);
    struct mutex *mutex = &mutex_table.mutexes[muxid];

    if(mutex->used == 0){
        release(&mutex_table.lock);
        return -1;
    }
    if(mutex->locked){
        release(&mutex_table.lock);
        return -3;
    }
    if(mutex->cv.num_waiters > 0) return -4; //procs in wait sequeunce, do not delete
    
    
    mutex_table.mutexes[muxid].locked = 0;
    mutex_table.mutexes[muxid].used = 0;
    release(&mutex_table.lock);
    return 1;
}


int
mutex_lock(int muxid){
    
    if(muxid < 0 || muxid > MUX_MAXNUM){
        //invalid muxid
        return -1;
    }

    struct mutex *mutex = &mutex_table.mutexes[muxid];
    struct proc *currproc = myproc();
    if(!mutex->used){ //if mutex not in used state return -1
        return -1;
    }

    //Name space not the same, return -2
    if(mutex->nameSpace != currproc->namespace) return -2;
    //Trying to lock mutex twice, return -3
    if(mutex->pid == currproc->pid) return -3;

    acquire(&mutex->lock);

    while(mutex->locked != 0){ //Sleep till available
        sleep(mutex , &mutex->lock);
    }

    //After geting the lock set lock to 1
    mutex->locked = 1;

    //Set mutex to proc_mutex array in designated proc
    //Neede for trap instances
    int pid = currproc->pid;
    mutex->pid = pid;
    for(int i = 0; i < MUX_MAXNUM; i++){
        if(currproc->proc_mutexes[i] == -1){
            currproc->proc_mutexes[i] = muxid;
            break;
        }
    }
    
    release(&mutex->lock);
    return 1;
}

int
mutex_unlock(int muxid){
    if(muxid < 0 || muxid > MUX_MAXNUM){ //invalid muxid
        return -1;
    }   
    struct mutex *mutex = &mutex_table.mutexes[muxid];
    acquire(&mutex->lock);
    if (!mutex->used) {
        release(&mutex->lock);
        return -1;
    }

    //Lvl 3 proc mutexes
    struct proc *currproc = myproc();

    //pid does not equal pid of mutex, also protects against double release
    if(mutex->pid != myproc()->pid){
        release(&mutex->lock);
        return -1;
    }
    for(int i = 0; i < MUX_MAXNUM; i++){
        if(currproc->proc_mutexes[i] == muxid){
            currproc->proc_mutexes[i] = -1; //0 would be a mutex so set to -1
            break;
        }
    }
    
    mutex->locked = 0;
    mutex->pid = -1;
    release(&mutex->lock);
    wakeup(mutex);
    return 1;
}


//Add proc to wseq and set to sleep
int
cv_wait(int muxid){
    if(muxid < 0 || muxid > MUX_MAXNUM){ //invalid muxid
        return -1;
    }   
    struct proc *currproc = myproc();
    struct mutex *mutex = &mutex_table.mutexes[muxid];
    int pid = currproc->pid;
    acquire(&mutex->lock);
    acquire(&ptable.lock);          // to set sleep state
    


    if (!mutex->used) { //Attemped to unlock an unused mutex
        release(&ptable.lock);
        release(&mutex->lock);
        return -1;
    }
    //Cv_wait should not be able to be called on mutex that it has lock on
    if(mutex->pid == pid){
        release(&ptable.lock);
        release(&mutex->lock);
        return -3; 
    }

   //Add waiter to wseq, check if proc is already in wseq
    //If already in wseq then dont add and return
    if(mutex->cv.num_waiters == MUX_MAXNUM) return -1;
    mutex->cv.waiters[mutex->cv.num_waiters++] = pid; //Add pid to waiter arr and increment

	currproc->state = SLEEPING;     // Set proc to sleep sleep()

    release(&mutex->lock);  

    sched(); //Say we are schleepy and can now sched()

    acquire(&mutex->lock);
    mutex->locked = 1;

    //Set mutex to proc_mutex array in designated proc
    //Neede for trap instances
    mutex->pid = pid;
    for(int i = 0; i < MUX_MAXNUM; i++){
        if(currproc->proc_mutexes[i] == -1){
            currproc->proc_mutexes[i] = muxid;
            break;
        }
    }

    release(&mutex->lock);
    release(&ptable.lock);  //Must release after calling scheds otherwise ptable.lock panic
    return 1;
}

int
cv_signal(int muxid){
    if(muxid < 0 || muxid > MUX_MAXNUM){ //invalid mutex
        return -1;
    }   

    acquire(&mutex_table.mutexes[muxid].cv.lock);
    acquire(&ptable.lock); 
    struct mutex *mutex = &mutex_table.mutexes[muxid];

    //No waiters to wakeup 
    if(mutex->cv.num_waiters==0){
        release(&ptable.lock);
        release(&mutex_table.mutexes[muxid].cv.lock);
        return 0;
    }


    //Get pid of next in seq using wseq logic
    int wseq_wakeup = mutex->cv.waiters[0]; 
    //Shift wait queue down to maintain order
    for(int i = 0; i < mutex->cv.num_waiters-1 ; i++){
        mutex->cv.waiters[i] = mutex->cv.waiters[i+1]; //Test 4 OutOfBounds error
    }

    //Deincrement num waiters for mutex
    mutex->cv.num_waiters--;  

    struct proc *procproc; 
    //Wakey Wakey
    //Go through ptable and look for proc with same pid ad wseq and for safety
    //Also check that it is sleeping, loop needed because ptable isnt indexed >:(
    for(procproc = ptable.proc; procproc < &ptable.proc[NPROC]; procproc++){
        if(procproc->pid == wseq_wakeup && procproc->state==SLEEPING){
            procproc->state=RUNNABLE;
			//inssert FPenqueue
			fp_enqueue(procproc); 
            break;
        }
    }

    release(&ptable.lock);
    release(&mutex_table.mutexes[muxid].cv.lock);

    //No sched necessary
    return 1;
}

int
wseq_remove(int muxid, int pid){
    acquire(&mutex_table.mutexes[muxid].cv.lock);
    acquire(&ptable.lock); 

    struct mutex *mutex = &mutex_table.mutexes[muxid];

    if(mutex->cv.num_waiters==0){
        release(&ptable.lock);
        release(&mutex_table.mutexes[muxid].cv.lock);
        return 0;
    }
    int i;
    for(i = 0 ; i < mutex->cv.num_waiters; i++){
        if(mutex->cv.waiters[i] == pid){
            for(int j = 0; j < mutex->cv.num_waiters-i-1 ; j++){
                mutex->cv.waiters[j] = mutex->cv.waiters[j+1]; 
                mutex->cv.num_waiters--;
            }
        }
    }

    release(&ptable.lock);
    release(&mutex_table.mutexes[muxid].cv.lock);
    return 1;
}



int 
prio_set(int pid, int priority){

	if (priority > PRIO_MAX || priority < 0){
		return -1;
	}

	acquire(&ptable.lock);

	struct proc* currproc;
	currproc = myproc();

	currproc -> queueType = FP_POLICY; 

	if (currproc->fp_priority > priority)
	{
		release(&ptable.lock);
		return -1;
	}

	if (currproc->pid == pid){
		currproc->fp_priority = priority;
	}
	else
	{
		uint i;
		for (i = 0; i < NPROC; i++)
		{
			if (ptable.proc[i].pid == pid)
			{
				if ((ptable.proc[i].parent)->pid == currproc->pid)
				{
					ptable.proc[i].fp_priority = priority;
					break;
				}
				else
				{
					release(&ptable.lock);
					return -1;
				}
			}
		}
	}

	release(&ptable.lock);
	return 0;
}

int
fp_enqueue (struct proc* p){
	if(!(p -> state == RUNNABLE)){
        return -1; 
    }

    int priority = p -> fp_priority; 
    int head = ptable.htindex[priority][0]; 
    int tail = ptable.htindex[priority][1]; 

    if(tail == (head-1)%QUEUESIZE){
        return -1; 
    }

    ptable.pqueues[priority][tail] = p; 
    ptable.htindex[priority][1] = (tail+1)%QUEUESIZE; 
    return 1; 
}

struct proc* 
fp_dequeue(){
	int priority = 0; 
    while(priority < PRIO_MAX && ptable.htindex[priority][0] == ptable.htindex[priority][1]){
        priority++; 
    }

    if(priority >= PRIO_MAX){
        return NULL; 
    }

    int head = ptable.htindex[priority][0]; 
    struct proc *p = ptable.pqueues[priority][head]; 

    ptable.htindex[priority][0] = (head+1)%QUEUESIZE; 
    return p; 
}

int
hfs_enqueue (struct proc* p){
	if (!(p->state == RUNNABLE)){
		return -1;
	}

	if(p -> hfs_priority > HFS_PRIO_MAX){
		p -> hfs_priority = HFS_PRIO_MAX; 
	}

	int priority = p -> hfs_priority; 
	int head = ptable.htindex[priority][0]; 
    int tail = ptable.htindex[priority][1]; 

    if(tail == (head-1)%QUEUESIZE){
        return -1; 
    }

    ptable.pqueues[priority][tail] = p; 
    ptable.htindex[priority][1] = (tail+1)%QUEUESIZE; 
    return 1; 
}

struct proc* 
hfs_dequeue(){
	int priority = 0; 
    while(priority < PRIO_MAX && ptable.htindex[priority][0] == ptable.htindex[priority][1]){
        priority++; 
    }

    if(priority >= PRIO_MAX){
        return NULL; 
    }

    int head = ptable.htindex[priority][0]; 
    struct proc *p = ptable.pqueues[priority][head]; 

    ptable.htindex[priority][0] = (head+1)%QUEUESIZE; 
    return p; 
}