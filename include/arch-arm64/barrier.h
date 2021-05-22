/*
 * Based on arch/arm/include/asm/barrier.h
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_BARRIER_H
#define __ASM_BARRIER_H

#ifndef __ASSEMBLY__

#define sev()		asm volatile("sev" : : : "memory")
#define wfe()		asm volatile("wfe" : : : "memory")
#define wfi()		asm volatile("wfi" : : : "memory")

#define isb()		asm volatile("isb" : : : "memory")
//#define dsb()		asm volatile("dsb sy" : : : "memory")

#define dmb(opt)	asm volatile("dmb " #opt : : : "memory")
#define dsb(opt)	asm volatile("dsb " #opt : : : "memory")

#define mb()		dsb(sy)
#define rmb()		asm volatile("dsb ld" : : : "memory")
#define wmb()		asm volatile("dsb st" : : : "memory")


#define smp_mb()	asm volatile("dmb ish" : : : "memory")
#define smp_rmb()	asm volatile("dmb ishld" : : : "memory")
#define smp_wmb()	asm volatile("dmb ishst" : : : "memory")

#define read_barrier_depends()		do { } while(0)
#define smp_read_barrier_depends()	do { } while(0)

#define set_wmb(var, value)     do { var = value; wmb(); } while (0)
#define set_mb(var, value)	do { var = value; smp_mb(); } while (0)
#define nop()		asm volatile("nop");

#endif	/* __ASSEMBLY__ */

#endif	/* __ASM_BARRIER_H */
