/*
 * Copyright (C) Linaro.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cpu-properties.h"

int read_cpu_properties(struct cpu_properties *p, const struct device_node *dn)
{
	const u32 *cell;

	memset(p, 0, sizeof(*p));
	p->hwid = INVALID_HWID;
	p->cpu_release_addr = INVALID_ADDR;
	p->cpu_return_addr = INVALID_ADDR;

	cell = of_get_property(dn, "reg", NULL);

	if (!cell) {
		pr_err("%s: Error: %s: invalid reg property\n",
		       __func__, dn->full_name);
		return -1;
	}

	p->hwid = of_read_number(cell,
		of_n_addr_cells((struct device_node *)dn)) & MPIDR_HWID_BITMASK;

	p->enable_method = of_get_property(dn, "enable-method", NULL);

	if (!p->enable_method) {
		pr_err("%s: Error: %s: invalid enable-method\n",
		       __func__, dn->full_name);
		return -1;
	}

	if (!strcmp(p->enable_method, "psci")) {
		p->type = cpu_enable_method_psci;
		return 0;
	}

	if (strcmp(p->enable_method, "spin-table")) {
		p->type = cpu_enable_method_unknown;
		return -1;
	}

	p->type = cpu_enable_method_spin_table;

	if (of_property_read_u64(dn, "cpu-release-addr",
				 &p->cpu_release_addr)) {
		pr_err("%s: Error: %s: invalid cpu-return-addr property\n",
		       __func__, dn->full_name);
		return -1;
	}

	if (of_property_read_u64(dn, "cpu-return-addr", &p->cpu_return_addr)) {
		pr_err("%s: Error: %s: invalid cpu-return-addr property\n",
			__func__, dn->full_name);
		return -1;
	}

	return 0;
}
