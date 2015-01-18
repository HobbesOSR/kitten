#ifndef _ASM_GENERIC_BITOPS_ATOMIC_H_
#define _ASM_GENERIC_BITOPS_ATOMIC_H_

#include <arch/types.h>
#include <arch/fls.h>
#include <arch/fls64.h>
#include <arch/ffs.h>

#define ffz(x)  __ffs(~(x))

// TODO Brian this is stop gap until we can refactor the large number of header files to correct the issues
// Current issue is that arch-arm64/bitops.h requres spinlocks unlike the previous x86_64 code. This
// is causing an ordering issues with several types and functions.
/*static inline void set_bit(int nr, volatile unsigned long *addr);
static inline void clear_bit(int nr, volatile unsigned long *addr);
static inline void change_bit(int nr, volatile unsigned long *addr);
static inline int test_and_set_bit(int nr, volatile unsigned long *addr);
static inline int test_and_clear_bit(int nr, volatile unsigned long *addr);
static inline int test_and_change_bit(int nr, volatile unsigned long *addr);
*/
// TODO this is a nasty fix, but the way bit ops are implemented on ARM is extremely different than on x86 causing issues is LWK
#ifndef ARM_RAW_SPINLOCK
#define ARM_RAW_SPINLOCK
typedef struct _raw_spinlock_t {
	volatile unsigned int slock;
} raw_spinlock_t;
#endif
#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)

#ifdef CONFIG_SMP
//#include <lwk/spinlock.h>
#include <arch/cache.h>		/* we use L1_CACHE_BYTES */

/* Use an array of spinlocks for our atomic_ts.
 * Hash function to index into a different SPINLOCK.
 * Since "a" is usually an address, use one spinlock per cacheline.
 */
#  define ATOMIC_HASH_SIZE 4
#  define ATOMIC_HASH(a) (&(__atomic_hash[ (((unsigned long) a)/L1_CACHE_BYTES) & (ATOMIC_HASH_SIZE-1) ]))

extern raw_spinlock_t __atomic_hash[ATOMIC_HASH_SIZE];// __lock_aligned;

/* Can't use raw_spin_lock_irq because of #include problems, so
 * this is the substitute */
#define _atomic_spin_lock_irqsave(l,f) do {	\
	spinlock_t *s = ATOMIC_HASH(l);	\
	local_irq_save(f);			\
	_raw_spin_lock(s);			\
} while(0)

#define _atomic_spin_unlock_irqrestore(l,f) do {	\
	spinlock_t *s = ATOMIC_HASH(l);		\
	_raw_spin_unlock(s);				\
	local_irq_restore(f);				\
} while(0)


#else
#  define _atomic_spin_lock_irqsave(l,f) do { local_irq_save(f); } while (0)
#  define _atomic_spin_unlock_irqrestore(l,f) do { local_irq_restore(f); } while (0)
#endif

#define __set_bit(nr,addr) \
	set_bit(nr,addr)
//void set_bit(int nr, volatile unsigned long *addr);

#define __clear_bit(nr, addr) \
	clear_bit(nr,addr)
//void clear_bit(int nr, volatile unsigned long *addr);
//void change_bit(int nr, volatile unsigned long *addr);
//int test_bit(int nr, volatile unsigned long *addr);
//int test_and_set_bit(int nr, volatile unsigned long *addr);
//int test_and_clear_bit(int nr, volatile unsigned long *addr);
//int test_and_change_bit(int nr, volatile unsigned long *addr);

#endif /* _ASM_GENERIC_BITOPS_ATOMIC_H */
