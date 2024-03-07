#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
	return fork();
}

int
sys_exit(void)
{
	exit();
	return 0; // not reached
}

int
sys_wait(void)
{
	return wait();
}

int
sys_kill(void)
{
	int pid;

	if (argint(0, &pid) < 0) return -1;
	return kill(pid);
}

int
sys_getpid(void)
{
	return myproc()->pid;
}

int
sys_sbrk(void)
{
	int addr;
	int n;

	if (argint(0, &n) < 0) return -1;
	addr = myproc()->sz;
	if (growproc(n) < 0) return -1;
	return addr;
}

int
sys_sleep(void)
{
	int  n;
	uint ticks0;

	if (argint(0, &n) < 0) return -1;
	acquire(&tickslock);
	ticks0 = ticks;
	while (ticks - ticks0 < n) {
		if (myproc()->killed) {
			release(&tickslock);
			return -1;
		}
		sleep(&ticks, &tickslock);
	}
	release(&tickslock);
	return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
	uint xticks;

	acquire(&tickslock);
	xticks = ticks;
	release(&tickslock);
	return xticks;
}

int
sys_cm_maxproc(void) 
{
    int nproc;

    if (argint(0, &nproc) < 0)
        return -1;
    return do_maxproc(nproc);
}

int 
sys_cm_setroot(void) 
{
    char *path;
    int path_len;

    if (argstr(0, &path) < 0 || argint(1, &path_len) < 0) {
        return -1;
    }

    return do_setroot(path, path_len);
}

int 
sys_cm_create_and_enter(void)
{
    return do_ce();
}


int 
sys_test_newroot(void) {
    char *path;
    int path_len;
    struct proc *curproc = myproc();
    struct inode *path_inode;

    // Get args
    if (argstr(0, &path) < 0 || argint(1, &path_len) < 0)
        return -1;

    // Resolve path to its inode
    path_inode = resolve_path_to_inode(path);
	// cprintf("newroot inode in struct is %p\n", curproc->newroot_inode);
	// cprintf("path_node in testnewroot is %p\n", path_inode);
    if (path_inode == 0) {
        // Path resolution failed
        return -1;
    }

    // Check if the newroot inode matches the inode of the provided path
    if (curproc->newroot_inode != path_inode) {
        // The inodes do not match
        iput(path_inode); // Release the inode obtained from path
        return -1;
    }
    iput(path_inode); // Release the inode obtained from path

    // Check if in_newroot is set
    if (!curproc->in_newroot) {
        // in_newroot is not set
        return -1;
    }
    // newroot and in_newroot checks passed
    return 0;
}

int sys_test_dotdot(void) {
    struct proc *curproc = myproc();

    // Ensure that newroot_inode and cwd are set
    if(curproc->in_newroot && curproc->newroot_inode && curproc->cwd) {
        // Compare the inodes
        if(curproc->newroot_inode == curproc->cwd)
            return 0; // cwd is the same as newroot_inode
        else
            return -1; // cwd is different from newroot_inode
    }
    return -1; // newroot_inode or cwd not set
}
/*----------------------------SHARED MEM-----------------------------*/

// chat, i think i need this.

//extern char* shm_get(char*);
int sys_shm_get(void) {
    char* name;

    if (argstr(0, &name) < 0) {
        return -1; // Failed to fetch the name argument
    }
    return (int)shm_get(name); // Casting to int as syscalls typically return int
}

//extern int shm_rem(char*);
int sys_shm_rem(void) {
    char* name;

    if (argstr(0, &name) < 0) {
        return -1; // Failed to fetch the name argument
    }
    return shm_rem(name);
}

/*----------------------------SHARED MEM-----------------------------*/

int
sys_mutex_init(void){
	mutex_init();
	return 0;
}

int 
sys_mutex_create(void){
	char *name;
	if (argptr(0, (void*)&name, sizeof(*name) < 0)) return -1;
	return mutex_create(name);
}

int
sys_mutex_delete(void){
	int muxid;
	if (argint(0, &muxid) < 0)  return -1;
	return mutex_delete(muxid);
}

int
sys_mutex_lock(void){
	int muxid;
	if (argint(0, &muxid) < 0)  return -1;
	return mutex_lock(muxid);
}

int
sys_mutex_unlock(void){
	int muxid;
	if (argint(0, &muxid) < 0)  return -1;
	return mutex_unlock(muxid);
}

int
sys_cv_wait(void){
	int muxid;
	if (argint(0, &muxid) < 0)  return -1;
	return cv_wait(muxid);
}

int
sys_cv_signal(void){
	int muxid;
	if (argint(0, &muxid) < 0)  return -1;
	return cv_signal(muxid);
}

int 
sys_prio_set(void)
{
    int pid, priority;
    if (argint(0, &pid) < 0 || argint(1, &priority) < 0){
        return -1;
	}
	return prio_set(pid, priority);
}

