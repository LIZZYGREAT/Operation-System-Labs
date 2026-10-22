#include <slub_test.h>

/* Compile out both code and large static control tables in normal builds. */
#if defined(SLUB_TEST) && SLUB_TEST
#include <slub.h>
#include <pmm.h>
#include <stdio.h>

#define SLOT_COUNT 160
#define MIX_ROUNDS 384
/* A capacity bound from the supported physical address range, not an
 * assumption about the number of pages actually free on this boot.
 */
#define HELD_LIMIT ((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE)

struct test_object {
    void *ptr;
    size_t size;
    unsigned int seed;
};

static struct test_object objects[SLOT_COUNT];
static struct Page *held[HELD_LIMIT];
static unsigned int scenario;
static size_t step;
static size_t scenario_baseline;

#define CHECK(condition) do { \
    if (!(condition)) { \
        panic("SLUB case %u step %lu: %s", scenario, step, #condition); \
    } \
} while (0)

static uint8_t
pattern(unsigned int seed, size_t offset) {
    return (uint8_t)(seed * 37U + offset * 13U + (offset >> 8) * 19U);
}

static void
verify_all(void) {
    for (size_t i = 0; i < SLOT_COUNT; i++) {
        if (objects[i].ptr == NULL) {
            continue;
        }
        uintptr_t begin = (uintptr_t)objects[i].ptr;
        uintptr_t end = begin + objects[i].size;
        for (size_t j = i + 1; j < SLOT_COUNT; j++) {
            if (objects[j].ptr != NULL) {
                uintptr_t other = (uintptr_t)objects[j].ptr;
                CHECK(end <= other || other + objects[j].size <= begin);
            }
        }
        volatile uint8_t *bytes = objects[i].ptr;
        for (size_t k = 0; k < objects[i].size; k++) {
            uint8_t expected = pattern(objects[i].seed, k);
            uint8_t actual = bytes[k];
            if (actual != expected) {
                panic("SLUB case %u step %lu slot %lu offset %lu: "
                      "expected %u actual %u", scenario, step, i, k,
                      (unsigned int)expected, (unsigned int)actual);
            }
        }
    }
}

static void
take(size_t slot, size_t size, unsigned int seed) {
    step++;
    CHECK(slot < SLOT_COUNT && objects[slot].ptr == NULL);
    CHECK(size > 0);
    void *ptr = slub_alloc(size);
    CHECK(ptr != NULL);
    uintptr_t begin = (uintptr_t)ptr;
    CHECK(begin % 16 == 0);
    CHECK(begin >= KERNBASE && begin < KERNTOP);
    CHECK(size <= KERNTOP - begin);
    /* Check all ranges before writing: even an alias must not overwrite a
     * live object before the test has a chance to diagnose the overlap.
     */
    for (size_t i = 0; i < SLOT_COUNT; i++) {
        if (objects[i].ptr != NULL) {
            uintptr_t other = (uintptr_t)objects[i].ptr;
            CHECK(begin + size <= other || other + objects[i].size <= begin);
        }
    }
    objects[slot].ptr = ptr;
    objects[slot].size = size;
    objects[slot].seed = seed;
    volatile uint8_t *bytes = ptr;
    for (size_t k = 0; k < size; k++) {
        bytes[k] = pattern(seed, k);
    }
    verify_all();
}

static void
drop(size_t slot) {
    step++;
    CHECK(slot < SLOT_COUNT && objects[slot].ptr != NULL);
    verify_all();
    slub_free(objects[slot].ptr);
    objects[slot].ptr = NULL;
    verify_all();
}

static void
begin_case(unsigned int number, const char *expectation) {
    scenario = number;
    step = 0;
    for (size_t i = 0; i < SLOT_COUNT; i++) {
        CHECK(objects[i].ptr == NULL);
    }
    scenario_baseline = nr_free_pages();
    cprintf("[SLUB %u] BEGIN: %s\n", number, expectation);
}

static void
end_case(void) {
    for (size_t i = 0; i < SLOT_COUNT; i++) {
        if (objects[i].ptr != NULL) {
            drop(i);
        }
    }
    size_t actual = nr_free_pages();
    if (actual != scenario_baseline) {
        panic("SLUB case %u: free pages expected %lu actual %lu",
              scenario, scenario_baseline, actual);
    }
    cprintf("[SLUB %u] PASS: free pages restored to %lu\n",
            scenario, actual);
}

static void
check_null(void) {
    begin_case(1, "zero/NULL preserve pages and later allocations");
    CHECK(slub_alloc(0) == NULL);
    slub_free(NULL);
    CHECK(nr_free_pages() == scenario_baseline);
    take(0, 31, 1);
    size_t count = nr_free_pages();
    slub_free(NULL);
    CHECK(slub_alloc(0) == NULL);
    CHECK(nr_free_pages() == count);
    verify_all();
    take(1, 31, 2);
    end_case();
}

static void
check_boundaries(void) {
    static const size_t sizes[] = {
        1, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513,
        1023, 1024, 1025, 2047, 2048, 2049,
    };
    begin_case(2, "all class boundaries: aligned, full-range readable/writable");
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        take(i, sizes[i], 10 + i);
    }
    end_case();
}

static void
check_isolation(void) {
    static const size_t sizes[] = {7, 23, 61, 127, 250, 500, 1000, 2000,
                                   2049, PGSIZE + 1, 2 * PGSIZE + 37};
    begin_case(3, "mixed live ranges disjoint; survivors retain every byte");
    for (size_t i = 0; i < 11; i++) {
        take(i, sizes[i], 50 + i);
    }
    for (size_t i = 0; i < 11; i += 2) {
        drop(i);
        take(i, sizes[10 - i], 90 + i);
    }
    end_case();
}

static void
check_reuse(void) {
    begin_case(4, "reuse freed slots while slab stays live; no extra pages");
    for (size_t i = 0; i < 16; i++) {
        take(i, 32, 110 + i);
    }
    CHECK(scenario_baseline - nr_free_pages() == 1);
    uintptr_t page = ROUNDDOWN((uintptr_t)objects[0].ptr, PGSIZE);
    for (size_t i = 1; i < 16; i++) {
        CHECK(ROUNDDOWN((uintptr_t)objects[i].ptr, PGSIZE) == page);
    }
    size_t count = nr_free_pages();
    for (size_t i = 0; i < 16; i += 2) {
        drop(i);
        CHECK(nr_free_pages() == count);
    }
    for (size_t i = 0; i < 16; i += 2) {
        take(i, 32, 150 + i);
        CHECK(nr_free_pages() == count);
    }
    end_case();
}

static void
check_multiple_slabs(void) {
    begin_case(5, "80 x 128 bytes span slabs; 16 x 16 share a page");
    for (size_t i = 0; i < 80; i++) {
        take(i, 128, 170 + i);
    }
    CHECK(80 * 128 > PGSIZE);
    size_t distinct = 0;
    for (size_t i = 0; i < 80; i++) {
        bool seen = 0;
        uintptr_t page = ROUNDDOWN((uintptr_t)objects[i].ptr, PGSIZE);
        for (size_t j = 0; j < i; j++) {
            if (ROUNDDOWN((uintptr_t)objects[j].ptr, PGSIZE) == page) {
                seen = 1;
            }
        }
        if (!seen) {
            distinct++;
        }
    }
    CHECK(distinct > 1);
    CHECK(scenario_baseline - nr_free_pages() == distinct);
    for (size_t i = 0; i < 80; i++) {
        drop((i * 37) % 80); /* permutation: 37 is coprime with 80 */
    }
    CHECK(nr_free_pages() == scenario_baseline);
    for (size_t i = 0; i < 16; i++) {
        take(i, 16, 270 + i);
    }
    CHECK(scenario_baseline - nr_free_pages() == 1);
    end_case();
}

static void
check_reclaim(void) {
    begin_case(6, "partial free keeps slab; final free immediately returns page");
    take(0, 64, 300);
    take(1, 64, 301);
    take(2, 512, 302);
    CHECK(scenario_baseline - nr_free_pages() == 2);
    uintptr_t page = ROUNDDOWN((uintptr_t)objects[0].ptr, PGSIZE);
    CHECK(ROUNDDOWN((uintptr_t)objects[1].ptr, PGSIZE) == page);
    drop(0);
    CHECK(scenario_baseline - nr_free_pages() == 2);
    drop(1);
    CHECK(scenario_baseline - nr_free_pages() == 1);
    drop(2);
    CHECK(nr_free_pages() == scenario_baseline);
    end_case();
}

static void
check_large(void) {
    static const size_t sizes[] = {
        2049, PGSIZE - 1, PGSIZE, PGSIZE + 1, 2 * PGSIZE + 123,
    };
    begin_case(7, "large ranges, contiguous backing and all charged pages freed");
    take(0, 45, 320);
    for (size_t i = 0; i < 5; i++) {
        size_t before = nr_free_pages();
        take(i + 1, sizes[i], 321 + i);
        size_t charged = before - nr_free_pages();
        /* An independent lower bound from payload alone, not SLUB headers. */
        size_t minimum = sizes[i] / PGSIZE + (sizes[i] % PGSIZE != 0);
        CHECK(charged >= minimum);
        uintptr_t first = PADDR(objects[i + 1].ptr);
        uintptr_t last = first + sizes[i] - 1;
        /* Count the pages actually crossed by the returned payload. Unlike
         * a copy of SLUB's header formula this also catches a header pushing
         * a page-sized payload into an uncharged following page.
         */
        CHECK(charged >= PPN(last) - PPN(first) + 1);
        for (uintptr_t pa = ROUNDDOWN(first, PGSIZE);
             pa <= ROUNDDOWN(last, PGSIZE); pa += PGSIZE) {
            CHECK(!PageProperty(pa2page(pa)));
        }
        cprintf("[SLUB 7] size=%lu charged=%lu payload-pages>=%lu\n",
                sizes[i], charged, minimum);
    }
    /* All large objects coexist above. Check each actual credit below;
     * no duplicated implementation formula predicts its allocation size.
     */
    for (size_t i = 1; i <= 5; i++) {
        drop(i);
    }
    for (size_t i = 0; i < 5; i++) {
        size_t before = nr_free_pages();
        take(1, sizes[i], 340 + i);
        size_t charged = before - nr_free_pages();
        drop(1);
        CHECK(nr_free_pages() == before);
        CHECK(charged > 0);
    }
    end_case();
}

static void
expect_failure(size_t size) {
    step++;
    size_t before = nr_free_pages();
    CHECK(slub_alloc(size) == NULL);
    CHECK(nr_free_pages() == before);
    verify_all();
}

static void
check_failures(void) {
    begin_case(8, "oversize/overflow plus real PMM exhaustion; rollback and recovery");
    take(0, 64, 360);
    take(1, PGSIZE + 1, 361);
    size_t available = nr_free_pages();
    CHECK(available < ((size_t)-1 / PGSIZE) - 2);
    expect_failure((available + 2) * PGSIZE);
    expect_failure((size_t)-1);
    expect_failure((size_t)-1 - 15);
    expect_failure((size_t)-1 - PGSIZE);
    take(2, 129, 362);
    drop(2);

    /* No SLUB internals are changed. Reserve every remaining physical page
     * via the public PMM API, recording every Page in a bounded static table.
     */
    size_t count = 0;
    size_t before_hold = nr_free_pages();
    CHECK(before_hold <= HELD_LIMIT);
    while (count < before_hold) {
        step++;
        CHECK(count < HELD_LIMIT);
        held[count] = alloc_pages(1);
        CHECK(held[count] != NULL);
        count++;
    }
    CHECK(nr_free_pages() == 0);
    CHECK(alloc_pages(1) == NULL);
    expect_failure(128); /* unused class must request a new slab page */
    expect_failure(PGSIZE + 1);
    /* Existing partial slab must remain usable even without free pages. */
    take(2, 64, 363);
    CHECK(nr_free_pages() == 0);
    drop(2);
    CHECK(nr_free_pages() == 0);
    CHECK(count > 0);
    free_pages(held[--count], 1);
    held[count] = NULL;
    CHECK(nr_free_pages() == 1);
    expect_failure(2 * PGSIZE + 1); /* genuinely insufficient nonzero supply */
    take(2, 128, 364);
    CHECK(nr_free_pages() == 0);
    drop(2);
    CHECK(nr_free_pages() == 1);
    while (count > 0) {
        free_pages(held[--count], 1);
        held[count] = NULL;
    }
    CHECK(nr_free_pages() == before_hold);
    cprintf("[SLUB 8] PMM exhaustion exercised: held=%lu, small/large failures verified\n",
            before_hold);
    take(2, 2 * PGSIZE + 1, 365);
    end_case();
}

static void
check_mixed(void) {
    static const size_t sizes[] = {
        1, 16, 17, 32, 33, 64, 65, 128, 129, 256, 257,
        512, 513, 1024, 1025, 2048, 2049, PGSIZE, PGSIZE + 1,
        2 * PGSIZE + 17,
    };
    begin_case(9, "384 deterministic mixed operations with live-data checks");
    for (size_t i = 0; i < MIX_ROUNDS; i++) {
        size_t slot = (i * 17 + i / 32) % 32;
        if (objects[slot].ptr != NULL) {
            drop(slot);
        }
        if (i % 4 != 0) {
            size_t index = (i * 7 + i / 20) % (sizeof(sizes) / sizeof(sizes[0]));
            take(slot, sizes[index], 400 + i);
        }
        verify_all();
    }
    end_case();
}

void
slub_check(void) {
    size_t baseline = nr_free_pages();
    cprintf("[SLUB] START: baseline=%lu, slots=%u, mixed-rounds=%u\n",
            baseline, SLOT_COUNT, MIX_ROUNDS);
    check_null();
    check_boundaries();
    check_isolation();
    check_reuse();
    check_multiple_slabs();
    check_reclaim();
    check_large();
    check_failures();
    check_mixed();
    CHECK(nr_free_pages() == baseline);
    cprintf("[SLUB] ALL 9 CASES PASSED: baseline=%lu final=%lu\n",
            baseline, nr_free_pages());
}
#endif /* SLUB_TEST */
