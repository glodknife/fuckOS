#include <fuckOS/task.h>
#include <fuckOS/sched.h>

#include <sys/elf.h>

#include <mm/pages.h>
#include <mm/mmu.h>

#include <errno.h>
#include <string.h>

static int load_icode(struct task_struct *, uint8_t *);
static int region_alloc(struct task_struct *, viraddr_t, size_t ,int );
static int task_alloc(struct task_struct **, pid_t);

extern int alloc_pidmap();
extern void free_pidmap(pid_t);

static int
load_icode(struct task_struct *task, uint8_t *binary)
{
	assert(task);
	assert(binary);
	int r;
	struct vm_area_struct* vma = NULL;
	ElfHeader* eh = (ElfHeader*)binary;

	if(eh->e_magic != ELF_MAGIC) {
		return -ENOEXEC;
	}

	ProgHeader* ph = (ProgHeader*)((uint8_t *)eh + eh->e_phoff);

	ProgHeader* eph = ph + eh->e_phnum;

	lcr3(pgd2p(task->mm->mm_pgd));


	for(ph;ph < eph;ph++) {
		if (ph->p_type != ELF_PROG_LOAD)	
			continue;

		if ((ph->p_flags & ELF_PROG_FLAG_EXEC) && 
		   !(ph->p_flags & ELF_PROG_FLAG_WRITE)) {	
			task->mm->start_code = ph->p_va;
			task->mm->end_code = ph->p_va + ph->p_memsz;
		} else {
			task->mm->start_data = ph->p_va;
			task->mm->end_data = ph->p_va + ph->p_memsz;
		}
		
		vma = create_vma(task->mm, ph->p_va, ph->p_memsz, VM_READ | VM_WRITE);
	
		if (!vma)
			return -ENOMEM;

		r = region_alloc(task, ph->p_va, ph->p_memsz,
				_PAGE_PRESENT | _PAGE_RW |_PAGE_USER);
		if (r < 0) 
			return r;
		memcpy((void *)ph->p_va, (void *)(binary + ph->p_offset), ph->p_filesz);
		if ( ph->p_filesz > ph->p_memsz )  
			return -ENOEXEC;
		else 
			memset((char *)ph->p_va + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
	}
	
	//用户栈区域
	vma = create_vma(task->mm, USER_STACKBOTT, USER_STACK_SIZE, VM_READ | VM_WRITE);
	if (!vma) {
		panic("vma\n");	
		return -ENOMEM;
	}

	//用户堆区域
	task->mm->start_brk = (task->mm->end_data ? task->mm->end_data : task->mm->end_code);
	task->mm->start_brk = ROUNDUP(task->mm->start_brk,PAGE_SIZE);
	task->mm->end_brk = task->mm->start_brk + PAGE_SIZE;
	vma = create_vma(task->mm, task->mm->start_brk, PAGE_SIZE , VM_READ | VM_WRITE);
	if (!vma) {
		panic("vma\n");			
		return -ENOMEM;
	}

	

	task->frame.tf_eip = eh->e_entry;

	lcr3(pgd2p(kpgd));
	return 0;
}

static int
region_alloc(struct task_struct *task, viraddr_t va, size_t len,int perm)
{
	assert(task);
	assert(va);
	assert(len > 0);
	int r;
	struct page* page;
	viraddr_t down = ROUNDDOWN(va,PAGE_SIZE);
	viraddr_t up = ROUNDUP(va + len,PAGE_SIZE);	
	for (;down < up;down += PAGE_SIZE) {
		page = page_alloc(_GFP_ZERO);
		
		if(page == NULL) 
			return -ENOMEM;

		r = page_insert(task->mm->mm_pgd, page, down, perm);
		if(r < 0) 
			return r;
	}
	return 0;
}
int task_set_vm(struct task_struct *task)
{
	struct page *page;
	struct mm_struct* mm;
	pgd_t *pgd;
	mm = alloc_mm();

	if (!mm) {
		return -ENOMEM;
	}
	
	page = page_alloc(_GFP_ZERO);
	if (!page) {
		return -ENOMEM;
	}
	pgd = (pgd_t *)page2virt(page);

	memset(pgd,0,PAGE_SIZE);
	task->mm = mm;
	mm->mm_pgd = pgd;
	task->task_pgd = pgd;
	memmove(pgd,kpgd,PAGE_SIZE);
	return 0;
}
static int
task_alloc(struct task_struct **newenv_store, pid_t parent_id)
{
	struct task_struct *task;
	int reval;
	task = kmalloc(sizeof(struct task_struct));
	if (!task) {
		assert(0);
		return -ENOMEM;
	}
	reval = task_set_vm(task);
	if (reval < 0) {
		assert(0);
		kfree(task);
		return reval;
	}

	task->pid = alloc_pidmap();

	// Set the basic status variables.
	task->ppid = parent_id;
	task->task_type = TASK_TYPE_USER;
	task->task_status = TASK_RUNNING;


	memset(&task->frame, 0, sizeof(struct frame));


	task->frame.tf_ds = _USER_DS_ | 3;
	task->frame.tf_es = _USER_DS_ | 3;
	task->frame.tf_ss = _USER_DS_ | 3;
	task->frame.tf_esp = USER_STACKTOP;
	task->frame.tf_cs = _USER_CS_ | 3;
	
	task->frame.tf_eflags = FL_IF;

	if(newenv_store)
		*newenv_store  = task;
	return 0;
}

int
task_create(uint8_t *binary, enum task_type type)
{
	int reval;
	struct task_struct *task;
	reval = task_alloc(&task, 0);
	if (reval < 0) {
		assert(0);		
		return reval;
	}

	reval = load_icode(task, binary);
	if (reval < 0) {
		assert(0);
		return reval;
	}
	task->task_type = type;

	schedule_add_task(task);
	return 0;
}

void
task_pop_tf(struct frame *tf)
{
	// Record the CPU we are running on for user-space debugging

	__asm __volatile("movl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
		"\tiret"
		: : "g" (tf) : "memory");
	panic("iret failed");  /* mostly to placate the compiler */
}
void print_frame(struct frame *tf);
int
task_run(struct task_struct *task)
{
	//if (curtask != NULL ) 
	//	curenv->env_status = TASK_INTERRUPTIBLE;
	curtask = task;
	curtask->task_status = TASK_RUNNING;

	lcr3(pgd2p(curtask->mm->mm_pgd));
	task_pop_tf(&task->frame);
	return 0;
}

void
task_free(struct task_struct *task)
{
	
}


