#if !defined(LINUX_HACKS_H)
#define LINUX_HACKS_H

/*
 * model_brk - Hard breakpoint for the Fast Model debugger.
 * c_out - output a char using Model Semi-hosting.
 * s_out - output a string using Model Semi-hosting.
 */

#if defined(__ASSEMBLY__)

.macro	model_brk
	mov	x1, #0x20000; 
	add	x1, x1, #0x20; /* ADP_Stopped_BreakPoint */
	mov	x0, #0x18;     /* angel_SWIreason_ReportException */
	hlt	#0xf000        /* A64 semihosting */
.endm

.macro	c_out, a
	adr	x1, .debug_str
	mov	w2, \a
	strb	w2, [x1]
	mov	x0, #3
	hlt	#0xf000
.endm

.macro	s_out, a
	adr	x1, \a
	mov	x0, #4			/* SYS_WRITE0 */
	hlt	#0xf000			/* A64 semihosting */
.endm

.macro	mmu_check, a
	c_out	#(\a + '0')
	mrs	x1, sctlr_el1
	tbz	x1, #0, 1f
	s_out	.debug_y
	b	2f
1:
	s_out	.debug_n
2:
.endm

.macro	out_data

	.align 3

	.debug_y:
		.string	" debug_y\n"
	.debug_n:
		.string	" debug_n\n"

	.align 3, 0
	.debug_str:
		.quad	0
		.quad	0
		.quad	0
		.quad	0
.endm

#else

#include <linux/kernel.h>

static inline void model_brk(void)
{
	asm volatile("ldr x1, =0x20020;" /* ADP_Stopped_BreakPoint */
		     "mov x0, #0x18;"    /* angel_SWIreason_ReportException */
		     "hlt  0xf000\n"     /* A64 semihosting */
		     : : : "x0", "x1");
}

static inline void c_out(char c)
{
	asm volatile("mov  x1, %0\n"
		     "mov  x0, #3\n"
		     "hlt  0xf000\n"
		     : : "r" (&c) : "x0", "x1", "memory");
}

static inline void s_out(const char *s)
{
	asm volatile("mov  x1, %0\n"
		     "mov  x0, #4\n"
		     "hlt  0xf000\n"
		     : : "r" (s) : "x0", "x1", "memory");
}

#define S(x) #x
#define S_(x) S(x)

#define line_out(_s) s_out(__func__); s_out(":" S_(__LINE__) ": " _s)

#define print_sctlr_el1() _print_sctlr_el1(__func__, __LINE__)
static inline void _print_sctlr_el1(const char *func, int line)
{
	u32 val;

	asm volatile("mrs %0,  sctlr_el1" : "=r" (val));

	printk("%s:%d: sctlr_el1: %x - %s\n", func, line, val, (val & 1) ? "y" : "n");
}

#endif
#endif /* LINUX_HACKS_H */