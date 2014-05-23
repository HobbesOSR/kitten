/* Pisces Cross OS Spinlock implementation 
 *  (c) 2013, Jiannan Ouyang
 */


#include <arch/pisces/pisces_lock.h>



/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
static inline void pisces_cpu_relax(void) {
	__asm__ __volatile__("rep;nop": : :"memory");
}

/*
 * Note: no "lock" prefix even on SMP: xchg always implies lock anyway
 * Note 2: xchg has side effect, so that attribute volatile is necessary,
 *	  but generally the primitive is invalid, *ptr is output argument. --ANK
 */
static inline unsigned long pisces_xchg8(volatile void * ptr, unsigned char x ) {
  __asm__ __volatile__("xchgb %0,%1"
                       :"=r" (x)
                       :"m" (*(volatile unsigned char *)ptr), "0" (x)
                       :"memory");
  return x;
}



void pisces_lock_init(struct pisces_spinlock * lock) {
    lock->raw_lock = 0;
}


void pisces_spin_lock(struct pisces_spinlock * lock) {

    while (1) {
	if (pisces_xchg8(&(lock->raw_lock), 1) == 0) {
	    return;
	}

	while (lock->raw_lock) pisces_cpu_relax();
    }
}


void pisces_spin_unlock(struct pisces_spinlock * lock) {
    __asm__ __volatile__ ("": : :"memory");
    lock->raw_lock = 0;
}
