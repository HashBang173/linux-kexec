/*
 * Copyright (C) Linaro.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(__ARM64_CPU_PROPERTIES_H)
#define __ARM64_CPU_PROPERTIES_H

#include <asm/memory.h>
#include <asm/cputype.h>

#define INVALID_ADDR UL(~0)

#if !defined(__ASSEMBLY__)

#include <linux/kernel.h>
#include <linux/of.h>

enum cpu_enable_method {
	cpu_enable_method_unknown,
	cpu_enable_method_psci,
	cpu_enable_method_spin_table,
};

struct cpu_properties {
	u64 hwid;
	u64 cpu_release_addr;
	u64 cpu_return_addr;
	const char *enable_method;
	enum cpu_enable_method type;
};

int read_cpu_properties(struct cpu_properties *p, const struct device_node *dn);

#endif /* !defined(__ASSEMBLY__) */

#endif
