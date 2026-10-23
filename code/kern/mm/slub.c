#include <slub.h>
#include <pmm.h>
#include <list.h>

/* Teaching allocator: one page per small-object slab, in-page metadata,
 * and an intrusive free list stored only in currently unused object slots.
 * No Page descriptor fields are borrowed from the physical allocator.
 */
#define SLUB_ALIGNMENT 16
#define SLUB_CLASS_COUNT 8
#define SLUB_LARGE SLUB_CLASS_COUNT

static const size_t class_sizes[SLUB_CLASS_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048,
};

struct slub_header {
    size_t class_index;
    size_t npages;
    size_t inuse;
    size_t capacity;
    void *free_head;
    list_entry_t partial_link;
};

/* Reserve an aligned prefix. Even large-object payloads start in page one,
 * so rounding a valid returned pointer down to a page recovers its header.
 */
#define SLUB_HEADER_SIZE \
    ((sizeof(struct slub_header) + SLUB_ALIGNMENT - 1) / SLUB_ALIGNMENT * \
     SLUB_ALIGNMENT)

/* Only slabs with available slots are linked here; full slabs are unlinked. */
static list_entry_t partial[SLUB_CLASS_COUNT];
static bool initialized;

static struct slub_header *
slub_new_slab(size_t class_index) {
    struct Page *page = alloc_pages(1);
    if (page == NULL) {
        return NULL;
    }

    struct slub_header *slab =
        (struct slub_header *)(page2pa(page) + va_pa_offset);
    size_t object_size = class_sizes[class_index];
    slab->class_index = class_index;
    slab->npages = 1;
    slab->inuse = 0;
    slab->capacity = (PGSIZE - SLUB_HEADER_SIZE) / object_size;
    slab->free_head = NULL;
    list_init(&slab->partial_link);

    for (size_t i = 0; i < slab->capacity; i++) {
        void *slot = (char *)slab + SLUB_HEADER_SIZE + i * object_size;
        *(void **)slot = slab->free_head;
        slab->free_head = slot;
    }
    /* No fallible operations follow page allocation: publish only when ready. */
    list_add(&partial[class_index], &slab->partial_link);
    return slab;
}

void
slub_init(void) {
    static_assert(SLUB_HEADER_SIZE < PGSIZE);
    static_assert(PGSIZE % SLUB_ALIGNMENT == 0);
    static_assert(sizeof(void *) <= SLUB_ALIGNMENT);
    assert(!initialized);
    for (size_t i = 0; i < SLUB_CLASS_COUNT; i++) {
        list_init(&partial[i]);
    }
    initialized = 1;
}

void *
slub_alloc(size_t size) {
    assert(initialized);
    if (size == 0) {
        return NULL;
    }

    if (size > class_sizes[SLUB_CLASS_COUNT - 1]) {
        if (size > (size_t)-1 - SLUB_HEADER_SIZE) {
            return NULL;
        }
        size_t total = size + SLUB_HEADER_SIZE;
        /* Division plus remainder avoids overflow in total + PGSIZE - 1. */
        size_t npages = total / PGSIZE + (total % PGSIZE != 0);
        struct Page *page = alloc_pages(npages);
        if (page == NULL) {
            return NULL;
        }
        struct slub_header *header =
            (struct slub_header *)(page2pa(page) + va_pa_offset);
        header->class_index = SLUB_LARGE;
        header->npages = npages;
        return (char *)header + SLUB_HEADER_SIZE;
    }

    size_t class_index = 0;
    while (class_sizes[class_index] < size) {
        class_index++;
    }
    struct slub_header *slab;
    if (list_empty(&partial[class_index])) {
        slab = slub_new_slab(class_index);
        if (slab == NULL) {
            return NULL;
        }
    } else {
        slab = to_struct(list_next(&partial[class_index]),
                         struct slub_header, partial_link);
    }

    void *object = slab->free_head;
    slab->free_head = *(void **)object;
    slab->inuse++;
    if (slab->inuse == slab->capacity) {
        list_del_init(&slab->partial_link);
    }
    return object;
}

void
slub_free(void *ptr) {
    assert(initialized);
    if (ptr == NULL) {
        return;
    }

    struct slub_header *slab =
        (struct slub_header *)ROUNDDOWN((uintptr_t)ptr, PGSIZE);
    if (slab->class_index == SLUB_LARGE) {
        /* Read both values before returning the entire allocation. */
        struct Page *page = pa2page(PADDR(slab));
        size_t npages = slab->npages;
        free_pages(page, npages);
        return;
    }

    bool was_full = (slab->inuse == slab->capacity);
    slab->inuse--;
    if (slab->inuse == 0) {
        if (!was_full) {
            list_del(&slab->partial_link);
        }
        free_pages(pa2page(PADDR(slab)), 1);
        return;
    }

    *(void **)ptr = slab->free_head;
    slab->free_head = ptr;
    if (was_full) {
        list_add(&partial[slab->class_index], &slab->partial_link);
    }
}
