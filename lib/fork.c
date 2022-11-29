// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	if ((err & FEC_WR) == 0)
		panic("pgfault: faulting address [%08x] not a write\n", addr);

	void *page_aligned_addr = (void *) ROUNDDOWN(addr, PGSIZE);
	uint32_t page_num = (uint32_t) page_aligned_addr / PGSIZE;
	if (!(uvpt[page_num] & PTE_COW))
		panic("pgfault: fault was not on a copy-on-write page\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	if ((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_alloc: %e\n", r);

	// Copy over
	void *src_addr = (void *) ROUNDDOWN(addr, PGSIZE);
	memmove(PFTEMP, src_addr, PGSIZE);

	// Remap
	if ((r = sys_page_map(0, PFTEMP, 0, src_addr, PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_map: %e\n", r);

	// panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.

	// get the child envid, and all syscall need the ID
	envid_t parent_envid = sys_getenvid();
    void *va = (void *)(pn * PGSIZE);
	// change the writable page as PTE_COW
    if ((uvpt[pn] & PTE_W) == PTE_W || (uvpt[pn] & PTE_COW) == PTE_COW) {
        if ((r = sys_page_map(parent_envid, va, envid, va, PTE_COW | PTE_U | PTE_P)) != 0) {
            panic("duppage: %e", r);
        }
        if ((r = sys_page_map(parent_envid, va, parent_envid, va, PTE_COW | PTE_U | PTE_P)) != 0) {
            panic("duppage: %e", r);
        }
    } else {
        if ((r = sys_page_map(parent_envid, va, envid, va, PTE_U | PTE_P)) != 0) {
            panic("duppage: %e", r);
        }
    }

	// panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.

    int r;
	envid_t envid;
    uint32_t addr;

	// use handler address to set the pgfault
    set_pgfault_handler(pgfault);

    // construct new env
    envid = sys_exofork();

	// Check if envid is valid
    if (envid < 0) {
        panic("sys_exofork: %e", envid);
    }

	// update thisenv in child if envid is valid
    if (envid == 0) {
		// update thisenv
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    // copy the address space mappings (page table and directory) to child
    for (addr = 0; addr < USTACKTOP; addr += PGSIZE) {
        if ((uvpd[PDX(addr)] & PTE_P) == PTE_P && (uvpt[PGNUM(addr)] & PTE_P) == PTE_P) {
            duppage(envid, PGNUM(addr));
        }
    }

    // set address for  _pgfault_upcall
    void _pgfault_upcall();

	// allocate new page for child's user exception stack
    if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P)) != 0) {
		// panic if error is returned
        panic("fork: %e", r);
    }

    // set the _pgfault_upcall address and save it in Env->env_pgfault_upcall
    if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) != 0) {
		// panic if error is returned
        panic("fork: %e", r);
    }

    // sys_env_set_status marks child as runnable
    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) != 0) {
		// panic if error is returned
        panic("fork: %e", r);
	}

    return envid;

	panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
