#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// The kernel image is mapped at VA=KERNBASE and PA=info.base
uint_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
pde_t *boot_pgdir_va = NULL;
// physical address of boot-time page directory
uintptr_t boot_pgdir_pa;

// physical memory management
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void)
{
    pmm_manager = &best_fit_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

// Initialize physical page descriptors and hand the remaining memory to the
// selected page allocator. The kernel image and descriptors stay reserved.
static void page_init(void)
{
    extern char end[];
    uint64_t memory_base = get_memory_base();
    uint64_t memory_size = get_memory_size();

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    uintptr_t kernel_begin_pa = KERNBASE - va_pa_offset;
    uintptr_t physical_limit = KERNTOP - va_pa_offset;
    if (memory_base == 0 || memory_size == 0 ||
        memory_size > (uint64_t)-1 - memory_base)
    {
        panic("page_init: invalid DTB memory range");
    }
    uint64_t memory_end = memory_base + memory_size;
    cprintf("Physical memory from DTB: [0x%016lx, 0x%016lx)\n",
            memory_base, memory_end);

    uintptr_t kernel_end = PADDR(end);
    if (memory_base > kernel_begin_pa || kernel_end < kernel_begin_pa ||
        kernel_end > memory_end || kernel_end > physical_limit)
    {
        panic("page_init: memory range does not contain the kernel");
    }
    uintptr_t managed_end = ROUNDDOWN(
        memory_end < physical_limit ? memory_end : physical_limit, PGSIZE);
    if (managed_end <= kernel_begin_pa)
    {
        panic("page_init: no manageable memory");
    }
    if (kernel_end > managed_end)
    {
        panic("page_init: insufficient memory for Page descriptors");
    }

    npage = PPN(managed_end);
    if (npage <= nbase)
    {
        panic("page_init: managed memory is below DRAM_BASE");
    }
    size_t page_count = npage - nbase;
    uintptr_t descriptors_pa = ROUNDUP(kernel_end, PGSIZE);
    if (descriptors_pa > managed_end ||
        page_count > (managed_end - descriptors_pa) / sizeof(struct Page))
    {
        panic("page_init: insufficient memory for Page descriptors");
    }

    uintptr_t descriptors_end = descriptors_pa + page_count * sizeof(struct Page);
    uintptr_t free_begin = ROUNDUP(descriptors_end, PGSIZE);
    pages = (struct Page *)(descriptors_pa + va_pa_offset);
    for (size_t i = 0; i < page_count; i++)
    {
        struct Page *page = &pages[i];
        page->flags = 0;
        SetPageReserved(page);
        set_page_ref(page, 0);
        page->property = 0;
        list_init(&page->page_link);
        list_init(&page->pra_page_link);
        page->pra_vaddr = 0;
    }

    cprintf("Managed physical memory end: 0x%016lx\n", managed_end);
    cprintf("Page descriptors: PA [0x%016lx, 0x%016lx)\n",
            descriptors_pa, descriptors_end);
    if (free_begin < managed_end)
    {
        size_t free_count = (managed_end - free_begin) / PGSIZE;
        init_memmap(pa2page(free_begin), free_count);
        cprintf("Free physical pages: [0x%016lx, 0x%016lx), %lu pages\n",
                free_begin, managed_end, free_count);
    }
    else
    {
        cprintf("page_init: no complete free physical pages to register\n");
    }
}

static void enable_paging(void)
{
    write_csr(satp, 0x8000000000000000 | (boot_pgdir_pa >> RISCV_PGSHIFT));
}

// boot_map_segment - setup&enable the paging mechanism
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  size: memory size
//  pa:   physical address of this memory
//  perm: permission of this memory
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE)
    {
        pte_t *ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);
    }
}

// boot_alloc_page - allocate one page using pmm->alloc_pages(1)
// return value: the kernel virtual address of this allocated page
// note: this function is used to get the memory for PDT(Page Directory
// Table)&PT(Page Table)
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();
    if (p == NULL)
    {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(p);
}

// pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup
// paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void)
{
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a
    // pmm
    check_alloc_page();
    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // now the basic virtual memory map(see memalyout.h) is established.
    // check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    kmalloc_init();
}

// get_pte - get pte and return the kernel virtual address of this pte for la
//        - if the PT contians this pte didn't exist, alloc a page for PT
// parameter:
//  pgdir:  the kernel virtual base address of PDT
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    if (pgdir == NULL)
    {
        return NULL;
    }

    bool pdep1_created = 0;
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if ((*pdep1 & PTE_V) && (*pdep1 & (PTE_R | PTE_W | PTE_X)))
    {
        // This address is covered by a superpage leaf, not a page table.
        return NULL;
    }
    if (!(*pdep1 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
        pdep1_created = 1;
    }
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if ((*pdep0 & PTE_V) && (*pdep0 & (PTE_R | PTE_W | PTE_X)))
    {
        // The next level is also allowed to contain a superpage leaf.
        return NULL;
    }
    if (!(*pdep0 & PTE_V))
    {
        struct Page *page;
        if (!create)
        {
            return NULL;
        }
        if ((page = alloc_page()) == NULL)
        {
            if (pdep1_created)
            {
                struct Page *pdep1_page = pte2page(*pdep1);
                *pdep1 = 0;
                tlb_invalidate(pgdir, la);
                assert(page_ref_dec(pdep1_page) == 0);
                free_page(pdep1_page);
            }
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

// get_page - get related Page struct for linear address la using PDT pgdir
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_V)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

// Insert a 4 KiB page mapping, replacing any existing mapping at la.
// 2412700
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    if (pgdir == NULL || page == NULL)
    {
        return -E_INVAL;
    }
    ppn_t ppn = page2ppn(page);

    const pte_t leaf_permissions = PTE_R | PTE_W | PTE_X;
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (*pdep1 & PTE_V)
    {
        if (*pdep1 & leaf_permissions)
        {
            return -E_INVAL;
        }
        pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
        if ((*pdep0 & PTE_V) && (*pdep0 & leaf_permissions))
        {
            return -E_INVAL;
        }
    }

    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }

    page_ref_inc(page);
    if (*ptep & PTE_V)
    {
        struct Page *mapped_page = pte2page(*ptep);
        if (mapped_page == page)
        {
            // Keep the existing reference while updating permissions.
            page_ref_dec(page);
        }
        else
        {
            page_remove(pgdir, la);
        }
    }

    *ptep = pte_create(ppn, perm);
    tlb_invalidate(pgdir, la);
    return 0;
}

// Remove the mapping at la and release its physical page when the last
// mapping reference is gone.
// 2412700
void page_remove(pde_t *pgdir, uintptr_t la)
{
    if (pgdir == NULL)
    {
        return;
    }

    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep == NULL || !(*ptep & PTE_V))
    {
        return;
    }

    struct Page *page = pte2page(*ptep);
    assert(page_ref(page) > 0);

    *ptep = 0;
    tlb_invalidate(pgdir, la);
    if (page_ref_dec(page) == 0)
    {
        free_page(page);
    }
}

// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    // flush_tlb();
    // The flush_tlb flush the entire TLB, is there any better way?
    asm volatile("sfence.vma %0" : : "r"(la));
}

static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(boot_pgdir_va[0] & PTE_U);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void)
{
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    uintptr_t pa_begin = ROUNDDOWN(KERNBASE - va_pa_offset, PGSIZE);
    uintptr_t pa_end = npage << PGSHIFT;
    const uintptr_t gigapage_size = (uintptr_t)1 << PDX1SHIFT;
    const uintptr_t megapage_size = (uintptr_t)1 << PDX0SHIFT;
    const pte_t leaf_permissions = PTE_R | PTE_W | PTE_X;

    for (uintptr_t pa = pa_begin; pa < pa_end; pa += PGSIZE)
    {
        uintptr_t va = (uintptr_t)KADDR(pa);
        pde_t pde1 = boot_pgdir_va[PDX1(va)];
        assert(pde1 & PTE_V);

        uintptr_t mapped_pa;
        if (pde1 & leaf_permissions)
        {
            uintptr_t base = PDE_ADDR(pde1) & ~(gigapage_size - 1);
            mapped_pa = base | (va & (gigapage_size - 1));
        }
        else
        {
            pde_t *pd0 = (pde_t *)KADDR(PDE_ADDR(pde1));
            pde_t pde0 = pd0[PDX0(va)];
            assert(pde0 & PTE_V);
            if (pde0 & leaf_permissions)
            {
                uintptr_t base = PDE_ADDR(pde0) & ~(megapage_size - 1);
                mapped_pa = base | (va & (megapage_size - 1));
            }
            else
            {
                pte_t *ptep = get_pte(boot_pgdir_va, va, 0);
                assert(ptep != NULL && (*ptep & PTE_V));
                mapped_pa = PTE_ADDR(*ptep) | PGOFF(va);
            }
        }
        assert(mapped_pa == pa);
    }

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

// perm2str - use string 'u,r,w,-' to present the permission
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// get_pgtable_items - In [left, right] range of PDT or PT, find a continuous
// linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
//  return value: 0 - not a invalid item range, perm - a valid item range with
//  perm permission
static int get_pgtable_items(size_t left, size_t right, size_t start,
                             uintptr_t *table, size_t *left_store,
                             size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    while (start < right && !(table[start] & PTE_V))
    {
        start++;
    }
    if (start < right)
    {
        if (left_store != NULL)
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm)
        {
            start++;
        }
        if (right_store != NULL)
        {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}
