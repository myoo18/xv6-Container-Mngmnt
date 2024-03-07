#define QUEUESIZE 50
#define NULL ((void*)-1)
#define PRIO_MAX 10
#define HFS_PRIO_MAX 10 

// Per-CPU state
/*----------------------------SHARED MEM-----------------------------*/
#define MAX_SHM_PER_PROC 16
#define SHM_NAME_LEN 16
/*----------------------------SHARED MEM-----------------------------*/

struct cpu {
	uchar            apicid;     // Local APIC ID
	struct context * scheduler;  // swtch() here to enter scheduler
	struct taskstate ts;         // Used by x86 to find stack for interrupt
	struct segdesc   gdt[NSEGS]; // x86 global descriptor table
	volatile uint    started;    // Has the CPU started?
	int              ncli;       // Depth of pushcli nesting.
	int              intena;     // Were interrupts enabled before pushcli?
	struct proc *    proc;       // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int        ncpu;

// PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
	uint edi;
	uint esi;
	uint ebx;
	uint ebp;
	uint eip;
};

enum procstate
{
	UNUSED,
	EMBRYO,
	SLEEPING,
	RUNNABLE,
	RUNNING,
	ZOMBIE
};
#define MUX_MAXNUM 16
#define MAX_SHM_PER_PROC 16
// Per-process state


/*----------------------------SHARED MEM-----------------------------*/

struct shm_mapping {
    char name[SHM_NAME_LEN];  // Shared memory segment identifier
    uint va;                 // Virtual address where the segment is mapped in this process
};

/*----------------------------SHARED MEM-----------------------------*/


struct proc {
	uint              sz;            // Size of process memory (bytes)
	pde_t *           pgdir;         // Page table
	char *            kstack;        // Bottom of kernel stack for this process
	enum procstate    state;         // Process state
	int               pid;           // Process ID
	struct proc *     parent;        // Parent process
	struct trapframe *tf;            // Trap frame for current syscall
	struct context *  context;       // swtch() here to run process
	void *            chan;          // If non-zero, sleeping on chan
	int               killed;        // If non-zero, have been killed
	struct file *     ofile[NOFILE]; // Open files
	struct inode *    cwd;           // Current directory
	char              name[16];      // Process name (debugging)
	int 			  maxproc;       //the maximum number of processes (including this process) that can exist in a process tree of descendents rooted at the current process
	int 			  maxprocset;    //flag to see if maxproc has been called by a proc or its paren
	struct inode *    newroot_inode;       //Inode of the new root directory
	int               in_newroot;	 //flag to see if the newroot has been set
	int 			  in_container;  //flag to see if proc is in a container already
	int               namespace;     //the namespace/container id for isolation
	
	//mod 3
	uint			proc_mutexes[MUX_MAXNUM]; //Array of mutexes held by proc per spec

/*----------------------------SHARED MEM-----------------------------*/
	struct shm_mapping shm[MAX_SHM_PER_PROC];   //ADD THIS FOR SHARED MEM
/*----------------------------SHARED MEM-----------------------------*/


	int 			  fp_priority; 
	int 			  hfs_priority;
	int 			  queueType;     // 0 indicates FP 1 indicates HFS
	// struct proc* 	  head;
	// struct proc*      tail;
	struct proc* 	  next; 
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

//proc.c 
int fp_enqueue(struct proc *p);
struct proc* fp_dequeue();

int hfs_enqueue(struct proc *p); 
struct proc* hfs_dequeue(); 
