#include <types.h>
#include <string.h>

#include <fuckOS/kernel.h>
#include <fuckOS/assert.h>
#include <fuckOS/tty.h>

#include <mm/pages.h>
#include <mm/layout.h>

#include <asm/x86.h>
struct mmap
{
	uint64_t	addr;
	uint64_t	len;
	uint32_t	type;
	uint32_t 	zero;
};

extern void detect_memory(void*);
extern void slab_init();
extern char _BSS_END[];

static void printmapp();
static void get_max_pfn();
static void get_normal_maxpfn();
static void get_high_maxpfn();
static void get_totalram_pages();
static void bootmm_init();
static void mempage_init();

struct mmap mmapinfo[128];
int nr_mmapinfo;
int max_pfn;
int normal_maxpfn;
int high_maxpfn;
int totalram_pages;
pgd_t *kpgd;

struct page *mempage;

/*
*	获取内存分布信息
*	计算各种全局变量的值
*/
void bootmm(void* ginfo)
{

	detect_memory(ginfo);

	get_max_pfn();

	get_normal_maxpfn();

	get_high_maxpfn();

	get_totalram_pages();

	high_maxpfn = max_pfn < high_maxpfn ? max_pfn : high_maxpfn;

	printmapp();

	bootmm_init();
	
	slab_init();
}



/*
*	获取最大可用页号
*	mmapinfo是从detect_memory计算出来的
*/
static void get_totalram_pages()
{
	struct mmap *mmap = mmapinfo;
	struct mmap *end = mmapinfo + nr_mmapinfo;

	uint32_t last = 0;

	for(mmap ;mmap < end;mmap++) {
		if(mmap->type == 1) {
			last += (mmap->len / PAGE_SIZE);
		}
		
	}
	totalram_pages = last;
}

static void get_max_pfn()
{
	struct mmap *mmap = mmapinfo;
	struct mmap *end = mmapinfo + nr_mmapinfo;

	uint64_t last = 0;

	for(mmap ;mmap < end;mmap++) {
		if(mmap->type == 1) {
			if (last < mmap->addr + mmap->len)
				last = mmap->addr + mmap->len;
		}
	}
	max_pfn = last >> PAGE_SHIFT;
#ifndef	CONFIG_PAE
	if(max_pfn >= 0x100000) {
		max_pfn = 0x100000;
	}
#endif
	
}


static void get_normal_maxpfn()
{
	struct mmap *mmap = mmapinfo;
	struct mmap *end = mmapinfo + nr_mmapinfo;

	int last = 0;

	for(mmap ;mmap < end;mmap++) {
		if(mmap->type == 1) {
			if (mmap->addr >= NORMAL_ADDR)
				continue;

			if (mmap->addr + mmap->len < NORMAL_ADDR) {
				if (mmap->addr + mmap->len > last)
					last = mmap->addr + mmap->len;
			} else{
				last = NORMAL_ADDR;
				break;
			}
		}
	}
	normal_maxpfn = PFN(last);

}

static void get_high_maxpfn()
{
	struct mmap *mmap = mmapinfo;
	struct mmap *end = mmapinfo + nr_mmapinfo;

	uint64_t last = 0;

	for(mmap ;mmap < end;mmap++) {
		if(mmap->type == 1) {
			if (mmap->addr + mmap->len <= KERNEL_NORMAL - KERNEL_BASE_ADDR)
				continue;
			
			if (mmap->addr + mmap->len > last)
					last = mmap->addr + mmap->len;
		
		}
	}
	high_maxpfn = (last & ~0xFFF) >> PAGE_SHIFT;
}


/*
*	内核刚刚启动时的页分配器
*	建立完内核页表时使用，初始化伙伴系统之后就
*	不再使用
*
*/
static void *base = NULL;
static void *alloc_bootmm_pages(int size)
{
	if (!base) {
		base = PAGE_ALIGN(_BSS_END + 0xFFF);
	}
#ifdef	CONFIG_DEBUG
	assert(size > 0);
#endif	
	void *tmp = base;
	base += size * PAGE_SIZE;

	if (tmp >= (void*)(normal_maxpfn * PAGE_SIZE + KERNEL_BASE_ADDR))
		panic("boot_bootmm: we're out of memory!\n");

	memset(tmp,0x0,size * PAGE_SIZE);
	return tmp;
}


static int is_page_aval(int pfn)
{
	struct mmap *mmap = mmapinfo;
	struct mmap *end = mmapinfo + nr_mmapinfo;

	uint64_t last = 0;

	if (pfn <= PFN(v2p(base)))
		return 0;

	if (pfn >= max_pfn)
		return 0;

	for(mmap ;mmap < end;mmap++) {
		if(mmap->type == 1) {
			if (PFN(mmap->addr) <= pfn && pfn < PFN(mmap->addr+ mmap->len))
				return 1;
		}
	}
	return 0;
}

static void check_page(struct page*page)
{
	if (!(page->flags & _pg_reserved)) {
		printk("pfn:%x phys:%lx",page2pfn(page),page2phys(page));
		printk("flags:%lx\n",page->flags);
		panic("check_page\n");
	}
}
static void mempage_init()
{
	int i;
	for(i = 0 ;i < max_pfn;i++) {
		memset(&mempage[i],0,sizeof(struct page));
		
		if(is_page_aval(i)) {
			mempage[i].flags = _pg_free;
			mempage[i].order = -1;
			INIT_LIST_HEAD(&mempage[i].list);
		} else {
			mempage[i].flags = _pg_reserved;
			mempage[i].order = -1;
			INIT_LIST_HEAD(&mempage[i].list);
		}
	}
}

static int get_pmd(pgd_t *pgd,int perm)
{

	struct page* page;
	physaddr_t pa;
	pa = v2p((viraddr_t)alloc_bootmm_pages(1));

#ifdef CONFIG_PAE
	uint32_t pe = perm & ~(_PAGE_RW | _PAGE_USER);
#else
	uint32_t pe = perm;
#endif

	pgd_set(pgd,pa,pe);
	return 0;
}

static int get_pte(pmd_t *pmd,int perm)
{
	struct page* page;
	physaddr_t pa;
	pa = v2p((viraddr_t)alloc_bootmm_pages(1));
	pmd_set(pmd,pa,perm);
	return 0;
}


static pte_t *boot_page_walk(pgd_t * pgdp,viraddr_t address,bool create)
{

	pgd_t * pgd = NULL;
	pte_t * pte = NULL;
	pmd_t * pmd = NULL;
	struct page *page = NULL;

	pgd =  pgd_offset (pgdp, address);

	if (pgd_none(*pgd) && !create)
		goto unlock;
	else if (pgd_none(*pgd)) {
		if( get_pmd(pgd,_PAGE_PRESENT | _PAGE_RW | _PAGE_USER) < 0)
			goto unlock;
		pgd =  pgd_offset (pgdp, address);
	}
	pmd =  pmd_offset (pgd, address);
	if (pmd_none(*pmd) && !create)
		goto unlock;
	else if (pmd_none(*pmd)) {
		if( get_pte(pmd,_PAGE_PRESENT | _PAGE_RW | _PAGE_USER) < 0)
			goto unlock;
		pmd =  pmd_offset (pgd, address);
		
	}
	pte = pte_offset (pmd, address);
unlock:
	return pte;
}

/*
*
*	对内核页表进行映射
*
*/
static void boot_map_region(pgd_t *pgdir, viraddr_t va, 
			size_t size, physaddr_t pa, int perm)
{
	uint32_t i;
	for ( i = 0; i < size; i += PAGE_SIZE ) {
		pte_t* pte = boot_page_walk(pgdir,va + i, true);;
		assert(pte);
		pte_set(pte, pa + i, perm);
	}
	
}

viraddr_t
mmio_map_region(physaddr_t pa, size_t size)
{
	
	static viraddr_t base = KERNEL_MMIO;

	size = ROUNDUP(size, PAGE_SIZE);
	pa = ROUNDDOWN(pa, PAGE_SIZE);
	
	if (base+size >= KERNEL_MMIO_LIMIT) 
		panic("not enough memory");
	
	boot_map_region(kpgd, base, size, pa, _PAGE_PRESENT|_PAGE_PCD|_PAGE_PWT|_PAGE_RW);
	base += size;

	return (base - size);
}

extern void page_check();
extern void zone_init();
extern uint8_t percpu_kstacks[CPUNUMS][KERNEL_STKSIZE];
static void bootmm_init()
{
	int num;
	int i;

	//分配内核页目录
	kpgd = alloc_bootmm_pages(1);
	memset(kpgd,0,PAGE_SIZE);

	//映射内核页目录 0xc0000000 ~ 0xf0000000 = 0x00000000 ~ 0x30000000
	boot_map_region(kpgd, KERNEL_BASE_ADDR, NORMAL_ADDR, 0, _PAGE_RW | _PAGE_PRESENT);
	
	//映射内核VAG映射区
	boot_map_region(kpgd, KERNEL_VIDEO, PT_SIZE, 0xb8000, _PAGE_RW | _PAGE_PRESENT);
	
	//映射LAPIC区域
	for(i = 0;i < CPUNUMS; i++) {
		boot_map_region(kpgd, KERNEL_MMIO + i*PAGE_SIZE, PAGE_SIZE, 0xFEE00000,
					 _PAGE_PRESENT|_PAGE_PCD|_PAGE_PWT|_PAGE_RW);
	}
	//映射内核栈区域
	for(i = 0;i < CPUNUMS; i++) {
		boot_map_region(kpgd, KERNEL_STACK_TOP -KERNEL_STKSIZE - (KERNEL_STKSIZE + PAGE_SIZE) * i, 
			KERNEL_STKSIZE, v2p(percpu_kstacks[i]),_PAGE_PRESENT|_PAGE_RW);
	}

	lcr3(pgd2p(kpgd));
	
	updata_display_addr(KERNEL_VIDEO);

	num = max_pfn * sizeof(struct page) / PAGE_SIZE + 1;

	mempage = alloc_bootmm_pages(num);//分配page数组

	assert(mempage);


	//初始化page数组
	mempage_init();

	//初始化伙伴分配系统 mm/zone.c
	zone_init();

}

static void printmapp()
{
	struct mmap *mmap = mmapinfo;
	struct mmap *end = mmapinfo + nr_mmapinfo;
	for(mmap ;mmap < end;mmap++) {
		printk ("0x%016llx 0x%016llx %x\n",mmap->addr,mmap->len,mmap->type);
	}
	printk("max_pfn %x ",max_pfn);
	printk("high_maxpfn:%x ",high_maxpfn);
	printk("normal_maxpfn:%x \n",normal_maxpfn);
	printk("totalram_pages:%d \n",totalram_pages);
	
}
