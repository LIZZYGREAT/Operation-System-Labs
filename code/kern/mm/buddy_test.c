#include <buddy_test.h>

#ifdef BUDDY_TEST
#include <buddy_pmm.h>
#include <stdio.h>

/* lab2 independent Buddy tests - 2412712
 * These descriptors are synthetic. Never dereference their physical pages:
 * their PPNs may refer to real firmware/kernel memory. The oracle is a simple
 * per-page ownership map, independent of the allocator's free lists.
 */
#define TEST_PAGES 64
#define MODEL_RESERVED (-2)
#define MODEL_FREE (-1)
#define ALLOCATED_HEAD ((uint64_t)1 << 2)

static struct Page test_pages[TEST_PAGES];
static int owner[TEST_PAGES];
static size_t requests[TEST_PAGES];
static size_t expected_free;
static bool fixture_ready;

static size_t
rounded_pages(size_t request) {
    assert(request > 0 && request <= TEST_PAGES);
    size_t count = 1;
    while (count < request) {
        count *= 2;
    }
    return count;
}

static size_t
test_index(struct Page *page) {
    assert((uintptr_t)page >= (uintptr_t)test_pages);
    uintptr_t offset = (uintptr_t)page - (uintptr_t)test_pages;
    assert(offset % sizeof(struct Page) == 0);
    assert(offset / sizeof(struct Page) < TEST_PAGES);
    return offset / sizeof(struct Page);
}

/* Check the oracle, descriptor partition, head flags, and list links. */
static void
check_model(void) {
    bool covered[TEST_PAGES] = {0};
    size_t free_count = 0, allocated_count = 0;
    for (size_t i = 0; i < TEST_PAGES; i++) {
        struct Page *page = &test_pages[i];
        assert(page_ref(page) == 0);
        if (owner[i] == MODEL_RESERVED) {
            assert(PageReserved(page) && !PageProperty(page));
            assert(!(page->flags & ALLOCATED_HEAD) && page->property == 0);
        } else if (owner[i] == MODEL_FREE) {
            assert(!PageReserved(page) && !(page->flags & ALLOCATED_HEAD));
            free_count++;
            if (PageProperty(page)) {
                size_t count = page->property;
                assert(count > 0 && (count & (count - 1)) == 0);
                assert(count <= TEST_PAGES - i);
                assert((page2ppn(page) & (count - 1)) == 0);
                assert(page->page_link.next != &page->page_link);
                assert(page->page_link.next->prev == &page->page_link);
                assert(page->page_link.prev->next == &page->page_link);
                for (size_t j = i; j < i + count; j++) {
                    assert(owner[j] == MODEL_FREE && !covered[j]);
                    covered[j] = 1;
                }
                size_t partner_ppn = page2ppn(page) ^ count;
                if (partner_ppn >= nbase && partner_ppn < npage &&
                    count <= npage - partner_ppn) {
                    struct Page *partner = &test_pages[partner_ppn - nbase];
                    assert(!PageProperty(partner) || partner->property != count);
                }
            } else {
                assert(page->property == 0);
            }
        } else {
            assert(owner[i] >= 0 && owner[i] < TEST_PAGES);
            size_t head = owner[i];
            assert(requests[head] > 0);
            assert(i >= head && i - head < rounded_pages(requests[head]));
            assert(!PageReserved(page) && !PageProperty(page));
            if (i == head) {
                assert((page->flags & ALLOCATED_HEAD) && page->property == requests[head]);
            } else {
                assert(!(page->flags & ALLOCATED_HEAD) && page->property == 0);
            }
            allocated_count++;
        }
    }
    for (size_t i = 0; i < TEST_PAGES; i++) {
        assert(covered[i] == (owner[i] == MODEL_FREE));
        if (requests[i] != 0) {
            size_t count = rounded_pages(requests[i]);
            assert(count <= TEST_PAGES - i);
            for (size_t j = i; j < i + count; j++) {
                assert(owner[j] == (int)i);
            }
        }
    }
    assert(free_count + allocated_count <= TEST_PAGES);
    assert(free_count == expected_free && nr_free_pages() == expected_free);
}

static void
reset_fixture(void) {
    if (fixture_ready) {
        for (size_t i = 0; i < TEST_PAGES; i++) {
            assert(requests[i] == 0);
        }
    }
    /* Known physical alignment makes exact-block cases deterministic. */
    assert((nbase & (TEST_PAGES - 1)) == 0);
    pages = test_pages;
    npage = nbase + TEST_PAGES;
    buddy_pmm_manager.init();
    for (size_t i = 0; i < TEST_PAGES; i++) {
        test_pages[i].flags = 0;
        SetPageReserved(&test_pages[i]);
        test_pages[i].property = 0;
        set_page_ref(&test_pages[i], 0);
        list_init(&test_pages[i].page_link);
        owner[i] = MODEL_RESERVED;
        requests[i] = 0;
    }
    expected_free = 0;
    fixture_ready = 1;
    check_model();
}

static void
register_region(size_t first, size_t count) {
    assert(first < TEST_PAGES && count > 0 && count <= TEST_PAGES - first);
    for (size_t i = first; i < first + count; i++) {
        assert(owner[i] == MODEL_RESERVED);
    }
    buddy_pmm_manager.init_memmap(&test_pages[first], count);
    for (size_t i = first; i < first + count; i++) {
        owner[i] = MODEL_FREE;
    }
    expected_free += count;
    check_model();
}

static struct Page *
take_pages(size_t request) {
    size_t count = rounded_pages(request);
    struct Page *page = alloc_pages(request);
    assert(page != NULL);
    size_t first = test_index(page);
    assert(count <= TEST_PAGES - first && expected_free >= count);
    assert((page2ppn(page) & (count - 1)) == 0);
    for (size_t i = first; i < first + count; i++) {
        assert(owner[i] == MODEL_FREE);
        owner[i] = first;
    }
    requests[first] = request;
    expected_free -= count;
    check_model();
    return page;
}

static void
give_pages(struct Page *page) {
    size_t first = test_index(page);
    size_t request = requests[first];
    assert(request > 0 && owner[first] == (int)first);
    size_t count = rounded_pages(request);
    free_pages(page, request);
    for (size_t i = first; i < first + count; i++) {
        assert(owner[i] == (int)first);
        owner[i] = MODEL_FREE;
    }
    requests[first] = 0;
    expected_free += count;
    check_model();
}

static void
expect_failure(size_t request) {
    assert(alloc_pages(request) == NULL);
    check_model();
}

static void
drain_and_return(size_t count) {
    struct Page *allocated[TEST_PAGES];
    assert(expected_free == count && count <= TEST_PAGES);
    for (size_t i = 0; i < count; i++) {
        allocated[i] = take_pages(1);
    }
    expect_failure(1);
    for (size_t i = count; i > 0; i--) {
        give_pages(allocated[i - 1]);
    }
    assert(expected_free == count);
}

static void
test_init_memmap(void) {
    reset_fixture();
    register_region(0, 16);
    drain_and_return(16);
    cprintf("[Buddy test] PASS init_aligned\n");

    reset_fixture();
    register_region(3, 13);
    drain_and_return(13);
    cprintf("[Buddy test] PASS init_unaligned_13\n");

    reset_fixture();
    register_region(0, 8);
    register_region(8, 8);
    struct Page *whole = take_pages(16);
    assert(whole == test_pages);
    give_pages(whole);
    drain_and_return(16);
    cprintf("[Buddy test] PASS init_adjacent_regions\n");

    reset_fixture();
    register_region(0, 4);
    register_region(8, 4);
    expect_failure(8);
    drain_and_return(8);
    cprintf("[Buddy test] PASS init_reserved_gap\n");
}

static void
test_split_merge(void) {
    reset_fixture();
    register_region(0, 16);
    struct Page *whole = take_pages(16);
    assert(whole == test_pages);
    expect_failure(1);
    give_pages(whole);
    struct Page *single[16] = {NULL};
    for (size_t i = 0; i < 16; i++) {
        struct Page *page = take_pages(1);
        size_t index = test_index(page);
        assert(index < 16 && single[index] == NULL);
        single[index] = page;
    }
    give_pages(single[1]);
    give_pages(single[2]);
    expect_failure(2); // Adjacent pages 1 and 2 are not buddies.
    give_pages(single[0]);
    give_pages(single[3]);
    struct Page *four = take_pages(4);
    assert(four == test_pages);
    give_pages(four);
    for (size_t i = 16; i > 4; i--) {
        give_pages(single[i - 1]);
    }
    whole = take_pages(16);
    assert(whole == test_pages);
    give_pages(whole);
    cprintf("[Buddy test] PASS split_merge_nonbuddy\n");
}

static void
test_rounding(void) {
    const size_t sizes[] = {1, 2, 3, 5, 8, 9};
    const size_t actual[] = {1, 2, 4, 8, 8, 16};
    const unsigned int release_order[] = {3, 0, 5, 1, 4, 2};
    struct Page *allocated[6];
    reset_fixture();
    register_region(0, TEST_PAGES);
    for (size_t i = 0; i < 6; i++) {
        size_t before = expected_free;
        allocated[i] = take_pages(sizes[i]);
        assert(before - expected_free == actual[i]);
    }
    for (size_t i = 0; i < 6; i++) {
        give_pages(allocated[release_order[i]]);
    }
    struct Page *whole = take_pages(TEST_PAGES);
    give_pages(whole);

    reset_fixture();
    register_region(0, 4);
    struct Page *rounded = take_pages(3);
    expect_failure(1); // The fourth page is owned by this allocation too.
    give_pages(rounded);
    whole = take_pages(4);
    give_pages(whole);
    cprintf("[Buddy test] PASS rounding_original_length\n");
}

static void
test_failure(void) {
    reset_fixture();
    expect_failure(1);
    register_region(0, 16);
    expect_failure(17);
    expect_failure((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE + 1);
    expect_failure((size_t)-1);
    expect_failure((size_t)1 << (sizeof(size_t) * 8 - 1));
    struct Page *whole = take_pages(16);
    expect_failure(1);
    give_pages(whole);

    struct Page *single[16] = {NULL};
    for (size_t i = 0; i < 16; i++) {
        struct Page *page = take_pages(1);
        size_t index = test_index(page);
        assert(index < 16 && single[index] == NULL);
        single[index] = page;
    }
    for (size_t i = 0; i < 16; i += 2) {
        give_pages(single[i]);
    }
    assert(expected_free == 8);
    expect_failure(2); // Eight free pages, but no complete buddy pair.
    struct Page *recovered[8];
    for (size_t i = 0; i < 8; i++) {
        recovered[i] = take_pages(1);
        assert(test_index(recovered[i]) % 2 == 0);
    }
    expect_failure(1);
    for (size_t i = 1; i < 16; i += 2) {
        give_pages(single[i]);
    }
    for (size_t i = 0; i < 8; i++) {
        give_pages(recovered[i]);
    }
    whole = take_pages(16);
    give_pages(whole);
    cprintf("[Buddy test] PASS failure_preserves_state\n");
}

static void
test_reuse(void) {
    const size_t sizes[] = {3, 1, 5, 2, 9, 1, 2, 4};
    struct Page *allocated[8];
    reset_fixture();
    register_region(0, TEST_PAGES);
    for (unsigned int round = 0; round < 16; round++) {
        for (size_t i = 0; i < 8; i++) {
            allocated[i] = take_pages(sizes[i]);
        }
        for (size_t i = 0; i < 8; i++) {
            give_pages(allocated[(i * 3 + round) % 8]);
        }
        assert(expected_free == TEST_PAGES);
        struct Page *whole = take_pages(TEST_PAGES);
        assert(whole == test_pages);
        give_pages(whole);
    }
    cprintf("[Buddy test] PASS reuse_16_rounds\n");
}

static void
test_invalid_request(unsigned int case_id) {
    reset_fixture();
    if (case_id == 1) {
        cprintf("[Buddy test] EXPECT_PANIC 1 alloc_zero\n");
        alloc_pages(0);
    } else if (case_id == 2) {
        register_region(0, 16);
        struct Page *page = take_pages(3);
        give_pages(page);
        cprintf("[Buddy test] EXPECT_PANIC 2 double_free\n");
        free_pages(page, 3);
    } else if (case_id == 3) {
        register_region(0, 16);
        struct Page *page = take_pages(3);
        cprintf("[Buddy test] EXPECT_PANIC 3 wrong_length\n");
        free_pages(page, 4);
    } else if (case_id == 4) {
        register_region(0, 16);
        struct Page *page = take_pages(4);
        cprintf("[Buddy test] EXPECT_PANIC 4 interior_free\n");
        free_pages(page + 1, 1);
    } else if (case_id == 5) {
        register_region(0, 8);
        cprintf("[Buddy test] EXPECT_PANIC 5 duplicate_register\n");
        buddy_pmm_manager.init_memmap(test_pages + 4, 4);
    } else if (case_id == 6) {
        cprintf("[Buddy test] EXPECT_PANIC 6 register_out_of_range\n");
        buddy_pmm_manager.init_memmap(test_pages + 60, 8);
    } else {
        panic("BUDDY_TEST_FAIL: unknown case %u", case_id);
    }
    panic("BUDDY_TEST_FAIL: invalid case %u returned", case_id);
}

void
buddy_test_all(void) {
    assert(pmm_manager == &buddy_pmm_manager);
    assert(pages == NULL && npage == 0 && nr_free_pages() == 0);
    struct Page *saved_pages = pages;
    size_t saved_npage = npage;
    if (BUDDY_TEST_CASE != 0) {
        test_invalid_request(BUDDY_TEST_CASE);
        panic("BUDDY_TEST_FAIL: negative test returned");
    }
    test_init_memmap();
    test_split_merge();
    test_rounding();
    test_failure();
    test_reuse();
    for (size_t i = 0; i < TEST_PAGES; i++) {
        assert(requests[i] == 0);
    }
    check_model();
    buddy_pmm_manager.init();
    pages = saved_pages;
    npage = saved_npage;
    fixture_ready = 0;
    assert(nr_free_pages() == 0);
    cprintf("[Buddy test] ALL PASS; fixture restored; continuing real page_init\n");
}
#endif /* BUDDY_TEST */
