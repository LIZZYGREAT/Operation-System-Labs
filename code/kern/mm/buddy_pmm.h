#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

/* lab2 Buddy System - 2412712
 * alloc_pages(n) reserves the smallest power-of-two block containing n pages.
 * free_pages(base, n) must use the original request n and frees the whole block.
 * Partial frees are unsupported. Intended for single-hart, serial execution.
 */
extern const struct pmm_manager buddy_pmm_manager;

#endif /* !__KERN_MM_BUDDY_PMM_H__ */
