#include <default_pmm.h>
#include <best_fit_pmm.h>
// lab2 Buddy integration - 2412712
#include <buddy_pmm.h>
// lab2 optional independent tests - 2412712
#ifdef BUDDY_TEST
#include <buddy_test.h>
#endif
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
// choose a pmm_manager instance according to the compile-time configuration
#if defined(PMM_USE_BUDDY)
    pmm_manager = &buddy_pmm_manager;
#elif defined(PMM_USE_FIRST_FIT)
    pmm_manager = &default_pmm_manager;
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2413318
static void page_init(void) {
    extern char end[];
    uint64_t memory_base = get_memory_base();
    uint64_t memory_size = get_memory_size();

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    if (memory_base == 0 || memory_size == 0 ||
        memory_size > (uint64_t)-1 - memory_base) {
        panic("page_init: invalid DTB memory range");
    }
    uint64_t memory_end = memory_base + memory_size;
    cprintf("Physical memory from DTB: [0x%016lx, 0x%016lx)\n",
            memory_base, memory_end);

    // The fixed nbase layout may describe unavailable lower pages, but they
    // remain reserved. The reported range must contain the whole kernel.
    uintptr_t kernel_end = PADDR(end);
    if (memory_base > KERNEL_BEGIN_PADDR ||
        kernel_end < KERNEL_BEGIN_PADDR || kernel_end > memory_end ||
        kernel_end > PHYSICAL_MEMORY_END) {
        panic("page_init: memory range does not contain the kernel");
    }
    uintptr_t managed_end = ROUNDDOWN(
        memory_end < PHYSICAL_MEMORY_END ? memory_end : PHYSICAL_MEMORY_END,
        PGSIZE);
    if (managed_end <= KERNEL_BEGIN_PADDR) {
        panic("page_init: no manageable memory");
    }
    npage = PPN(managed_end);
    size_t page_count = npage - nbase;
    uintptr_t descriptors_pa = ROUNDUP(kernel_end, PGSIZE);

    // Check capacity before multiplying or touching the descriptor array.
    if (descriptors_pa > managed_end ||
        page_count > (managed_end - descriptors_pa) / sizeof(struct Page)) {
        panic("page_init: insufficient memory for Page descriptors");
    }
    uintptr_t descriptors_end = descriptors_pa + page_count * sizeof(struct Page);
    uintptr_t free_begin = ROUNDUP(descriptors_end, PGSIZE);
    pages = (struct Page *)(descriptors_pa + va_pa_offset);
    for (size_t i = 0; i < page_count; i++) {
        pages[i].flags = 0;
        SetPageReserved(&pages[i]);
        set_page_ref(&pages[i], 0);
        pages[i].property = 0;
        list_init(&pages[i].page_link);
    }

    cprintf("Managed physical memory end: 0x%016lx\n", managed_end);
    cprintf("Page descriptors: PA [0x%016lx, 0x%016lx)\n",
            descriptors_pa, descriptors_end);
    if (free_begin < managed_end) {
        size_t free_count = (managed_end - free_begin) / PGSIZE;
        init_memmap(pa2page(free_begin), free_count);
        cprintf("Free physical pages: [0x%016lx, 0x%016lx), %lu pages\n",
                free_begin, managed_end, free_count);
    } else {
        cprintf("page_init: no complete free physical pages to register\n");
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // lab2 - 2412712: isolate tests before registering any real free pages.
#ifdef BUDDY_TEST
    buddy_test_all();
#endif

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
