/*
 * kexec for arm64
 *
 * Copyright (C) Linaro.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/assembler.h>
#include <asm/memory.h>
#include <asm/page.h>

/* The list entry flags. */

#define IND_DESTINATION_BIT 0
#define IND_INDIRECTION_BIT 1
#define IND_DONE_BIT        2
#define IND_SOURCE_BIT      3

/*
 * relocate_new_kernel - Put the 2nd stage kernel image in place and boot it.
 *
 * The memory that the old kernel occupies may be overwritten when coping the
 * new kernel to its final location.  To assure that the relocate_new_kernel
 * routine which does that copy is not overwritten all code and data needed
 * by relocate_new_kernel must be between the symbols relocate_new_kernel and
 * relocate_new_kernel_end.  The machine_kexec() routine will copy
 * relocate_new_kernel to the kexec control_code_page, a special page which
 * has been set up to be preserved during the kernel copy operation.
 */

.align 3

.globl relocate_new_kernel
relocate_new_kernel:

	/* Setup the list loop variables. */

	ldr	x10, kexec_kimage_head		/* x10 = list entry */

	mrs	x0, ctr_el0
	ubfm	x0, x0, #16, #19
	mov	x11, #4
	lsl	x11, x11, x0			/* x11 = dcache line size */

	mov	x12, xzr			/* x12 = segment start */
	mov	x13, xzr			/* x13 = entry ptr */
	mov	x14, xzr			/* x14 = copy dest */

	/* Check if the new kernel needs relocation. */

	cbz	x10, .Ldone
	tbnz	x10, IND_DONE_BIT, .Ldone

.Lloop:
	and	x15, x10, PAGE_MASK		/* x15 = addr */

	/* Test the entry flags. */

.Ltest_source:
	tbz	x10, IND_SOURCE_BIT, .Ltest_indirection

	/* copy_page(x20 = dest, x21 = src) */

	mov x20, x14
	mov x21, x15

1:	ldp	x22, x23, [x21]
	ldp	x24, x25, [x21, #16]
	ldp	x26, x27, [x21, #32]
	ldp	x28, x29, [x21, #48]
	add	x21, x21, #64
	stnp	x22, x23, [x20]
	stnp	x24, x25, [x20, #16]
	stnp	x26, x27, [x20, #32]
	stnp	x28, x29, [x20, #48]
	add	x20, x20, #64
	tst	x21, #(PAGE_SIZE - 1)
	b.ne	1b

	/* dest += PAGE_SIZE */

	add	x14, x14, PAGE_SIZE
	b	.Lnext

.Ltest_indirection:
	tbz	x10, IND_INDIRECTION_BIT, .Ltest_destination

	/* ptr = addr */

	mov	x13, x15
	b	.Lnext

.Ltest_destination:
	tbz	x10, IND_DESTINATION_BIT, .Lnext

	/* flush segment */

	bl	.Lflush
	mov	x12, x15

	/* dest = addr */

	mov	x14, x15

.Lnext:
	/* entry = *ptr++ */

	ldr	x10, [x13]
	add	x13, x13, 8

	/* while (!(entry & DONE)) */

	tbz	x10, IND_DONE_BIT, .Lloop

.Ldone:
	/* flush last segment */

	bl	.Lflush

	dsb	sy
	isb
	ic	ialluis

	/* start_new_kernel */

	ldr	x4, kexec_kimage_start
	ldr	x0, kexec_dtb_addr
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr
	br	x4

/* flush - x11 = line size, x12 = start addr, x14 = end addr. */

.Lflush:
	cbz	x12, 2f
	mov	x0, x12
	sub	x1, x11, #1
	bic	x0, x0, x1
1:	dc	civac, x0
	add	x0, x0, x11
	cmp	x0, x14
	b.lo	1b
2:	ret

.align 3

/* The machine_kexec routines set these variables. */

/*
 * kexec_dtb_addr - Physical address of the new kernel's device tree.
 */

.globl kexec_dtb_addr
kexec_dtb_addr:
	.quad	0x0

/*
 * kexec_kimage_head - Copy of image->head, the list of kimage entries.
 */

.globl kexec_kimage_head
kexec_kimage_head:
	.quad	0x0

/*
 * kexec_kimage_start - Copy of image->start, the entry point of the new kernel.
 */

.globl kexec_kimage_start
kexec_kimage_start:
	.quad	0x0

.Lrelocate_new_kernel_end:

/*
 * relocate_new_kernel_size - Number of bytes to copy to the control_code_page.
 */

.globl relocate_new_kernel_size
relocate_new_kernel_size:
	.quad .Lrelocate_new_kernel_end - relocate_new_kernel
