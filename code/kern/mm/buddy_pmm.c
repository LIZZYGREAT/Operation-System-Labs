#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>

/* lab2 Buddy System - 2412712
 * Each order has a circular free list. Alignment and XOR use absolute PPNs.
 * Free head: PG_property set, property = block pages.
 * Allocated head: BUDDY_ALLOCATED set, property = original request pages.
 * Interior pages have neither head flag and property == 0.
 */
#define BUDDY_ALLOCATED ((uint64_t)1 << 2)
#define BUDDY_ORDER_COUNT (sizeof(size_t) * 8 - PGSHIFT)
#define BUDDY_CAPACITY ((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE)

#if PG_reserved == 2 || PG_property == 2
#error "Buddy allocated-head flag conflicts with existing page flags"
#endif

static list_entry_t buddy_free_list[BUDDY_ORDER_COUNT];
static size_t buddy_nr_free;
static unsigned int buddy_max_order;

static size_t
buddy_block_pages(unsigned int order) {
    assert(order <= buddy_max_order);
    return (size_t)1 << order;
}

/* Validate the address before subtracting descriptor pointers or dereferencing. */
static size_t
buddy_page_index(struct Page *page) {
    assert(pages != NULL && npage >= nbase);
    assert((uintptr_t)page >= (uintptr_t)pages);
    uintptr_t offset = (uintptr_t)page - (uintptr_t)pages;
    assert(offset % sizeof(struct Page) == 0);
    size_t index = offset / sizeof(struct Page);
    assert(index < npage - nbase);
    return index;
}

/* The maximum order bounds every shift, including oversized requests. */
static bool
buddy_request_order(size_t n, unsigned int *order) {
    if (n == 0 || n > buddy_block_pages(buddy_max_order)) {
        return 0;
    }
    *order = 0;
    while (buddy_block_pages(*order) < n) {
        (*order)++;
    }
    return 1;
}

static void
buddy_add_free(struct Page *page, unsigned int order) {
    assert(!PageReserved(page) && !PageProperty(page));
    assert(!(page->flags & BUDDY_ALLOCATED));
    assert(list_empty(&page->page_link));
    page->property = buddy_block_pages(order);
    SetPageProperty(page);
    list_add(&buddy_free_list[order], &page->page_link);
}

static void
buddy_remove_free(struct Page *page) {
    assert(PageProperty(page));
    list_del_init(&page->page_link);
    ClearPageProperty(page);
    page->property = 0;
}

/* Called with an unlinked block; coalescing never changes the page count. */
static void
buddy_release_block(struct Page *page, unsigned int order) {
    size_t ppn = page2ppn(page);
    while (order < buddy_max_order) {
        size_t count = buddy_block_pages(order);
        size_t partner_ppn = ppn ^ count;
        if (partner_ppn < nbase || partner_ppn >= npage ||
            count > npage - partner_ppn) {
            break;
        }
        struct Page *partner = &pages[partner_ppn - nbase];
        if (PageReserved(partner) || !PageProperty(partner) ||
            (partner->flags & BUDDY_ALLOCATED) || partner->property != count) {
            break;
        }
        buddy_remove_free(partner);
        if (partner_ppn < ppn) {
            page = partner;
            ppn = partner_ppn;
        }
        order++;
    }
    buddy_add_free(page, order);
}

static void
buddy_init(void) {
    /* init() precedes page_init(): derive capacity without reading npage. */
    size_t capacity = BUDDY_CAPACITY;
    assert(capacity > 0 && capacity <= (unsigned int)-1);
    buddy_max_order = 0;
    while ((capacity >>= 1) != 0) {
        buddy_max_order++;
    }
    assert(buddy_max_order < BUDDY_ORDER_COUNT);
    for (size_t i = 0; i < BUDDY_ORDER_COUNT; i++) {
        list_init(&buddy_free_list[i]);
    }
    buddy_nr_free = 0;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    size_t index = buddy_page_index(base);
    assert(npage - nbase <= BUDDY_CAPACITY);
    assert(n <= npage - nbase - index);
    assert(buddy_nr_free <= npage - nbase - n);
    /* Validate the whole range before changing any descriptor or list. */
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
    }
    for (size_t i = 0; i < n; i++) {
        struct Page *page = base + i;
        page->flags = page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }

    size_t remaining = n;
    struct Page *page = base;
    while (remaining != 0) {
        unsigned int order = buddy_max_order;
        size_t ppn = page2ppn(page);
        while (buddy_block_pages(order) > remaining ||
               (ppn & (buddy_block_pages(order) - 1)) != 0) {
            assert(order > 0);
            order--;
        }
        size_t count = buddy_block_pages(order);
        buddy_release_block(page, order);
        page += count;
        remaining -= count;
    }
    buddy_nr_free += n;
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    unsigned int order;
    if (!buddy_request_order(n, &order)) {
        return NULL;
    }
    size_t count = buddy_block_pages(order);
    if (count > buddy_nr_free) {
        return NULL;
    }
    unsigned int found = order;
    while (found <= buddy_max_order && list_empty(&buddy_free_list[found])) {
        found++;
    }
    if (found > buddy_max_order) {
        return NULL;
    }

    struct Page *page = le2page(list_next(&buddy_free_list[found]), page_link);
    buddy_remove_free(page);
    while (found > order) {
        found--;
        buddy_add_free(page + buddy_block_pages(found), found);
    }
    page->flags |= BUDDY_ALLOCATED;
    page->property = n;
    buddy_nr_free -= count;
    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    size_t index = buddy_page_index(base);
    assert(!PageReserved(base) && !PageProperty(base));
    assert((base->flags & BUDDY_ALLOCATED) && base->property == n);
    unsigned int order;
    assert(buddy_request_order(n, &order));
    size_t count = buddy_block_pages(order);
    assert(count <= npage - nbase - index);
    assert((page2ppn(base) & (count - 1)) == 0);
    assert(buddy_nr_free <= npage - nbase - count);
    for (size_t i = 0; i < count; i++) {
        struct Page *page = base + i;
        assert(!PageReserved(page) && !PageProperty(page));
        assert(i == 0 || (!(page->flags & BUDDY_ALLOCATED) && page->property == 0));
    }
    for (size_t i = 0; i < count; i++) {
        struct Page *page = base + i;
        page->flags = page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }
    buddy_release_block(base, order);
    buddy_nr_free += count;
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_nr_free;
}

/* Basic structural checks; the independent test suite is a later task. */
static void
buddy_check_lists(void) {
    size_t total = 0, blocks = 0;
    for (unsigned int order = 0; order <= buddy_max_order; order++) {
        list_entry_t *head = &buddy_free_list[order];
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
            struct Page *page = le2page(le, page_link);
            size_t index = buddy_page_index(page);
            size_t count = buddy_block_pages(order);
            assert(++blocks <= npage - nbase);
            assert(count <= npage - nbase - index);
            assert((page2ppn(page) & (count - 1)) == 0);
            assert(PageProperty(page) && page->property == count);
            assert(!PageReserved(page) && !(page->flags & BUDDY_ALLOCATED));
            assert(le->next->prev == le && le->prev->next == le);
            for (size_t i = 1; i < count; i++) {
                assert(!PageReserved(page + i) && !PageProperty(page + i));
                assert(!((page + i)->flags & BUDDY_ALLOCATED));
                assert((page + i)->property == 0);
            }
            total += count;
            assert(total <= npage - nbase);
            if (order < buddy_max_order) {
                size_t partner_ppn = page2ppn(page) ^ count;
                if (partner_ppn >= nbase && partner_ppn < npage &&
                    count <= npage - partner_ppn) {
                    struct Page *partner = &pages[partner_ppn - nbase];
                    assert(!PageProperty(partner) || partner->property != count);
                }
            }
        }
    }
    assert(total == nr_free_pages());
}

static void
buddy_check(void) {
    assert(pmm_manager == &buddy_pmm_manager);
    size_t before = nr_free_pages();
    buddy_check_lists();
    assert(alloc_pages((size_t)-1) == NULL);
    assert(nr_free_pages() == before);
    if (before == 0) {
        assert(alloc_pages(1) == NULL);
        cprintf("buddy_check: no free pages; allocation exercises skipped\n");
        return;
    }
    for (unsigned int pass = 0; pass < 2; pass++) {
        struct Page *single = alloc_pages(1);
        assert(single != NULL && nr_free_pages() == before - 1);
        assert(page_ref(single) == 0 && !PageProperty(single));
        struct Page *rounded = alloc_pages(3);
        if (rounded != NULL) {
            assert(nr_free_pages() == before - 5);
            assert(single + 1 <= rounded || rounded + 4 <= single);
            assert(rounded->property == 3 && !PageProperty(rounded));
        } else {
            assert(nr_free_pages() == before - 1);
            cprintf("buddy_check: no suitable 4-page block; rounded case skipped\n");
        }
        buddy_check_lists();
        free_pages(single, 1);
        if (rounded != NULL) {
            assert(nr_free_pages() == before - 4);
            free_pages(rounded, 3);
        }
        assert(nr_free_pages() == before);
        buddy_check_lists();
    }
    cprintf("buddy_check: passed (free pages restored: %lu)\n", before);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
