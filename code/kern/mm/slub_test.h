#ifndef __KERN_MM_SLUB_TEST_H__
#define __KERN_MM_SLUB_TEST_H__

/* Build with -DSLUB_TEST=1. PMM and SLUB must already be initialized;
 * there must be no other live SLUB objects or concurrent PMM users.
 */
void slub_check(void);

#endif /* !__KERN_MM_SLUB_TEST_H__ */
