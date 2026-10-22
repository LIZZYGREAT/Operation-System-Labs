#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

/* Single-core, serial callers only. Call once after pmm_init(), before use.
 * Initialization allocates no pages. Returned memory is not zeroed and is
 * aligned to at least 16 bytes; size 0 and allocation failure return NULL.
 */
void slub_init(void);
void *slub_alloc(size_t size);

/* NULL is harmless. Otherwise ptr must be a live, original allocation pointer.
 * Empty small-object slabs and complete large allocations are freed at once.
 */
void slub_free(void *ptr);

#endif /* !__KERN_MM_SLUB_H__ */
