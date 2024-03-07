#include "shm.h"
#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

/*-----------------------------------------SHARED MEM-----------------------------------------*/
uint is_shm(uint pa);
int shm_index(uint pa);
/*-----------------------------------------SHARED MEM-----------------------------------------*/



extern char data[]; // defined by kernel.ld
pde_t *     kpgdir; // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
	struct cpu *c;

	// Map "logical" addresses to virtual addresses using identity map.
	// Cannot share a CODE descriptor for both kernel and user
	// because it would have to have DPL_USR, but the CPU forbids
	// an interrupt from CPL=0 to DPL=3.
	c                 = &cpus[cpuid()];
	c->gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, 0);
	c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
	c->gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
	c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
	lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
	pde_t *pde;
	pte_t *pgtab;

	pde = &pgdir[PDX(va)];
	if (*pde & PTE_P) {
		pgtab = (pte_t *)P2V(PTE_ADDR(*pde));
	} else {
		if (!alloc || (pgtab = (pte_t *)kalloc()) == 0) return 0;
		// Make sure all those PTE_P bits are zero.
		memset(pgtab, 0, PGSIZE);
		// The permissions here are overly generous, but they can
		// be further restricted by the permissions in the page table
		// entries, if necessary.
		*pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
	}
	return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.



static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
	char * a, *last;
	pte_t *pte;

	a    = (char *)PGROUNDDOWN((uint)va);
	last = (char *)PGROUNDDOWN(((uint)va) + size - 1);
	for (;;) {
		if ((pte = walkpgdir(pgdir, a, 1)) == 0) return -1;
		if (*pte & PTE_P) panic("remap");
		*pte = pa | perm | PTE_P;
		if (a == last) break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
	void *virt;
	uint  phys_start;
	uint  phys_end;
	int   perm;
} kmap[] = {
  {(void *)KERNBASE, 0, EXTMEM, PTE_W},            // I/O space
  {(void *)KERNLINK, V2P(KERNLINK), V2P(data), 0}, // kern text+rodata
  {(void *)data, V2P(data), PHYSTOP, PTE_W},       // kern data+memory
  {(void *)DEVSPACE, DEVSPACE, 0, PTE_W},          // more devices
};

// Set up kernel part of a page table.
pde_t *
setupkvm(void)
{
	pde_t *      pgdir;
	struct kmap *k;

	if ((pgdir = (pde_t *)kalloc()) == 0) return 0;
	memset(pgdir, 0, PGSIZE);
	if (P2V(PHYSTOP) > (void *)DEVSPACE) panic("PHYSTOP too high");
	for (k = kmap; k < &kmap[NELEM(kmap)]; k++)
		if (mappages(pgdir, k->virt, k->phys_end - k->phys_start, (uint)k->phys_start, k->perm) < 0) {
			freevm(pgdir);
			return 0;
		}
	return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
	kpgdir = setupkvm();
	switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
	lcr3(V2P(kpgdir)); // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
	if (p == 0) panic("switchuvm: no process");
	if (p->kstack == 0) panic("switchuvm: no kstack");
	if (p->pgdir == 0) panic("switchuvm: no pgdir");

	pushcli();
	mycpu()->gdt[SEG_TSS]   = SEG16(STS_T32A, &mycpu()->ts, sizeof(mycpu()->ts) - 1, 0);
	mycpu()->gdt[SEG_TSS].s = 0;
	mycpu()->ts.ss0         = SEG_KDATA << 3;
	mycpu()->ts.esp0        = (uint)p->kstack + KSTACKSIZE;
	// setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
	// forbids I/O instructions (e.g., inb and outb) from user space
	mycpu()->ts.iomb = (ushort)0xFFFF;
	ltr(SEG_TSS << 3);
	lcr3(V2P(p->pgdir)); // switch to process's address space
	popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
	char *mem;

	if (sz >= PGSIZE) panic("inituvm: more than a page");
	mem = kalloc();
	memset(mem, 0, PGSIZE);
	mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W | PTE_U);
	memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
	uint   i, pa, n;
	pte_t *pte;

	if ((uint)addr % PGSIZE != 0) panic("loaduvm: addr must be page aligned");
	for (i = 0; i < sz; i += PGSIZE) {
		if ((pte = walkpgdir(pgdir, addr + i, 0)) == 0) panic("loaduvm: address should exist");
		pa = PTE_ADDR(*pte);
		if (sz - i < PGSIZE)
			n = sz - i;
		else
			n = PGSIZE;
		if (readi(ip, P2V(pa), offset + i, n) != n) return -1;
	}
	return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
	char *mem;
	uint  a;

	if (newsz >= KERNBASE) return 0;
	if (newsz < oldsz) return oldsz;

	a = PGROUNDUP(oldsz);
	for (; a < newsz; a += PGSIZE) {
		mem = kalloc();
		if (mem == 0) {
			cprintf("allocuvm out of memory\n");
			deallocuvm(pgdir, newsz, oldsz);
			return 0;
		}
		memset(mem, 0, PGSIZE);
		if (mappages(pgdir, (char *)a, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
			cprintf("allocuvm out of memory (2)\n");
			deallocuvm(pgdir, newsz, oldsz);
			kfree(mem);
			return 0;
		}
	}
	return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
	pte_t *pte;
	uint   a, pa;

	if (newsz >= oldsz) return oldsz;

	a = PGROUNDUP(newsz);
	for (; a < oldsz; a += PGSIZE) {
		pte = walkpgdir(pgdir, (char *)a, 0);
		if (!pte)
			a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
		else if ((*pte & PTE_P) != 0) {
			pa = PTE_ADDR(*pte);
			if (pa == 0) panic("kfree");
			char *v = P2V(pa);
			kfree(v);
			*pte = 0;
		}
	}
	return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
	uint i;

	if (pgdir == 0) panic("freevm: no pgdir");
	deallocuvm(pgdir, KERNBASE, 0);
	for (i = 0; i < NPDENTRIES; i++) {
		if (pgdir[i] & PTE_P) {
			char *v = P2V(PTE_ADDR(pgdir[i]));
			kfree(v);
		}
	}
	kfree((char *)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if (pte == 0) panic("clearpteu");
	*pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t *
copyuvm(pde_t *pgdir, uint sz) {
    pde_t *d;
    pte_t *pte;
    uint pa, i, flags;
    char *mem;

    if ((d = setupkvm()) == 0) return 0;

    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walkpgdir(pgdir, (void *)i, 0)) == 0) panic("copyuvm: pte should exist");
        if (!(*pte & PTE_P)) continue;  // Page not present, skip it

        pa = PTE_ADDR(*pte);
        flags = PTE_FLAGS(*pte);
		
/*-----------------------------------------SHARED MEM-----------------------------------------*/
        // Check if the page is a shared memory page
        if (is_shm(pa) == pa) {
			mem = P2V(pa);
			int index = shm_index(pa);
            // Shared memory should not be duplicated, map the same physical page
            if (mappages(d, (void *)i, PGSIZE, V2P(mem), flags) < 0) goto bad;

            shared_memory.shm_mems[index].ref++;

/*-----------------------------------------SHARED MEM-----------------------------------------*/
        } else {
            // Normal memory, duplicate it
	        if ((mem = kalloc()) == 0) goto bad;
            memmove(mem, (char *)P2V(pa), PGSIZE);
            if (mappages(d, (void *)i, PGSIZE, V2P(mem), flags) < 0) goto bad;
        }
    }
    return d;

bad:
    freevm(d);
    return 0;
    freevm(d);
    return 0;
}


// PAGEBREAK!
// Map user virtual address to kernel address.
char *
uva2ka(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if ((*pte & PTE_P) == 0) return 0;
	if ((*pte & PTE_U) == 0) return 0;
	return (char *)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
	char *buf, *pa0;
	uint  n, va0;

	buf = (char *)p;
	while (len > 0) {
		va0 = (uint)PGROUNDDOWN(va);
		pa0 = uva2ka(pgdir, (char *)va0);
		if (pa0 == 0) return -1;
		n = PGSIZE - (va - va0);
		if (n > len) n= len;
		memmove(pa0 + (va - va0), buf, n);
		len -= n;
		buf += n;
		va = va0 + PGSIZE;
	}
	return 0;
}

/*-----------------------------------------SHARED MEM-----------------------------------------*/
int copy_page_table(pde_t *src, pde_t *dest) {
    for (int i = 0; i < NPDENTRIES; i++) {
        if (src[i] & PTE_P) {
            // Page table is present
            pte_t *src_pte = (pte_t*)P2V(PTE_ADDR(src[i]));
            pte_t *dest_pte;

            // Allocate a new page table for dest
            if ((dest_pte = (pte_t*)kalloc()) == 0) {
                return -1; // Failed to allocate page table
            }

            // Set up the page directory entry
            dest[i] = V2P(dest_pte) | PTE_FLAGS(src[i]);

            // Copy entries from the source page table to the destination
            for (int j = 0; j < NPTENTRIES; j++) {
                if (src_pte[j] & PTE_P) {
                    // Page is present
                    char *mem;
                    if ((mem = kalloc()) == 0) {
                        return -1; // Failed to allocate memory
                    }
                    memmove(mem, (char*)P2V(PTE_ADDR(src_pte[j])), PGSIZE);
                    dest_pte[j] = V2P(mem) | PTE_FLAGS(src_pte[j]);
                }
            }
        }
    }
    return 0; // Success
}

void print_pte(pde_t *pgdir, void *va) {
    pte_t *pte = walkpgdir(pgdir, va, 0);
    if (pte && *pte & PTE_P) {
        cprintf("PTE of VA %p: %p\n", va, *pte);
    } else {
        cprintf("PTE not found or not present for VA %p\n", va);
    }
}



void print_shm() {
    cprintf("Shared Memory Segments:\n");
    for (int i = 0; i < shared_memory.shm_num; i++) {
        if (shared_memory.shm_mems[i].ref > 0) {
            cprintf("  Segment %d: Name = '%s', PA = %p, Refs = %d, MuxID = %d\n", 
                    i, 
                    shared_memory.shm_mems[i].name, 
                    shared_memory.shm_mems[i].pa, 
                    shared_memory.shm_mems[i].ref,
					shared_memory.shm_mems[i].shm_mutex_index);
        } else {
            cprintf("  Segment %d: [Empty]\n", i);
        }
    }
}


void print_shm_state() {
    cprintf("Shared Memory State for Process:\n");
	struct proc *curproc = myproc();
	for (int i = 0; i < SHM_MAXNUM; i++) {
		if (curproc->shm[i].va != 0) {
			cprintf("  Segment %d: Name = '%s', VA = %p\n", i, curproc->shm[i].name, curproc->shm[i].va);
		} else {
			cprintf("  Segment %d: [Empty]\n", i);
		}
	}

}


//new and improved gigachad is_shm
uint is_shm(uint pa) {
    for (int i = 0; i < SHM_MAXNUM; i++) {
        if (shared_memory.shm_mems[i].pa == pa) {
            return shared_memory.shm_mems[i].pa; // This is a shared memory page
        }
    }
	return 0;
}

int shm_index(uint pa) {
    for (int i = 0; i < SHM_MAXNUM; i++) {
        if (shared_memory.shm_mems[i].pa == pa) {
            return i;
        }
    }
	return 0;
}





int shm_get(char *name) {

	//TODO
	/*
	char *shm_get(char *name) - Map a 4096 page into the calling process’s virtual
	address space. For the simplest implementation, it should likely map at the address that
	a sbrk would normally use next. A subsequent call to shm_get or sbrk should still work
	(i.e. it can’t overlap with the region a previous call returned). Return NULL if name is NULL
	or if the memory cannot be mapped. The name identifies which shared memory region
	we want to map in. If there are no shared memory regions, and a process calls s =
	shm_get(“ggez”);, then the system will allocate a page (e.g. with kalloc()), and map
	it into the process. If another process then makes the call with the same name, then it
	will map in the same page, thus creating shared memory.


	Note that this API is created to allow you to call this function once, get the memory
	mapped in, then to just use the shared memory from that point onward. Thus, the client,
	and the server, communicating via shared memory, should each only call this function
	once. This is essential to get the performance we’d like by avoiding system calls.
	*/
	
	if (name == 0) {
		
		return -1; // Return error or appropriate value for NULL name
	}

	//took this shit from allocuvm
    uint sz, a, newsz;
    struct proc *curproc = myproc();
    char *mem;

	//took this shit from allocuvm
    sz = curproc->sz;
    a = PGROUNDUP(sz);
    newsz = sz + PGSIZE;
    if (newsz >= KERNBASE) return -1;
    mem = '\0';
	
	for(int i =0 ; i < SHM_MAXNUM; i++){
		if(shared_memory.shm_mems[i].ref > 0){
			//int strncmp(const char *p, const char *q, uint n). Using this strncmp like a dog
			//after cmpr check if we find mem and then add a ref if we do. 
			if(strncmp(name,shared_memory.shm_mems[i].name, SHM_NAME_LEN) == 0){
				mem = (char *) shared_memory.shm_mems[i].pa;
				shared_memory.shm_mems[i].ref ++;
				// gg go next
				break;
			}
		}
	}
	if(mem == 0){
	//this means that we couldnt find it
		//find a new shared_mem spot
		if((shared_memory.shm_num +1) >= SHM_MAXNUM){
			//error if not enoough s pace
			cprintf("MAX SHARED MEM\n"); 
			return 0;
		}

		mem = (char *) V2P(kalloc());
		if (mem == 0) {
			//no way this happens but this means its out of mem
			cprintf("MAX MEM\n"); 
			return 0;
		}
		memset(P2V(mem), 0, PGSIZE);
		mutex_init();
		for(int i =0 ; i < SHM_MAXNUM; i++){
			//check for open shm_mem then referencing it & storing Phyiscal Address
			if(shared_memory.shm_mems[i].ref == 0){
				memmove((void *)&shared_memory.shm_mems[i].name, (void *)name, 16);
				shared_memory.shm_mems[i].ref = 1; 
				shared_memory.shm_mems[i].pa = (uint) mem;

				
				int mutex_id = mutex_create("shm_mutex");
				if (mutex_id == -1) {
					cprintf("NO MUTEX ID\n");
				} else {
					shared_memory.shm_mems[i].shm_mutex_index = mutex_id;
				}
				break;
			}
		}
		shared_memory.shm_num ++;
	}
	//UPDATE the page tables for user lvl.
	if (mappages(curproc->pgdir, (char *)a, PGSIZE,(uint) mem, PTE_W | PTE_U) < 0) {
		//straight from allocuvm
		cprintf("allocuvm out of memory (2)\n");
		kfree(P2V(mem));
		return 0;
	}
    if (newsz != 0) {
        curproc->sz = newsz;
        switchuvm(curproc);

		//now updated the process shm struct in proc.h
        for(int i = 0; i < SHM_MAXNUM; i++) {
            if(curproc->shm[i].va == 0) {
                curproc->shm[i].va = sz;
                memmove((void *)&(curproc->shm[i].name), (void *)name, 16);
                break;
            }
        }
		//cprintf("--------------after shm_get\n");
		//print_pte(curproc->pgdir ,(void *)a);
        return (int)a;
    }

    return -1;
}





int shm_rem(char *name) {

	//TODO
	/*
	int shm_rem(char *name) - This removes a mapping of the shared page from the
	calling process’s virtual address space. If there are no more references to the shared
	page, then it is deallocated, and any subsequent mappings will return a new page. This
	returns -1 if the mapping with the given name doesn’t exist, or if name is NULL.
	*/
	if (name == 0) {
		return -1; // Return error or appropriate value for NULL name
	}

    uint va = 0;
    struct proc *curproc = myproc();
    int i, found = 0;

    // Find the shared memory page in the current process
    for(i = 0; i < SHM_MAXNUM; i++) {
        if(strncmp(name, curproc->shm[i].name, SHM_NAME_LEN) == 0) {
            va = curproc->shm[i].va;
            found = 1;

            break;
        }
    }

    if (!found) {
        cprintf("Name not found\n");
        return -1;
    }

    // Remove the virtual memory mapping
    pte_t *pte = walkpgdir(curproc->pgdir, (char *)va, 0);
    if (pte && *pte) {
        *pte = 0; // Invalidate the page table entry
    } else {
		//();
		//print_pte(curproc->pgdir ,(void *)va);
        cprintf("PTE not found\n");
    }

    // Clear the entry in the process's shared memory pages
    memset(curproc->shm[i].name, sizeof(char) * 16, 0);
    curproc->shm[i].va = 0;

    // Decrement the size of the process's virtual memory
    curproc->sz -= PGSIZE;

    // Update the global shared memory table
    for(i = 0; i < SHM_MAXNUM; i++) {
		//strncmp(const char *p, const char *q, uint n)
        if(shared_memory.shm_mems[i].ref > 0 && strncmp(name, shared_memory.shm_mems[i].name, SHM_NAME_LEN) == 0) {
            shared_memory.shm_mems[i].ref--;
            if(shared_memory.shm_mems[i].ref == 0) {
                // Last reference, deallocate physical memory
                kfree(P2V(shared_memory.shm_mems[i].pa));
                shared_memory.shm_mems[i].pa = 0;
                memset(shared_memory.shm_mems[i].name, 0, 16);

				// Clean up the mutex associated with this shared memory block
				int mutex_id = shared_memory.shm_mems[i].shm_mutex_index;
				if (mutex_delete(mutex_id) != 1) {
					cprintf("Failed to delete mutex\n");
					return -1;
				}

                shared_memory.shm_num--;
            }
            break;
        }
    }

    return 0;
}

/*-----------------------------------------SHARED MEM-----------------------------------------*/