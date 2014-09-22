/*
 * Based on arch/arm/include/asm/assembler.h
 *
 * Copyright (C) 1996-2000 Russell King
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
#ifndef __ASSEMBLY__
#error "Only include this from assembly code"
#endif

#include <asm/ptrace.h>
#include <asm/thread_info.h>

/**
 * macro atomic - Macros for simple atomic operations.
 *
 * @op: The operation to perform.
 * @reg: A 32 or 64 bit register to operate on.
 * @value: The value for the operation.
 * @tmp1:  A scratch register the same size as @reg.  Defaluts to x20.
 * @tmp2: A 32 bit scratch register.  Defaluts to w21.
 **/

	.macro	atomic	op:req, reg:req, value:req, tmp1=x20, tmp2=w21
.Latomic\@:	ldxr	\tmp1, [\reg]
	\op	\tmp1, \tmp1, \value
	stxr	\tmp2, \tmp1, [\reg]
	cbnz	\tmp2, .Latomic\@
	.endm

	.macro	atomic_add	reg:req, value:req, tmp1=x20, tmp2=w21
	atomic add, \reg, \value, \tmp1, \tmp2
	.endm

	.macro	atomic_sub	reg:req, value:req, tmp1, tmp2
	atomic sub, \reg, \value, \tmp1, \tmp2
	.endm

	.macro	atomic_inc	reg:req, tmp1, tmp2
	atomic add, \reg, 1, \tmp1, \tmp2
	.endm

	.macro	atomic_dec	reg:req, tmp1, tmp2
	atomic sub, \reg, 1, \tmp1, \tmp2
	.endm

/*
 * Stack pushing/popping (register pairs only). Equivalent to store decrement
 * before, load increment after.
 */
	.macro	push, xreg1, xreg2
	stp	\xreg1, \xreg2, [sp, #-16]!
	.endm

	.macro	pop, xreg1, xreg2
	ldp	\xreg1, \xreg2, [sp], #16
	.endm

/*
 * Enable and disable interrupts.
 */
	.macro	disable_irq
	msr	daifset, #2
	.endm

	.macro	enable_irq
	msr	daifclr, #2
	.endm

/*
 * Save/disable and restore interrupts.
 */
	.macro	save_and_disable_irqs, olddaif
	mrs	\olddaif, daif
	disable_irq
	.endm

	.macro	restore_irqs, olddaif
	msr	daif, \olddaif
	.endm

/*
 * Enable and disable debug exceptions.
 */
	.macro	disable_dbg
	msr	daifset, #8
	.endm

	.macro	enable_dbg
	msr	daifclr, #8
	.endm

	.macro	disable_step_tsk, flgs, tmp
	tbz	\flgs, #TIF_SINGLESTEP, 9990f
	mrs	\tmp, mdscr_el1
	bic	\tmp, \tmp, #1
	msr	mdscr_el1, \tmp
	isb	// Synchronise with enable_dbg
9990:
	.endm

	.macro	enable_step_tsk, flgs, tmp
	tbz	\flgs, #TIF_SINGLESTEP, 9990f
	disable_dbg
	mrs	\tmp, mdscr_el1
	orr	\tmp, \tmp, #1
	msr	mdscr_el1, \tmp
9990:
	.endm

/*
 * Enable both debug exceptions and interrupts. This is likely to be
 * faster than two daifclr operations, since writes to this register
 * are self-synchronising.
 */
	.macro	enable_dbg_and_irq
	msr	daifclr, #(8 | 2)
	.endm

/*
 * SMP data memory barrier
 */
	.macro	smp_dmb, opt
#ifdef CONFIG_SMP
	dmb	\opt
#endif
	.endm

#define USER(l, x...)				\
9999:	x;					\
	.section __ex_table,"a";		\
	.align	3;				\
	.quad	9999b,l;			\
	.previous

/*
 * Register aliases.
 */
lr	.req	x30		// link register

/*
 * Vector entry
 */
	 .macro	ventry	label
	.align	7
	b	\label
	.endm

/*
 * Select code when configured for BE.
 */
#ifdef CONFIG_CPU_BIG_ENDIAN
#define CPU_BE(code...) code
#else
#define CPU_BE(code...)
#endif

/*
 * Select code when configured for LE.
 */
#ifdef CONFIG_CPU_BIG_ENDIAN
#define CPU_LE(code...)
#else
#define CPU_LE(code...) code
#endif

/*
 * Define a macro that constructs a 64-bit value by concatenating two
 * 32-bit registers. Note that on big endian systems the order of the
 * registers is swapped.
 */
#ifndef CONFIG_CPU_BIG_ENDIAN
	.macro	regs_to_64, rd, lbits, hbits
#else
	.macro	regs_to_64, rd, hbits, lbits
#endif
	orr	\rd, \lbits, \hbits, lsl #32
	.endm
